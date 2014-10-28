#include "beacon.h"

int s_beacon_outer_event(zloop_t *loop, zmq_pollitem_t *item, void *arg)
{
    assert(loop);
    assert(item);
    proxy_t *self = (proxy_t *) arg;
    assert (self);
    char *ipaddress = zstr_recv (zbeacon_socket (self->udp));
    if (!ipaddress)
        return 0;
    char *msg = zstr_recv (zbeacon_socket (self->udp));
    if (!msg)
        return 0;
    zstr_sendf (self->global_fe, "%s", ipaddress);
    zstr_sendf (self->global_fe, "%s", msg);
    free (ipaddress);
    free (msg);
    return 0;
}

void beacon_thread (void *args, zctx_t *ctx, void *pipe)
{
    assert(ctx);
    assert(!args);
    int rc = 0;
    char *self;

    proxy_t *beacon_proxy = (proxy_t *) zmalloc (sizeof (proxy_t));
	assert(beacon_proxy);

    // Create new zbeacon
    beacon_proxy->udp = zbeacon_new (ctx, UDP_PORT);
    self = zbeacon_hostname (beacon_proxy->udp);
    beacon_proxy->global_fe = pipe;

    assert (self);

    //send IP
    zstr_sendf(pipe, "%s",self);
    char *endpoint = zstr_recv(pipe);

    zbeacon_set_interval (beacon_proxy->udp, UDP_INTERVAL);

    zbeacon_noecho (beacon_proxy->udp);
    char name[128];
    sprintf(&name[0], "%s@%s", MODBUS_DOMAIN , endpoint);
    free(endpoint);
    log_info("beacon", " beacon name [%s], wait for start", name);
    //wait for 'start' received on pipe
    char* start = zstr_recv(beacon_proxy->global_fe);
    free(start);

    //Pub as ModBus-IP
    zbeacon_publish   (beacon_proxy->udp, (byte *) name, 128);
    //Sub on any domain name
    zbeacon_subscribe (beacon_proxy->udp, NULL, 0);
    log_info("beacon", " beacon set ok");

    //  Poll on beacon
    zmq_pollitem_t poll_beacon = { zbeacon_socket (beacon_proxy->udp), 0, ZMQ_POLLIN, 0};

    zloop_t *beacon_loop = zloop_new ();
    assert (beacon_loop);
    zloop_set_verbose (beacon_loop, DEBUG);

    rc = zloop_poller (beacon_loop, &poll_beacon, s_beacon_outer_event, beacon_proxy);
    assert (rc == 0);

    log_info("beacon", " beacon started");
    //start reactor
    zloop_start (beacon_loop);
    zloop_destroy (&beacon_loop);

    // Stop listening
    zbeacon_unsubscribe (beacon_proxy->udp);
    //  Stop beacon
    zbeacon_silence (beacon_proxy->udp);
    //  Destroy the beacon
    zbeacon_destroy ((zbeacon_t**) &beacon_proxy->udp);

}

int s_beacon_event(zloop_t *loop, zmq_pollitem_t *item, void *arg)
{
    assert(loop);
    assert(item);
    int rc = 0;
    proxy_t *self = (proxy_t *) arg;
    assert (self);
    char *ip = zstr_recv (self->udp);
    if (!ip)
        return 0;
    char *msg = zstr_recv (self->udp);
    if (!msg) 
    {
		free(ip);
        return 0; 
    }
    log_debug("beacon", " beacon [%s:%s]  ", ip, msg);
    fabric_t *fab = fab_new(msg);
    if (fab) {
        if (!zhash_insert(self->fab_list, fab->identity, (void*)fab))
        {
            zhash_freefn(self->fab_list, fab->identity, free_fab);
            log_info( "beacon", " new router found [%s]", fab->identity);
            fab->peer = zsocket_new(self->ctx, ZMQ_DEALER);
            zsocket_set_sndtimeo(fab->peer, 0);
            char *global_identity = zsocket_identity(self->global_fe);
            zsocket_set_identity(fab->peer, global_identity);
            free(global_identity);
            rc = zsocket_connect(fab->peer, "%s",fab->identity);
            if (rc == -1) {
                log_err( "beacon", " can't connect to router [%s] - [%s]", fab->identity, zmq_strerror (errno));
                zsocket_disconnect(fab->peer, "%s",fab->identity);
                zsocket_destroy(self->ctx, fab->peer);
                zhash_delete(self->fab_list, fab->identity);
                fab_destroy(fab);
            } else {
                log_info( "beacon", " connected to router [%s]", fab->identity, zmq_strerror (errno));
                fab->alive = true;
            }
        } else {
            fab_destroy(fab);
        }
    }
    free (ip);
    free (msg);
    return 0;
}
