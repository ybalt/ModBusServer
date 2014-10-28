#ifndef BEACON_H
#define BEACON_H

#include "modbusd.h"

#define UDP_PORT               	 5681
#define UDP_INTERVAL           	 1000
#define MODBUS_DOMAIN           "ModBus"

int s_beacon_outer_event(zloop_t *loop, zmq_pollitem_t *item, void *arg);
void beacon_thread (void *args, zctx_t *ctx, void *pipe);
int s_beacon_event(zloop_t *loop, zmq_pollitem_t *item, void *arg);

#endif //BEACON_H
