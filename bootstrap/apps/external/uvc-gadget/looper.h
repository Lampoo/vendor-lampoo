#ifndef LOOPER_H
#define LOOPER_H

#include "events.h"

/* Message definition */

struct ref {
	void (*deference)(void *obj);
};

struct message {
	int what;
#define MSG_FLAG_LPARAM_ALLOCATED	1
#define MSG_FLAG_LPARAM_REFERENCE	2
	int flag;
	int wParam;
	long lParam;
};

#define MSG_SIGNAL	0
#define MSG_TERMINATE	1

struct looper;

struct looper *looper_init(struct looper *looper);

int looper_register_handler(struct looper *looper,
		int (*handle_message)(struct message *, void *),
		void *priv);

int looper_loop(struct looper *looper);

int looper_cleanup(struct looper *looper);

int looper_send_message(struct looper *looper, int what, int flag, int arg, long larg);

#define looper_send_empty_message(looper, what) \
	looper_send_message(looper, what, 0, 0, 0);

struct events *looper_getevents(struct looper *looper);

#endif
