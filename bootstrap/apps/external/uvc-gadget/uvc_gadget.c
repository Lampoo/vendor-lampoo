#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/inotify.h>

#include <linux/usb/ch9.h>
#include <linux/usb/g_uvc.h>
#include <linux/usb/video.h>
#include <linux/videodev2.h>

#include "events.h"
#include "tools.h"
#include "v4l2.h"
#include "looper.h"
#include "hotplug.h"
#include "capture.h"
#include "message.h"

#define UVC_INTF_CONTROL	0
#define UVC_INTF_STREAMING	1

#define UVC_CTRL_STOP		0
#define UVC_CTRL_CONNECTED	1
#define UVC_CTRL_DISCONNECTED	2
#define UVC_CTRL_STREAM_ON	3
#define UVC_CTRL_STREAM_OFF	4

struct uvc_gadget
{
	struct looper *looper;

	struct v4l2_device *vdev;

	int is_streaming;

	unsigned int id;

	struct uvc_streaming_control probe;
	struct uvc_streaming_control commit;

	int control;

	unsigned int fcc;
	unsigned int width;
	unsigned int height;

	unsigned int bulk;

	struct list_entry list;

	char imgdata[640 * 480 * 2]; // FIXME: buffer size
};

static struct list_entry g_gadgets;

/* ---------------------------------------------------------------------------
 * Video streaming
 */
static void uvc_video_fill_buffer(struct uvc_gadget *gadget, struct v4l2_video_buffer *buf)
{
	unsigned int size;

	switch (gadget->fcc) {
	case V4L2_PIX_FMT_YUYV:
		size = gadget->width * gadget->height * 2;
		memcpy(buf->mem, gadget->imgdata, size);
		buf->bytesused = size;
		break;

	case V4L2_PIX_FMT_MJPEG:
		//memcpy(buf->mem, dev->imgdata, dev->imgsize);
		//buf->bytesused = dev->imgsize;
		break;
	}
}

static void uvc_video_process(int fd, void *arg)
{
	struct uvc_gadget *gadget = (struct uvc_gadget *) arg;
	struct v4l2_video_buffer buf;
	int ret;

	(void)fd;

	ret = v4l2_dequeue_buffer(gadget->vdev, &buf);
	if (ret < 0)
		return;

	uvc_video_fill_buffer(gadget, &buf);

	v4l2_queue_buffer(gadget->vdev, &buf);
}

static int uvc_video_stream(struct uvc_gadget *gadget, int enable)
{
	struct events *events = looper_getevents(gadget->looper);
	unsigned int i;
	int ret;

	if (!enable) {
		if (!gadget->is_streaming)
			return 0;

		printf("Stopping video stream.\n");
		events_unwatch_fd(events, gadget->vdev->fd, EVENT_WRITE);
		v4l2_stream_off(gadget->vdev);
		v4l2_free_buffers(gadget->vdev);
		gadget->is_streaming = 0;
		looper_send_message(gadget->looper, MSG_VIDEO_STREAM_OFF, 0, (int) gadget->id, 0);
		return 0;
	}

	if (gadget->is_streaming)
		return 0;

	printf("Starting video stream.\n");

	ret = v4l2_alloc_buffers(gadget->vdev, V4L2_MEMORY_MMAP, 4);
	if (ret < 0) {
		printf("Failed to allocate buffers.\n");
		return ret;
	}

	ret = v4l2_mmap_buffers(gadget->vdev);
	if (ret < 0) {
		printf("Failed to mmap buffers.\n");
		goto error;
	}

	for (i = 0; i < gadget->vdev->nbufs; ++i) {
		struct v4l2_video_buffer *buf = &gadget->vdev->buffers[i];

		uvc_video_fill_buffer(gadget, buf);

		ret = v4l2_queue_buffer(gadget->vdev, buf);
		if (ret < 0)
			goto error;
	}

	ret = v4l2_stream_on(gadget->vdev);
	if (ret < 0) {
		printf("Failed to start streaming.\n");
		goto error;
	}

	gadget->is_streaming = 1;

	events_watch_fd(events, gadget->vdev->fd, EVENT_WRITE,
			uvc_video_process, gadget);

	looper_send_message(gadget->looper, MSG_VIDEO_STREAM_ON, 0, (int) gadget->id, 0);

	return 0;

error:
	v4l2_free_buffers(gadget->vdev);
	return ret;
}

static int uvc_video_set_format(struct uvc_gadget *gadget)
{
	struct v4l2_pix_format *fmt = malloc(sizeof(*fmt));
	int ret;

	if (fmt == NULL) return -1;

	printf("Setting format to 0x%08x %ux%u\n",
		gadget->fcc, gadget->width, gadget->height);

	memset(fmt, 0, sizeof *fmt);
	fmt->width = gadget->width;
	fmt->height = gadget->height;
	fmt->pixelformat = gadget->fcc;
	fmt->field = V4L2_FIELD_NONE;
#if 0 // FIXME
	if (gadget->fcc == V4L2_PIX_FMT_MJPEG)
		fmt.sizeimage = gadget->imgsize * 1.5;
#endif
	ret = v4l2_set_format(gadget->vdev, fmt);
	if (ret < 0) {
		free(fmt);
		return ret;
	}

	looper_send_message(gadget->looper, MSG_VIDEO_SET_FMT,
			MSG_FLAG_LPARAM_ALLOCATED, gadget->id, (long) fmt);

	return 0;
}

/* ---------------------------------------------------------------------------
 * Request processing
 */

struct uvc_frame_info
{
	unsigned int width;
	unsigned int height;
	unsigned int intervals[8];
};

struct uvc_format_info
{
	unsigned int fcc;
	const struct uvc_frame_info *frames;
};

static const struct uvc_frame_info uvc_frames_yuyv[] = {
	{ 320, 240, { 333333 }, },
	{ 0, 0, { 0, }, },
};

static const struct uvc_format_info uvc_formats[] = {
	{ V4L2_PIX_FMT_YUYV, uvc_frames_yuyv },
};

static void uvc_fill_streaming_control(struct uvc_gadget *gadget,
		struct uvc_streaming_control *ctrl,
		int iframe, int iformat)
{
	const struct uvc_format_info *format;
	const struct uvc_frame_info *frame;
	unsigned int nframes;

	(void)gadget;

	if (iformat < 0)
		iformat = ARRAY_SIZE(uvc_formats) + iformat;
	if (iformat < 0 || iformat >= (int)ARRAY_SIZE(uvc_formats))
		return;
	format = &uvc_formats[iformat];

	nframes = 0;
	while (format->frames[nframes].width != 0)
		++nframes;

	if (iframe < 0)
		iframe = nframes + iframe;
	if (iframe < 0 || iframe >= (int)nframes)
		return;
	frame = &format->frames[iframe];

	memset(ctrl, 0, sizeof *ctrl);

	ctrl->bmHint = 1;
	ctrl->bFormatIndex = iformat + 1;
	ctrl->bFrameIndex = iframe + 1;
	ctrl->dwFrameInterval = frame->intervals[0];
	switch (format->fcc) {
	case V4L2_PIX_FMT_YUYV:
		ctrl->dwMaxVideoFrameSize = frame->width * frame->height * 2;
		break;
#if 0 //FIXME
	case V4L2_PIX_FMT_MJPEG:
		ctrl->dwMaxVideoFrameSize = gadget->imgsize;
		break;
#endif
	}
	ctrl->dwMaxPayloadTransferSize = 512;	/* TODO this should be filled by the driver. */
	ctrl->bmFramingInfo = 3;
	ctrl->bPreferedVersion = 1;
	ctrl->bMaxVersion = 1;
}

static void uvc_events_process_standard(struct uvc_gadget *gadget, struct usb_ctrlrequest *ctrl,
		struct uvc_request_data *resp)
{
	printf("standard request\n");
	(void)gadget;
	(void)ctrl;
	(void)resp;
}

static void uvc_events_process_control(struct uvc_gadget *gadget, uint8_t req, uint8_t cs,
		struct uvc_request_data *resp)
{
	printf("control request (req %02x cs %02x)\n", req, cs);
	(void)gadget;
	(void)resp;
}

static void uvc_events_process_streaming(struct uvc_gadget *gadget, uint8_t req, uint8_t cs,
		struct uvc_request_data *resp)
{
	struct uvc_streaming_control *ctrl;

	printf("streaming request (req %02x cs %02x)\n", req, cs);

	if (cs != UVC_VS_PROBE_CONTROL && cs != UVC_VS_COMMIT_CONTROL)
		return;

	ctrl = (struct uvc_streaming_control *)&resp->data;
	resp->length = sizeof *ctrl;

	switch (req) {
	case UVC_SET_CUR:
		gadget->control = cs;
		resp->length = 34;
		break;

	case UVC_GET_CUR:
		if (cs == UVC_VS_PROBE_CONTROL)
			memcpy(ctrl, &gadget->probe, sizeof *ctrl);
		else
			memcpy(ctrl, &gadget->commit, sizeof *ctrl);
		break;

	case UVC_GET_MIN:
	case UVC_GET_MAX:
	case UVC_GET_DEF:
		uvc_fill_streaming_control(gadget, ctrl, req == UVC_GET_MAX ? -1 : 0,
				req == UVC_GET_MAX ? -1 : 0);
		break;

	case UVC_GET_RES:
		memset(ctrl, 0, sizeof *ctrl);
		break;

	case UVC_GET_LEN:
		resp->data[0] = 0x00;
		resp->data[1] = 0x22;
		resp->length = 2;
		break;

	case UVC_GET_INFO:
		resp->data[0] = 0x03;
		resp->length = 1;
		break;
	}
}

static void uvc_events_process_class(struct uvc_gadget *gadget, struct usb_ctrlrequest *ctrl,
		struct uvc_request_data *resp)
{
	if ((ctrl->bRequestType & USB_RECIP_MASK) != USB_RECIP_INTERFACE)
		return;

	switch (ctrl->wIndex & 0xff) {
	case UVC_INTF_CONTROL:
		uvc_events_process_control(gadget, ctrl->bRequest, ctrl->wValue >> 8, resp);
		break;

	case UVC_INTF_STREAMING:
		uvc_events_process_streaming(gadget, ctrl->bRequest, ctrl->wValue >> 8, resp);
		break;

	default:
		break;
	}
}

static void uvc_events_process_setup(struct uvc_gadget *gadget, struct usb_ctrlrequest *ctrl,
		struct uvc_request_data *resp)
{
	gadget->control = 0;

	printf("bRequestType %02x bRequest %02x wValue %04x wIndex %04x "
			"wLength %04x\n", ctrl->bRequestType, ctrl->bRequest,
			ctrl->wValue, ctrl->wIndex, ctrl->wLength);

	switch (ctrl->bRequestType & USB_TYPE_MASK) {
	case USB_TYPE_STANDARD:
		uvc_events_process_standard(gadget, ctrl, resp);
		break;

	case USB_TYPE_CLASS:
		uvc_events_process_class(gadget, ctrl, resp);
		break;

	default:
		break;
	}
}

static void uvc_events_process_data(struct uvc_gadget *gadget, struct uvc_request_data *data)
{
	struct uvc_streaming_control *target;
	struct uvc_streaming_control *ctrl;
	const struct uvc_format_info *format;
	const struct uvc_frame_info *frame;
	const unsigned int *interval;
	unsigned int iformat, iframe;
	unsigned int nframes;

	switch (gadget->control) {
	case UVC_VS_PROBE_CONTROL:
		printf("setting probe control, length = %d\n", data->length);
		target = &gadget->probe;
		break;

	case UVC_VS_COMMIT_CONTROL:
		printf("setting commit control, length = %d\n", data->length);
		target = &gadget->commit;
		break;

	default:
		printf("setting unknown control, length = %d\n", data->length);
		return;
	}

	ctrl = (struct uvc_streaming_control *)&data->data;
	iformat = clamp((unsigned int)ctrl->bFormatIndex, 1U,
			(unsigned int)ARRAY_SIZE(uvc_formats));
	format = &uvc_formats[iformat-1];

	nframes = 0;
	while (format->frames[nframes].width != 0)
		++nframes;

	iframe = clamp((unsigned int)ctrl->bFrameIndex, 1U, nframes);
	frame = &format->frames[iframe-1];
	interval = frame->intervals;

	while (interval[0] < ctrl->dwFrameInterval && interval[1])
		++interval;

	target->bFormatIndex = iformat;
	target->bFrameIndex = iframe;
	switch (format->fcc) {
	case V4L2_PIX_FMT_YUYV:
		target->dwMaxVideoFrameSize = frame->width * frame->height * 2;
		break;
#if 0 // FIXME
	case V4L2_PIX_FMT_MJPEG:
		if (gadget->imgsize == 0)
			printf("WARNING: MJPEG requested and no image loaded.\n");
		target->dwMaxVideoFrameSize = gadget->imgsize;
		break;
#endif
	}
	target->dwFrameInterval = *interval;

	if (gadget->control == UVC_VS_COMMIT_CONTROL) {
		gadget->fcc = format->fcc;
		gadget->width = frame->width;
		gadget->height = frame->height;

		uvc_video_set_format(gadget);
		if (gadget->bulk)
			uvc_video_stream(gadget, 1);
	}
}

static void uvc_gadget_process(int fd, void *arg)
{
	struct uvc_gadget *gadget = (struct uvc_gadget *) arg;
	struct v4l2_event v4l2_event;
	struct uvc_event *uvc_event = (void *)&v4l2_event.u.data;
	struct uvc_request_data resp;
	int ret;

	(void)fd;

	ret = ioctl(gadget->vdev->fd, VIDIOC_DQEVENT, &v4l2_event);
	if (ret < 0) {
		printf("VIDIOC_DQEVENT failed: %s (%d)\n", strerror(errno),
				errno);
		return;
	}

	memset(&resp, 0, sizeof resp);
	resp.length = -EL2HLT;

	switch (v4l2_event.type) {
	case UVC_EVENT_CONNECT:
	case UVC_EVENT_DISCONNECT:
		return;

	case UVC_EVENT_SETUP:
		uvc_events_process_setup(gadget, &uvc_event->req, &resp);
		break;

	case UVC_EVENT_DATA:
		uvc_events_process_data(gadget, &uvc_event->data);
		return;

	case UVC_EVENT_STREAMON:
		uvc_video_stream(gadget, 1);
		return;

	case UVC_EVENT_STREAMOFF:
		uvc_video_stream(gadget, 0);
		return;
	}

	ioctl(gadget->vdev->fd, UVCIOC_SEND_RESPONSE, &resp);
	if (ret < 0) {
		printf("UVCIOC_S_EVENT failed: %s (%d)\n", strerror(errno), errno);
	}
}

static unsigned long current()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

static int uvc_gadget_init(struct uvc_gadget *gadget)
{
	struct v4l2_event_subscription sub;

	uvc_fill_streaming_control(gadget, &gadget->probe, 0, 0);
	uvc_fill_streaming_control(gadget, &gadget->commit, 0, 0);

	if (gadget->bulk) {
		/* FIXME Crude hack, must be negotiated with the driver. */
		gadget->probe.dwMaxPayloadTransferSize = 16 * 1024;
		gadget->commit.dwMaxPayloadTransferSize = 16 * 1024;
	}

	memset(&sub, 0, sizeof sub);
	sub.type = UVC_EVENT_SETUP;
	if (ioctl(gadget->vdev->fd, VIDIOC_SUBSCRIBE_EVENT, &sub) == -1) return -1;
	sub.type = UVC_EVENT_DATA;
	if (ioctl(gadget->vdev->fd, VIDIOC_SUBSCRIBE_EVENT, &sub) == -1) return -1;
	sub.type = UVC_EVENT_STREAMON;
	if (ioctl(gadget->vdev->fd, VIDIOC_SUBSCRIBE_EVENT, &sub) == -1) return -1;
	sub.type = UVC_EVENT_STREAMOFF;
	if (ioctl(gadget->vdev->fd, VIDIOC_SUBSCRIBE_EVENT, &sub) == -1) return -1;

	return 0;
}

static int uvc_gadget_attach(struct looper *looper, const char *devname)
{
	struct v4l2_device *dev = v4l2_open(devname);

	if (dev == NULL) return -1;

	if (dev->type != V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		v4l2_close(dev);
		return -1;
	}

	struct uvc_gadget *gadget = malloc(sizeof(*gadget));
	if (gadget == NULL) {
		v4l2_close(dev);
		return -1;
	}

	memset(gadget, 0, sizeof(*gadget));
	gadget->id = current();
	gadget->looper = looper;
	gadget->vdev = dev;
	list_init(&gadget->list);
	if (uvc_gadget_init(gadget) < 0) {
		v4l2_close(dev);
		free(gadget);
		return -1;
	}

	struct events *events = looper_getevents(looper);

	events_watch_fd(events, gadget->vdev->fd,
			EVENT_EXCEPTION, uvc_gadget_process, gadget);

	list_append(&gadget->list, &g_gadgets);

	looper_send_message(gadget->looper, MSG_UVC_ATTACHED, 0, (int) gadget->id, 0);

	return 0;
}

static int uvc_gadget_detach(struct looper *looper, const char *devname)
{
	struct uvc_gadget *gadget, *next;

	list_for_each_entry_safe(gadget, next, &g_gadgets, list) {
		if (strcmp(gadget->vdev->name, devname) == 0) {
			break;
		}
	}

	if (gadget != NULL) {
		int id = gadget->id;

		list_remove(&gadget->list);

		struct events *events = looper_getevents(looper);

		uvc_video_stream(gadget, 0); // stop streaming and free resource

		events_unwatch_fd(events, gadget->vdev->fd, EVENT_EXCEPTION);

		v4l2_close(gadget->vdev);

		free(gadget);

		looper_send_message(gadget->looper, MSG_UVC_DETACHED, 0, id, 0);

		return 0;
	}

	return -1;
}

static int uvc_gadget_fill_buffer(struct looper *looper, unsigned int id, void *obj)
{
	struct uvc_gadget *gadget, *next;

	(void)looper;

	list_for_each_entry_safe(gadget, next, &g_gadgets, list) {
		if (gadget->id == id) break;
	}

	if (gadget != NULL && gadget->is_streaming) {
		struct v4l2_video_buffer *buf = capture_obtain_buffer(obj);
		memcpy(gadget->imgdata, buf->mem, buf->bytesused);
		return 0;
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
		if (uvc_gadget_attach(looper, devname) == 0)
			return 1;
		break;
	case MSG_HOTPLUG_DETACHED:
		devname = (const char *) msg->lParam;
		if (uvc_gadget_detach(looper, devname) == 0)
			return 1;
		break;
	case MSG_VIDEO_FRAME_AVAILABLE:
		if (uvc_gadget_fill_buffer(looper, (unsigned int) msg->wParam, (void *) msg->lParam) == 0)
            return 1;
		break;
	}

	return 0;
}

int uvc_init(struct looper* looper)
{
	list_init(&g_gadgets);
	looper_register_handler(looper, handle_message, looper);
	return 0;
}
