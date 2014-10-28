#include "common.h"

void    *dealer;
zctx_t  *ctx;

static void finalize() {
	zsocket_destroy(ctx, dealer);
	zctx_destroy (&ctx);		
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

static int introduce()
{
	zmsg_t *msg = zmsg_new();

    msgpack_sbuffer* pIntBuffer = msgpack_sbuffer_new();
    msgpack_packer* pk = msgpack_packer_new( pIntBuffer, msgpack_sbuffer_write );
	char *actionid = getActionid();
	
    if (actionid)
    {

        msgpack_pack_map( pk, 3 );
        {
            m_pack_raw(pk, "command");
            m_pack_raw(pk, "introduce");

            m_pack_raw(pk, "body");
            msgpack_pack_map(pk, 5);
            {
                m_pack_raw(pk, "pid");
                msgpack_pack_fix_uint64(pk, getpid());

                m_pack_raw(pk, "name");
                m_pack_raw(pk, "worker");

                m_pack_raw(pk, "desc");
                m_pack_raw(pk, "worker test module");

                m_pack_raw(pk, "debug");
                msgpack_pack_true(pk);

                m_pack_raw(pk, "provides");
                msgpack_pack_map(pk, 4);
                {
                    m_pack_raw(pk, "echo");
                    msgpack_pack_fix_int32(pk, 1);
                    m_pack_raw(pk, "sleep");
                    msgpack_pack_fix_int32(pk, 1);
                    m_pack_raw(pk, "payload");
                    msgpack_pack_fix_int32(pk, 1);
                    m_pack_raw(pk, "memeat");
                    msgpack_pack_fix_int32(pk, 1);
                }
            }
            m_pack_raw(pk, "actionid");
            m_pack_raw(pk, actionid);
        }
		free(actionid);
    } else {
        return -1;
    }

    zmsg_pushmem(msg, pIntBuffer->data, pIntBuffer->size);

    msgpack_sbuffer_free( pIntBuffer );
    msgpack_packer_free( pk );

	zmsg_send(&msg, dealer);
	zmsg_t *resp = zmsg_recv(dealer);
	if (!resp)
		return -1;	
	
	zframe_t *payload = zmsg_pop(resp);
	msgpack_unpacked object;
    msgpack_unpacked_init(&object);
	
	if (msgpack_unpack_next(&object, (char*)zframe_data(payload), zframe_size(payload) , NULL))
	{
		//msgpack_object_print(stdout, object.data);
		char *command = (char*)m_lookup(object.data, "command");
		if (command)
			zclock_log("command %s", command);
		char *introduce = (char*)m_lookup(object.data, "introduce");
		if (introduce)
			zclock_log("introduce %s", introduce);
		free (command);
		free(introduce);
		msgpack_unpacked_destroy(&object);
		zframe_destroy(&payload);
		zmsg_destroy(&resp);
		return 0;
	}
	msgpack_unpacked_destroy(&object);
	zframe_destroy(&payload);
	zmsg_destroy(&resp);
	
	
    return -1;
}

static zmsg_t *result(const char *actionid, const char *command, const char *data)
{
	zmsg_t *msg = zmsg_new();

    msgpack_sbuffer* pIntBuffer = msgpack_sbuffer_new();
    msgpack_packer* pk = msgpack_packer_new( pIntBuffer, msgpack_sbuffer_write );

    if (data && actionid)
    {
        msgpack_pack_map( pk, 3 );
        {
            m_pack_raw(pk, "command");
            m_pack_raw(pk, "result");

            m_pack_raw(pk, "body");
            msgpack_pack_map( pk, 2 );
            {
                m_pack_raw(pk, "result");
                msgpack_pack_map( pk, 1);
                {
                    m_pack_raw(pk, command);
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

static zmsg_t *echo_command(msgpack_object obj)
{
	char *actionid = (char*)m_lookup(obj, "actionid");
	zmsg_t *msg = result(actionid, "echo", "ok");
	free(actionid);
	return msg;
}

static zmsg_t *sleep_command(msgpack_object obj)
{
	char *sleep_time = m_get_string(&obj,"sleep");
	char *actionid = (char*)m_lookup(obj, "actionid");
	uint64_t time = atoi(sleep_time);
	zclock_sleep(time);
	zmsg_t *msg = result(actionid, "sleep", sleep_time);
	return msg;
}

static void processCall(msgpack_object obj)
{
	char *function = (char*)m_lookup(obj, "function");
	if(function)
	{
			zmsg_t *msg = NULL;
			if (streq(function, "echo"))
			{
					msg = echo_command(obj);
			}
			if (streq(function, "sleep"))
			{
					msg = sleep_command(obj);
			}
			if (streq(function, "payload"))
			{
			}
			if (streq(function, "recursion"))
			{
			}
			if (msg)
			{
				zmsg_send(&msg, dealer);
				//zclock_log("send ok");
			}
			free(function);
	}
}
void memeater_thread (void *args, zctx_t *ctx, void *pipe)
{
	uint64_t used = 0;
	while(zctx_interrupted != 1) {
		uint64_t size = 1024*1024; //one MB
		byte *p = (byte*)malloc(size);
		used += size;
		zclock_sleep(1000);
		assert(p);
		
		long rss = 0L;
		FILE* fp = NULL;
		char file_statm[256];
		int pid = (int)getpid();
		sprintf(file_statm, "/proc/%d/statm", pid);
		if ( (fp = fopen( file_statm, "r" )) == NULL )
			zclock_log("can't open");
		int scan = fscanf( fp, "%ld", &rss );
		fclose( fp );
		if (scan) {
		uint64_t overall = ((size_t)rss * (size_t)sysconf( _SC_PAGESIZE));
		zclock_log("worker use %" PRIu64 " MB, alloc %" PRIu64 "", overall/(1024*1024), used/(1024*1024));
	}
	}
}

static int poll_event(zloop_t *loop, zmq_pollitem_t *item, void *arg) 
{
		zmsg_t *msg = zmsg_recv(item->socket);
		assert(msg);
		
		zframe_t *payload = zmsg_pop(msg);
		zmsg_destroy(&msg);
		msgpack_unpacked object;
		msgpack_unpacked_init(&object);
	
		if (msgpack_unpack_next(&object, (char*)zframe_data(payload), zframe_size(payload) , NULL))
		{
			//msgpack_object_print(stdout, object.data);
			char *command = (char*)m_lookup(object.data, "command");
			if (command) {
				//zclock_log("command: %s", command);
				if (streq(command, "call")) {
				    processCall(object.data);
				}
				if (streq(command, "stop")) {
					msgpack_unpacked_destroy(&object);
					free(command);
				    zclock_log("exiting");
				    zctx_interrupted = 1;
				    finalize();
				    return -1;
				}
				free(command);
			}
		}
		msgpack_unpacked_destroy(&object);
		zframe_destroy(&payload);
		return 0;
}

static void start(const char *command)
{
		
		zmq_pollitem_t poll  = { dealer, 0, ZMQ_POLLIN, 0 };
		zloop_t *loop = zloop_new ();
		assert(loop);
		if (streq(command, "memeater"))
		{
			void *memeater = zthread_fork(ctx, memeater_thread, NULL);
			assert(memeater);
		}
		zloop_poller (loop, &poll, poll_event, NULL);
		
		zloop_start (loop);
        
        zloop_destroy (&loop);
}

int main (int argc, char *argv [])
{
    char *socket = (argc > 1)? argv [1]: "";
    
    if (streq(socket,""))
    {
        zclock_log("cannot start worker for %s", socket);
        return -1;
    }
    if (s_connect(socket)<0) {
        zclock_log("cannot connect to %s", socket);
        finalize();
        return -1;
    }
    else {
        zclock_log("worker connected to %s", socket);
    }
    char *command = (argc > 2)? argv [2]: "";
    if (streq(command, "nostart")) {
		zclock_log("worker sleeping, wait for kill");
		sleep(100);
		finalize();	
		return -1;
	}
	
    if (introduce() == 0)
		start(command);
	finalize();
    exit(0);
}
