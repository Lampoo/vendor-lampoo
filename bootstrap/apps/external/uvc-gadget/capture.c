#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "v4l2.h"
#include "looper.h"
#include "events.h"
#include "list.h"
#include "message.h"

#ifndef ARRAY_SIZE
# define ARRAY_SIZE(x)	((sizeof(x)) / (sizeof(x[0])))
#endif

#define INTERNAL_BUFFER_NUMBER	2

struct v4l_capture {
	struct looper *looper;

	struct v4l2_device *vdev;

	int is_streaming;

	unsigned long gadget_id;

	struct list_entry list;

	unsigned int fcc;
	unsigned int width;
	unsigned int height;

	unsigned index;
	unsigned char *buffers[INTERNAL_BUFFER_NUMBER];
};

static struct list_entry g_captures;

static int v4l_process_buffer(struct v4l_capture *capture, struct v4l2_video_buffer *buf)
{
	unsigned char *y = (unsigned char *) buf->mem;
	int index = (capture->index % INTERNAL_BUFFER_NUMBER);
	unsigned char *yuyv = capture->buffers[index];
	int i;
	int size = capture->height * capture->width * 2;

	capture->index++;

	// Convert GreyScale to YUYV (YUV422)
	switch (capture->vdev->format.pixelformat) {
	case V4L2_PIX_FMT_GREY:
		for (i = 0; i < size; i += 2) {
			yuyv[i] = *y++;
		}
		break;
	case V4L2_PIX_FMT_YUYV:
		memcpy(yuyv, y, size);
		break;
	default:
		return -ENOSYS;
	}

	looper_send_message(capture->looper, MSG_VIDEO_FRAME_AVAILABLE,
				0, capture->gadget_id, (long) capture->buffers[index]);

	return 0;
}

static void capture_process(int fd, void *arg)
{
	struct v4l_capture *capture = (struct v4l_capture *) arg;
	struct v4l2_video_buffer buf;
	int ret;

	(void)fd;

	ret = v4l2_dequeue_buffer(capture->vdev, &buf);
	if (ret < 0) {
		printf("Failed to dequeue buffer.\n");
		return;
	}

	v4l_process_buffer(capture, &buf);

	v4l2_queue_buffer(capture->vdev, &buf);
}

static unsigned v4l_video_supported_format[] = {
	V4L2_PIX_FMT_YUYV,
	V4L2_PIX_FMT_GREY,
};

static int v4l_video_try_set_format(struct v4l_capture *capture, struct v4l2_pix_format *fmt)
{
	struct v4l2_pix_format format;
	unsigned i;

	// TODO: add more supported format
	if (fmt->pixelformat != V4L2_PIX_FMT_YUYV)
		return -1;

	memset(&format, 0, sizeof(format));
	format.width = fmt->width;
	format.height = fmt->height;
	format.field = V4L2_FIELD_ANY;

	for (i = 0; i < ARRAY_SIZE(v4l_video_supported_format); i++) {
		format.pixelformat = v4l_video_supported_format[i];
		if (v4l2_set_format(capture->vdev, &format) == 0) {
			capture->fcc = fmt->pixelformat;
			capture->width = fmt->width;
			capture->height = fmt->height;
			return 0;
		}
	}

	return -1;
}

static int v4l_video_stream(struct v4l_capture *capture, int on)
{
	struct events *events = looper_getevents(capture->looper);
	int ret, size;
	unsigned i;
	unsigned char *buffer;

	if (!on) {
		if (!capture->is_streaming) return 0;
		printf("V4L: Stop streaming.\n");
		events_unwatch_fd(events, capture->vdev->fd, EVENT_READ);
		v4l2_stream_off(capture->vdev);
		v4l2_free_buffers(capture->vdev);
		free(capture->buffers[0]);
		for (i = 0; i < INTERNAL_BUFFER_NUMBER; i++)
			capture->buffers[i] = NULL;
		capture->index = 0;
		capture->is_streaming = 0;
	} else {
		if (capture->is_streaming) return 0;

		printf("V4L: Start streaming.\n");

		size = capture->width * capture->height * 2; // YUYV
		buffer = (unsigned char *) malloc(size * INTERNAL_BUFFER_NUMBER);
		if (!buffer) {
			printf("Failed to allocate memory.\n");
			return -ENOMEM;
		}

		ret = v4l2_alloc_buffers(capture->vdev, V4L2_MEMORY_MMAP, 4);
		if (ret < 0) {
			printf("Failed to allocate buffers.\n");
			free(buffer);
			return ret;
		}

		ret = v4l2_mmap_buffers(capture->vdev);
		if (ret < 0) {
			printf("Failed to mmap buffers.\n");
			free(buffer);
			v4l2_free_buffers(capture->vdev);
			return ret;
		}

		for (i = 0; i < capture->vdev->nbufs; i++) {
			struct v4l2_video_buffer *buf = &capture->vdev->buffers[i];

			ret = v4l2_queue_buffer(capture->vdev, buf);
			if (ret < 0) {
				printf("Failed to queue buffer.\n");
				free(buffer);
				v4l2_free_buffers(capture->vdev);
				return ret;
			}
		}

		memset(buffer, 128, size * INTERNAL_BUFFER_NUMBER);

		for (i = 0; i < INTERNAL_BUFFER_NUMBER; i++) {
			capture->buffers[i] = buffer;
			buffer += size;
		}

		capture->index = 0;

		v4l2_stream_on(capture->vdev);

		events_watch_fd(events, capture->vdev->fd, EVENT_READ, capture_process, capture);

		capture->is_streaming = 1;
	}

	return 0;
}

static int v4l_capture_attach(struct looper *looper, const char *devname)
{
	struct v4l2_device *dev = v4l2_open(devname);

	if (dev == NULL) return -1;

	if (dev->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		v4l2_close(dev);
		return -1;
	}

	struct v4l_capture *capture = malloc(sizeof(*capture));
	if (capture == NULL) {
		v4l2_close(dev);
		return -1;
	}

	memset(capture, 0, sizeof(*capture));
	capture->looper = looper;
	capture->vdev = dev;
	list_init(&capture->list);

	list_append(&capture->list, &g_captures);

	return 0;
}

static int v4l_capture_detach(struct looper *looper, const char *devname)
{
	struct v4l_capture *capture, *next;

	list_for_each_entry_safe(capture, next, &g_captures, list) {
		if (strcmp(capture->vdev->name, devname) == 0) {
			list_remove(&capture->list);

			struct events *events = looper_getevents(looper);

			v4l_video_stream(capture, 0); // stop streaming and free resource

			v4l2_close(capture->vdev);

			free(capture);

			return 0;
		}
	}

	return -1;
}

static int v4l_capture_terminate()
{
	struct v4l_capture *capture, *next;

	list_for_each_entry_safe(capture, next, &g_captures, list) {
		list_remove(&capture->list);

		v4l_video_stream(capture, 0); // stop streaming and free resource

		v4l2_close(capture->vdev);

		free(capture);
	}

	return 0;
}

static int v4l_capture_set_fmt(struct looper *looper, unsigned int id, void *param)
{
	struct v4l_capture *capture, *next;

	(void)looper;

	list_for_each_entry_safe(capture, next, &g_captures, list) {
		if (capture->gadget_id == id) {
			if (v4l_video_try_set_format(capture, (struct v4l2_pix_format *) param) == 0) {
				return 0;
			} else {
				capture->gadget_id = 0;
				break;
			}
		}
	}

	list_for_each_entry_safe(capture, next, &g_captures, list) {
		if (capture->gadget_id == 0) {
			if (v4l_video_try_set_format(capture, (struct v4l2_pix_format *) param) < 0)
				continue;

			capture->gadget_id = id;
			return 0;
		}
	}

	return -1;
}

static int v4l_capture_stream(struct looper *looper, unsigned int id, int on)
{
	struct v4l_capture *capture, *next;

	(void)looper;

	list_for_each_entry_safe(capture, next, &g_captures, list) {
		if (capture->gadget_id == id) {
			if (v4l_video_stream(capture, on) == 0)
				return 0;
			break;
		}
	}

	return -1;
}

static int v4l_capture_detach_gadget(struct looper *looper, unsigned int id)
{
	struct v4l_capture *capture, *next;

	(void)looper;

	list_for_each_entry_safe(capture, next, &g_captures, list) {
		if (capture->gadget_id == id) {
			v4l_video_stream(capture, 0);
			capture->gadget_id = 0;
			return 0;
		}
	}

	return -1;
}

static int handle_message(struct message *msg, void *priv)
{
	struct looper *looper = (struct looper *) priv;
	const char *devname;

	switch (msg->what) {
	case MSG_HOTPLUG_ATTACHED:
		devname = (const char *) msg->lParam;
		if (v4l_capture_attach(looper, devname) == 0)
			return 1;
		break;
	case MSG_HOTPLUG_DETACHED:
		devname = (const char *) msg->lParam;
		if (v4l_capture_detach(looper, devname) == 0)
			return 1;
		break;
	case MSG_VIDEO_SET_FMT:
		v4l_capture_set_fmt(looper, (unsigned int) msg->wParam, (void *) msg->lParam);
		break;
	case MSG_VIDEO_STREAM_ON:
		v4l_capture_stream(looper, (unsigned int) msg->wParam, 1);
		break;
	case MSG_VIDEO_STREAM_OFF:
		v4l_capture_stream(looper, (unsigned int) msg->wParam, 0);
		break;
	case MSG_UVC_DETACHED:
		v4l_capture_detach_gadget(looper, (unsigned int) msg->wParam);
		break;
	case MSG_TERMINATE:
		v4l_capture_terminate();
		break;
	}

	return 0;
}

int capture_init(struct looper *looper)
{
	list_init(&g_captures);

	looper_register_handler(looper, handle_message, looper);

	return 0;
}
