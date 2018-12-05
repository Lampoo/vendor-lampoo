#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#include "looper.h"
#include "list.h"

struct looper_ops {
	int (*handle_message)(struct message *msg, void *priv);
	void *priv;
	struct list_entry list;
};

struct looper {
	struct events events;
	int ctrl_fd;
	struct list_entry handlers;
};

static void looper_dispatch(struct looper* looper, struct message *msg, int broadcast)
{
	struct looper_ops *entry, *next;

	list_for_each_entry_safe(entry, next, &looper->handlers, list) {
		int r = entry->handle_message(msg, entry->priv);
		if (broadcast) r = 0;
		if (r == 1) break;
	}
}

static void looper_process(int fd, void *arg)
{
	struct looper *looper = (struct looper *) arg;
	struct message *msg;
	char buffer[sizeof(*msg)*16];
	int n, i;

	n = TEMP_FAILURE_RETRY(read(fd, buffer, sizeof(buffer)));
	if (n <= 0)
		return;

	n /= sizeof(*msg);

	msg = (struct message *) buffer;

	for (i = 0; i < n; i++, msg++) {
		looper_dispatch(looper, msg, 0);

		/* default handler */
		switch (msg->what) {
		case MSG_SIGNAL:
			msg->what = MSG_TERMINATE;
			looper_dispatch(looper, msg, 1);
			events_stop(&looper->events);
			break;
		default:
			break;
		}

		if (msg->flag == MSG_FLAG_LPARAM_ALLOCATED) {
			free((void *)msg->lParam);
		} else if (msg->flag == MSG_FLAG_LPARAM_REFERENCE) {
			struct ref *ref = (struct ref *) msg->lParam;
			ref->deference((void *)ref);
		}
	}
}

struct looper *looper_init(struct looper *looper)
{
	int fds[2], r;

	if (looper == NULL) looper = malloc(sizeof(*looper));

	assert(looper != NULL);

	memset(looper, 0, sizeof(*looper));

	list_init(&looper->handlers);
	events_init(&looper->events);

	r = pipe(fds);
	assert(r == 0);

	fcntl(fds[0], F_SETFD, O_CLOEXEC);
	fcntl(fds[1], F_SETFD, O_CLOEXEC);

	looper->ctrl_fd = fds[1];

	events_watch_fd(&looper->events, fds[0], EVENT_READ,
			looper_process, looper);

	return looper;
}

int looper_register_handler(struct looper *looper,
			int (*handle_message)(struct message*, void *), void *priv)
{
	struct looper_ops *ops = malloc(sizeof(*ops));
	if (!ops) return -ENOMEM;

	list_init(&ops->list);
	ops->priv = priv;
	ops->handle_message = handle_message;

	list_append(&ops->list, &looper->handlers);

	return 0;
}

int looper_loop(struct looper *looper)
{
	events_loop(&looper->events);
	return 0;
}

struct events *looper_getevents(struct looper *looper)
{
	return &looper->events;
}

int looper_send_message(struct looper *looper, int what, int flag, int arg, long larg)
{
	struct message msg;
	msg.what = what;
	msg.flag = flag;
	msg.wParam = arg;
	msg.lParam = larg;
	int r = write(looper->ctrl_fd, &msg, sizeof(msg));
	return (r == sizeof(msg)) ? 0 : -1;
}