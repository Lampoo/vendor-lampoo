#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/inotify.h>

#include "looper.h"
#include "hotplug.h"
#include "message.h"

static void hotplug_attach(struct looper *looper, const char* devname)
{
	const char *_devname = strdup(devname);
	if (!_devname) return;
	looper_send_message(looper, MSG_HOTPLUG_ATTACHED, MSG_FLAG_LPARAM_ALLOCATED, 0, (long) _devname);
}

static void hotplug_detach(struct looper *looper, const char* devname)
{
	const char *_devname = strdup(devname);
	if (!_devname) return;
	looper_send_message(looper, MSG_HOTPLUG_DETACHED, MSG_FLAG_LPARAM_ALLOCATED, 0, (long) _devname);
}

static void hotplug_process(int fd, void *arg)
{
	struct looper *looper = (struct looper *) arg;
	char buffer[512];
	const struct inotify_event *event;

	int r = read(fd, buffer, sizeof(buffer));

	if (r <= 0) return;

	/* loop over all events */
	char *ptr;

	for (ptr = buffer; ptr < buffer + r; ptr += sizeof(*event) + event->len) {
		event = (const struct inotify_event *) ptr;

		if (event->mask & IN_ISDIR) continue;

		if (!strncmp(event->name, "video", 5)) {
			if (event->mask & IN_CREATE) {
				// on video device attached
				hotplug_attach(looper, event->name);
			} else if (event->mask & IN_DELETE) {
				// on video device detached
				hotplug_detach(looper, event->name);
			}
		}
	}
}

int hotplug_init(struct looper *looper)
{
	int fd = inotify_init();

	if (fd < 0) return fd;

	fcntl(fd, F_SETFD, O_CLOEXEC);
	fcntl(fd, F_SETFL, O_NONBLOCK);

	inotify_add_watch(fd, "/dev", IN_CREATE | IN_DELETE);

	struct events *events = looper_getevents(looper);

	events_watch_fd(events, fd, EVENT_READ, hotplug_process, looper);

	return 0;
}

int hotplug_scan(struct looper *looper)
{
	DIR *d = opendir("/dev");

	if (d == NULL)
		return -1;

	struct dirent *ent;

	while ((ent = readdir(d)) != NULL) {
		if (ent->d_name[0] == '.') continue;
		if (ent->d_name[0] != 'v') continue;
		if (strncmp(ent->d_name, "video", 5) != 0) continue;
		hotplug_attach(looper, ent->d_name);
	}

	closedir(d);

	return 0;
}
