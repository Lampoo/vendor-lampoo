#ifndef CAPTURE_H
#define CAPTURE_H

struct looper;

int capture_init(struct looper* looper);

struct v4l2_video_buffer *capture_obtain_buffer(void *obj);

#endif
