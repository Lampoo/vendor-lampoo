#ifndef HOTPLUG_H
#define HOTPLUG_H

struct looper;

int hotplug_init(struct looper *looper);
int hotplug_scan(struct looper *looper);

#endif
