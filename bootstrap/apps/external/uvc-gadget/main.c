
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/inotify.h>

#include "looper.h"
#include "hotplug.h"
#include "capture.h"
#include "uvc_gadget.h"

static struct looper *g_looper;

static void signal_handler(int signal)
{
	/* Stop the main loop when the user presses CTRL-C */
	looper_send_message(g_looper, MSG_SIGNAL, 0, signal, 0);
}

static void signal_init(struct looper *looper)
{
	(void)looper;
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
}

int main()
{
	g_looper = looper_init(NULL);

	signal_init(g_looper);

	hotplug_init(g_looper);

	uvc_init(g_looper);

	capture_init(g_looper);

	hotplug_scan(g_looper);

	looper_loop(g_looper);

	return 0;
}
