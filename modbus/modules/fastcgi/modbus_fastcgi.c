#include "modbus_fastcgi.h"

volatile static int socketId;

static zctx_t	*ctx;
static void		*s_zmq;
static zhash_t	*rq_list;

#define http_400 "Status: 400 Bad Request\r\n\r\n"
#define http_404 "Status: 404 Not Found\r\n\r\n"

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
                m_pack_raw(pk, APP_NAME);

                m_pack_raw(pk, "desc");
                m_pack_raw(pk, APP_DESC);

                m_pack_raw(pk, "debug");
                msgpack_pack_true(pk);

                m_pack_raw(pk, "provides");
                msgpack_pack_map(pk, 1);
                {
                    m_pack_raw(pk, "stop");
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

	zmsg_send(&msg, s_zmq);
	zmsg_t *resp = zmsg_recv(s_zmq);
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

int upstream_event(zloop_t *loop, zmq_pollitem_t *item, void *arg)
{ 
	int rc;
	
	zclock_log("have a data on socketid %d", socketId);
    FCGX_Request *request = (FCGX_Request*)malloc(sizeof(FCGX_Request)); 
	assert(request);
    if(FCGX_InitRequest(request, socketId, 0) != 0) 
    { 
        zclock_log("Can't init request"); 
        return 0;
    } 
	
	rc = FCGX_Accept_r(request);
	
	if (rc >= 0) {
		
		zclock_log("incoming request %p", request); 
	}
	else {
		zclock_log("error accepting upstream");
		return 0;
	}
	
	char *request_uri_ptr = FCGX_GetParam("REQUEST_URI", request->envp);
	if (!request_uri_ptr)
	{
		zclock_log("no REQUEST_URI in %p", request);
		FCGX_Finish_r(request);
		return 0;		
	}
	char *request_uri = strdup(request_uri_ptr);
	char *command = strtok(request_uri+1, "/");
	char *module = strtok(NULL, "/");
	char *function = strtok(NULL, "/");
	char *version = strtok(NULL, "/");
	char *actionid = getActionid();
	if (!command || !module || !function || !version) {
		zclock_log("request [%s] [%s] [%s] [%s] malformed", 
					command, module, function, version);
		FCGX_PutS(http_400, request->out);
		FCGX_Finish_r(request); 
		free(request_uri);
		return 0;
	}
	zclock_log("request %s %s %s %s processing", command, module, function, version);
	
	zmsg_t *msg = zmsg_new();

    msgpack_sbuffer* pIntBuffer = msgpack_sbuffer_new();
    msgpack_packer* pk = msgpack_packer_new( pIntBuffer, msgpack_sbuffer_write );

	int par_count = 0;
	char **p;
	for (p = request->envp; *p; ++p) 
	{
		par_count++;
	}
   
	msgpack_pack_map( pk, 3 );
	{
		m_pack_raw(pk, "command");
		m_pack_raw(pk, command);

		m_pack_raw(pk, "body");
		msgpack_pack_map( pk, 5 );
		{
			m_pack_raw(pk, "module");
			m_pack_raw(pk, module);
			
			m_pack_raw(pk, "function");
			m_pack_raw(pk, function);
			
			m_pack_raw(pk, "version");
			msgpack_pack_uint64(pk, atoi(version));
			
			m_pack_raw(pk, "parameters");
			msgpack_pack_map(pk, par_count);
			{	
				for (p=request->envp; *p; p++) 
				{
					char param[256];
					char value[8192];
					char *ptr;
		
					ptr = strchr(*p, '=');
					unsigned int pos = (unsigned int)(ptr - *p);
					if (pos!=0) 
					{
						memcpy(&param, *p, pos<256?pos:256);
						param[pos] = '\0';
						
						unsigned int val_pos = strlen(*p)-pos;
						memcpy(&value, *p+pos+1, val_pos<8192?val_pos:8192);
						value[val_pos] = '\0';
					
						//zclock_log("param %s value %s", param, value);
					
						m_pack_raw(pk, param);
						m_pack_raw(pk, value);
					} else {
						m_pack_raw(pk, "");
						m_pack_raw(pk, "");
					}
				}
			}	
			m_pack_raw(pk, "seqno");
			msgpack_pack_uint64(pk, 0);
		}
		m_pack_raw(pk, "actionid");
		m_pack_raw(pk, actionid);
	}
	
    zmsg_pushmem(msg, pIntBuffer->data, pIntBuffer->size);

    msgpack_sbuffer_free( pIntBuffer );
    msgpack_packer_free( pk );
    free(request_uri);
    
    rc = zmsg_send(&msg, s_zmq);
	if (rc == 0) 
	{
		zclock_log("message %s sent", actionid);
		zhash_insert(rq_list, actionid, request);
	} else {
		zclock_log("cannot sent message %s", actionid);
	}
	free(actionid);
    return 0; 
} 

int zmq_event(zloop_t *loop, zmq_pollitem_t *item, void *arg)
{
	zclock_log("message received");
	zmsg_t *msg = zmsg_recv(item->socket);
	zframe_t *payload = zmsg_pop(msg);
	msgpack_unpacked object;
	msgpack_unpacked_init(&object);
	
	if (msgpack_unpack_next(&object, (char*)zframe_data(payload), zframe_size(payload) , NULL))
	{
		//msgpack_object_print(stdout, object.data);
		//zclock_log("\n");
		char *actionid = (char*)m_lookup(object.data, "actionid");
		if (actionid) 
		{
			zclock_log("received message from fabric actionid: %s", actionid);
			FCGX_Request *request = (FCGX_Request*)zhash_lookup(rq_list, actionid);
			if (request)
			{
				char *command = (char*)m_lookup(object.data, "command");
				if (command)
				{
					if (streq(command,"result")) 
					{
						msgpack_object *result = (msgpack_object*)m_lookup(object.data, "result");
						if (result) 
						{
							if (result->type == MSGPACK_OBJECT_ARRAY &&
								result->via.array.size == 3)  
							{
								int code = 500;
								msgpack_object* p = result->via.array.ptr;
								char status[64];
								if (p->type == MSGPACK_OBJECT_POSITIVE_INTEGER)
								{
									code = p->via.u64;
									char *status_string;
									switch (code) {
										case (200): {status_string="OK";break;}
										case (400): {status_string="Bad Request";break;}
										case (404): {status_string="Not Found";break;}
										default: {status_string="Internal server error";break;}
									}
										sprintf(status, "Status: %d %s\r\n", code, status_string);
										FCGX_PutStr(status, strlen(status), request->out);
								} else {
									zclock_log("unknown status code for %s", actionid);
									sprintf(status, "Status: %d %s\r\n", 503, "Internal server error");
									FCGX_PutStr(status, strlen(status), request->out);
								}
									
								//response headers
								p++;
								if (p->type == MSGPACK_OBJECT_ARRAY &&
									p->via.array.size != 0)
								{
									msgpack_object* o = p->via.array.ptr;
									msgpack_object* const oend = p->via.array.ptr + p->via.array.size;
									for(; o < oend; ++o) 
									{
										FCGX_PutStr(o->via.raw.ptr, o->via.raw.size, request->out);
										FCGX_PutStr(":", 1, request->out);
										++o;
										FCGX_PutStr(o->via.raw.ptr, o->via.raw.size, request->out);
										FCGX_PutStr("\r\n", 2, request->out);
									}
								}
								FCGX_PutS("\r\n", request->out); //end headers
								//payload
								p++;
								if (p->type == MSGPACK_OBJECT_ARRAY &&
									p->via.array.size != 0)
								{
									msgpack_object* o = p->via.array.ptr;
									msgpack_object* const oend = p->via.array.ptr + p->via.array.size;
									for(; o < oend; ++o) 
									{
										FCGX_PutStr(o->via.raw.ptr, o->via.raw.size, request->out);
									}
								}
								FCGX_PutStr("\r\n", 2, request->out);
							} else {
								zclock_log("malformed result for %s, array size %d", actionid, result->via.array.size);
							}
						} else {
							zclock_log("no result for %s", actionid);
						}
					}
					if (streq(command,"exception"))
					{
						char status[64];
						sprintf(status, "Status: %d %s\r\n", 500, "Internal server error");
						FCGX_PutStr(status, strlen(status), request->out);
						FCGX_PutS("\r\n", request->out); //end headers
						char *message = (char*)m_lookup(object.data, "string");
						FCGX_PutS(message?message:"", request->out);
						FCGX_PutStr("\r\n", 2, request->out);
						free(message);
					}
					free(command);
				} else {
					zclock_log("no command for %s", actionid);
					char status[64];
					sprintf(status, "Status: %d %s\r\n", 500, "Internal server error");
					FCGX_PutStr(status, strlen(status), request->out);
					FCGX_PutS("\r\n", request->out); //end headers
					FCGX_PutStr("\r\n", 2, request->out);
				}
				FCGX_Finish_r(request);
				zhash_delete(rq_list, actionid);
				free(request);
			} else  {
				zclock_log("no request found for actionid %s", actionid);
			}
			free(actionid);
		} else {
			zclock_log("no actionid");
		}
		
	} else {
		zclock_log("error unpack payload");
	}
	msgpack_unpacked_destroy(&object);
	zframe_destroy(&payload);
	zmsg_destroy(&msg);
	return 0;
}

int fcgi_init(char *socket)
{
	FCGX_Init(); 
	zclock_log("Open upstream socket %s", socket); 
    socketId = FCGX_OpenSocket(socket, 10);
    if(socketId < 0) 
    { 
		zclock_log("Upstream socket unable to open\n"); 
        return -1; 
    } 
    zclock_log("Upstream socket opened\n");
    return 0; 
}

int main (int argc, char *argv []) 
{ 
	int rc;
	
	if (argc != 3)
	{
		zclock_log("wrong arguments, should be modbus_fastcgi <fabric_socket> <upstream_socket>");
		return -1;
	}
	ctx = zctx_new();
	assert(ctx);
	zloop_t *loop = zloop_new();
	assert(loop);
	s_zmq = zsocket_new(ctx, ZMQ_DEALER);
	assert(s_zmq);
	rc = zsocket_connect(s_zmq, "%s", argv[1]);
	if (rc == -1)
	{
		zclock_log("wrong fabric socket %s", argv[1]);
		return -1;
	}
	rc = fcgi_init(argv[2]);
	if (rc == -1)
	{
		zclock_log("unable open upstream socket %s", argv[2]);
		return -1;
	}
	zmq_pollitem_t poll_zmq  = { s_zmq, 0, ZMQ_POLLIN, 0 };
	
	zmq_pollitem_t poll_upstream  = { 0, socketId, ZMQ_POLLIN, 0 };
	
	rc = zloop_poller ( loop, &poll_zmq, zmq_event, NULL);
	
	rc = zloop_poller ( loop, &poll_upstream, upstream_event, NULL);
	zclock_log("Going to introduce on %s", argv[1]);
	if (introduce())
		return -1;
	rq_list = zhash_new();
	zloop_start(loop);
	zloop_destroy(&loop);
	zctx_destroy(&ctx);
	
    return 0; 
}
