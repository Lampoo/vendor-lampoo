#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "v4l2.h"
#include "looper.h"
#include "events.h"
#include "list.h"
#include "message.h"

struct v4l_capture {
	struct looper *looper;

	struct v4l2_device *vdev;

	unsigned long gadget_id;

	struct list_entry list;
};

struct v4l_buffer {
	struct ref ref;

	struct v4l_capture *capture;

	struct v4l2_video_buffer buf;
};

static struct list_entry g_captures;

static void v4l_buffer_deference(void *obj)
{
	struct v4l_buffer *buffer = (struct v4l_buffer *) obj;
	int ret;

	ret = v4l2_queue_buffer(buffer->capture->vdev, &buffer->buf);
	if (ret < 0) {
		printf("Failed to queue buffer.\n");
	}

	free(obj);
}

struct v4l2_video_buffer *capture_obtain_buffer(void *obj)
{
	struct v4l_buffer *buffer = (struct v4l_buffer *) obj;
	return &buffer->buf;
}

static void capture_process(int fd, void *arg)
{
	struct v4l_capture *capture = (struct v4l_capture *) arg;
#if 1
	struct v4l_buffer *buffer = malloc(sizeof(*buffer));
	int ret;

	(void)fd;

	if (!buffer) {
		printf("Out of memory.\n");
		return;
	}

	memset(buffer, 0, sizeof(*buffer));
	buffer->ref.deference = v4l_buffer_deference;
	buffer->capture = capture;

	ret = v4l2_dequeue_buffer(capture->vdev, &buffer->buf);
	if (ret < 0) {
		printf("Failed to dequeue buffer.\n");
		free(buffer);
		return;
	}

	looper_send_message(capture->looper, MSG_VIDEO_FRAME_AVAILABLE,
				MSG_FLAG_LPARAM_REFERENCE, capture->gadget_id, (long) buffer);
#else
	struct v4l2_video_buffer buf;
	int ret;

	(void) fd;

	ret = v4l2_dequeue_buffer(capture->vdev, &buf);
	if (ret < 0) {
		printf("Failed to dequeue buffer.\n");
		return;
	}

	v4l2_queue_buffer(capture->vdev, &buf);
#endif
}

static int v4l_video_set_fmt(struct v4l_capture *capture, struct v4l2_pix_format *fmt)
{
	return v4l2_set_format(capture->vdev, fmt);
}

static int v4l_video_stream(struct v4l_capture *capture, int on)
{
	struct events *events = looper_getevents(capture->looper);
	int ret;
	unsigned i;

	if (!on) {
		printf("V4L: Stop streaming.\n");
		events_unwatch_fd(events, capture->vdev->fd, EVENT_READ);
		v4l2_stream_off(capture->vdev);
		v4l2_free_buffers(capture->vdev);
	} else {
		printf("V4L: Start streaming.\n");

		ret = v4l2_alloc_buffers(capture->vdev, V4L2_MEMORY_MMAP, 4);
		if (ret < 0) {
			printf("Failed to allocate buffers.\n");
			return ret;
		}

		ret = v4l2_mmap_buffers(capture->vdev);
		if (ret < 0) {
			printf("Failed to mmap buffers.\n");
			v4l2_free_buffers(capture->vdev);
			return ret;
		}

		for (i = 0; i < capture->vdev->nbufs; i++) {
			struct v4l2_video_buffer *buf = &capture->vdev->buffers[i];

			ret = v4l2_queue_buffer(capture->vdev, buf);
			if (ret < 0) {
				printf("Failed to queue buffer.\n");
				v4l2_free_buffers(capture->vdev);
				return ret;
			}
		}

		v4l2_stream_on(capture->vdev);

		events_watch_fd(events, capture->vdev->fd, EVENT_READ, capture_process, capture);
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

static int v4l_capture_set_fmt(struct looper *looper, unsigned int id, void *param)
{
	struct v4l_capture *capture, *next;

	(void)looper;

	list_for_each_entry_safe(capture, next, &g_captures, list) {
		if (capture->gadget_id == id) {
			v4l_video_set_fmt(capture, (struct v4l2_pix_format *) param);
			return 0;
		}
	}

	list_for_each_entry_safe(capture, next, &g_captures, list) {
		if (capture->gadget_id == 0) {
			capture->gadget_id = id;
			v4l_video_set_fmt(capture, (struct v4l2_pix_format *) param);
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
	}

	return 0;
}

int capture_init(struct looper *looper)
{
	list_init(&g_captures);

	looper_register_handler(looper, handle_message, looper);

	return 0;
}
