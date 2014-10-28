#include "common.h"

#define COUNT 1000

void    *dealer;
zctx_t  *ctx;

long int counter = 0;
long int counter_prev;
long int failed = 0;
long int success = 0;
uint64_t start;

volatile int interrupt = 0;

static void s_signal_handler (int signal_value)
{
    zclock_log("handler got [%s]", strsignal(signal_value));
    interrupt=1;
}

static int s_connect(char *arg)
{
		ctx = zctx_new();
		if (!ctx) {
			zclock_log("error creating zmq context - %s", zmq_strerror (errno));
			return -1;
		}
		dealer = zsocket_new(ctx, ZMQ_DEALER);
		if (!dealer) {
			zclock_log("error creating zmq socket - %s", zmq_strerror (errno));
			return -1;
		}
		
		if (zsocket_connect(dealer, "%s", arg) != 0) {
				zclock_log("error connecting socket %s - %s",arg , zmq_strerror (errno));
			return -1;
		}
		return 0;
}

static zmsg_t *create_call(const char *actionid, const char *module, const char *function, const char *data)
{
	zmsg_t *msg = zmsg_new();

    msgpack_sbuffer* pIntBuffer = msgpack_sbuffer_new();
    msgpack_packer* pk = msgpack_packer_new( pIntBuffer, msgpack_sbuffer_write );

    if (data && actionid && module)
    {
        msgpack_pack_map( pk, 3 );
        {
            m_pack_raw(pk, "command");
            m_pack_raw(pk, "call");

            m_pack_raw(pk, "body");
            msgpack_pack_map( pk, 5 );
            {
                m_pack_raw(pk, "module");
                m_pack_raw(pk, module);
                
                m_pack_raw(pk, "function");
                m_pack_raw(pk, function);
                
                m_pack_raw(pk, "version");
                msgpack_pack_uint64(pk, 1);
                
                m_pack_raw(pk, "parameters");
                msgpack_pack_map( pk, 1 );
                {
					m_pack_raw(pk, function);
					m_pack_raw(pk, data);
				}
                m_pack_raw(pk, "seqno");
                msgpack_pack_uint64(pk, 0);
            }
            m_pack_raw(pk, "actionid");
            m_pack_raw(pk, actionid);
        }
    }  else {
        return NULL;
    }
	
    zmsg_pushmem(msg, pIntBuffer->data, pIntBuffer->size);

    msgpack_sbuffer_free( pIntBuffer );
    msgpack_packer_free( pk );

    return msg;
}

static int event(zloop_t *loop, zmq_pollitem_t *item, void *arg)
{
	if (interrupt)
		return -1;
	zmsg_t *msg = zmsg_recv(dealer);
	zframe_t *payload = zmsg_pop(msg);
	zmsg_destroy(&msg);
	msgpack_unpacked object;
	msgpack_unpacked_init(&object);
	
	if (msgpack_unpack_next(&object, (char*)zframe_data(payload), zframe_size(payload) , NULL))
	{
		//zclock_log("message");
		//msgpack_object_print(stdout, object.data);
		char *command = (char*)m_lookup(object.data, "command");
		if (command) {
			//zclock_log("command: %s", command);
			if (streq(command, "exception")) {
			    failed++;
			}
			if (streq(command, "result")) {
				success++;
			}
			free(command);
		}
	}
	msgpack_unpacked_destroy(&object);
	zframe_destroy(&payload);
	return 0;
}

static int info_loop(zloop_t *loop, int item, void *arg)
{
	struct timeval tv;
	gettimeofday(&tv,NULL);
	uint64_t end = (uint64_t)tv.tv_sec;
	if ((end - start) > 1) 
	{
		zclock_log("counter %ld, failed %ld, success %ld", counter, failed, success);
		counter = 0;
		failed = 0;
		success = 0;
		start = end;
	}
	return 0;
}

static int sleep_loop(zloop_t *loop, int item, void *arg)
{
	
	int *c = (int*)arg;
	int i;

	for (i = 0; i < *c; i++) 
	{
		if (interrupt)
			return -1;
		
		char *actionid = getActionid();
		zmsg_t *msg = create_call(actionid, "worker", "sleep", "echo");
		free(actionid);
		if (msg) {
			zmsg_send(&msg, dealer);
			counter ++;
		}
	}

	return 0;
}

void start_echo(char *arg)
{
	struct timeval tv;
	gettimeofday(&tv,NULL);
	start = (uint64_t)tv.tv_sec;
	//while(!interrupt) 
	//{
		char *actionid = getActionid();
		zmsg_t *msg = create_call(actionid, "worker", "sleep", arg);
		if (msg) 
		{
			zmsg_send(&msg, dealer);
			counter ++;
			zmsg_destroy(&msg);
			zmsg_t *msg = zmsg_recv(dealer);
			zframe_t *payload = zmsg_pop(msg);
			zmsg_destroy(&msg);
			msgpack_unpacked object;
			msgpack_unpacked_init(&object);
		
			if (msgpack_unpack_next(&object, (char*)zframe_data(payload), zframe_size(payload) , NULL))
			{
				//zclock_log("message");
				//msgpack_object_print(stdout, object.data);
				char *command = (char*)m_lookup(object.data, "command");
				if (command) {
					//zclock_log("command: %s", command);
					if (streq(command, "exception")) {
						failed++;
						zclock_log("exception");
					}
					if (streq(command, "result")) {
						success++;
						zclock_log("result ok");
					}
					free(command);
				}
			}
			msgpack_unpacked_destroy(&object);
			zframe_destroy(&payload);
		}
		
		gettimeofday(&tv,NULL);
		uint64_t end = (uint64_t)tv.tv_sec;
		if ((end - start) == 1) 
		{
			float speed = counter/(end-start);
			zclock_log("speed %f m/s, failed %ld, success %ld", speed, failed, success);
			counter = 0;
			failed = 0;
			success = 0;
			start = end;
		} 
	//} //end while
}

void start_loop(int count)
{
	zloop_t *loop = zloop_new();
	assert(loop);
	
	zmq_pollitem_t poll_socket = { dealer,  0, ZMQ_POLLIN, 0 };
	zloop_poller ( loop, &poll_socket, event, NULL);
	
	zloop_timer(loop, 1000, 0, sleep_loop, &count);
	zloop_timer(loop, 1000, 0, info_loop, &count);
	
	struct timeval tv;
	gettimeofday(&tv,NULL);
	start = (uint64_t)tv.tv_sec;
	
	zloop_start (loop);
	return;
}

int main (int argc, char *argv [])
{
    char *sock = (argc > 1)? argv [1]: "";
    if (streq(sock,""))
    {
        zclock_log("cannot start broker for %s", sock);
        return -1;
    }
    char *job = (argc > 2)? argv [2]: "";
    if (streq(job,"")) 
    {
		zclock_log("cannot start broker job %s", job);
		return -1;
	}
    
    if (s_connect(sock)<0) {
        zclock_log("cannot connect to %s", sock);
        return -1;
    }
    else {
        zclock_log("broker connected to %s", sock);
    }
    
    
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags=0;
    sa.sa_handler = s_signal_handler;
    sigaction(SIGHUP, &sa, 0);
    sigaction(SIGINT, &sa, 0);
    sigaction(SIGQUIT, &sa, 0);
    sigaction(SIGABRT, &sa, 0);
    sigaction(SIGTERM, &sa, 0);
	if (streq(job,"loop")) 
	{
		char *arg = (argc > 3)? argv [3]: "1";
		int count = atoi(arg);
		start_loop(count);
	}
	if (streq(job,"sleep"))
	{
		char *arg = (argc > 3)? argv [3]: "1000";
		start_echo(arg);
	}
    exit(0);
}
