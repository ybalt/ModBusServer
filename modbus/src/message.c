#include "message.h"


uint64_t getTimeStamp() 
{
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return tv.tv_sec*(uint64_t)1000000+tv.tv_usec;
}

uint64_t getInTimeDelta(message_t *msg) 
{

    if (!msg)
        return 0;

    struct timeval tv;
    gettimeofday(&tv,NULL);
    uint64_t now = tv.tv_sec*(uint64_t)1000000+tv.tv_usec;

    return now - (msg->in_timestamp);
}

uint64_t getOutTimeDelta(message_t *msg) 
{

    if (!msg)
        return 0;

    struct timeval tv;
    gettimeofday(&tv,NULL);
    uint64_t now = tv.tv_sec*(uint64_t)1000000+tv.tv_usec;

    return now - (msg->out_timestamp);
}

uint64_t getProcessTimeDelta(message_t *msg)
{
	 if (!msg)
        return 0;

    struct timeval tv;
    gettimeofday(&tv,NULL);
    uint64_t now = tv.tv_sec*(uint64_t)1000000+tv.tv_usec;

    return now - (msg->process_timestamp);
}


message_t* message_fromzmq(zmsg_t *msg, bool local)
{
    if (!msg)
        return NULL;

    message_t* message = message_new();
    if (!message)
        return NULL;

    zframe_t *payload_frame, *identity_frame;
	
    if (zmsg_size(msg) == 2) 
    {
        identity_frame = zmsg_pop(msg);
        if (!identity_frame)
        {
            log_err("message","malformed message received");
            message_destroy(message);
            return NULL;
        }
        message->local = local;
        if (message->local) 
        {
            message->identity = zframe_strhex(identity_frame);
        } else {
			size_t size = zframe_size (identity_frame);
			message->identity = (char *)zmalloc(size + 1);
			memcpy(message->identity,zframe_data(identity_frame),size);
			message->identity[size] = 0;
        }
        zframe_destroy(&identity_frame);
    } else {
        log_err("message","wrong size message received");
        return NULL;
    }
    payload_frame = zmsg_pop(msg);
    if (!payload_frame || zframe_size(payload_frame)==0)
    {
        message_destroy(message);
        zframe_destroy(&payload_frame);
        return NULL;
    }
    message->payload = zframe_dup(payload_frame);
    zframe_destroy(&payload_frame);
    if (msgpack_unpack_next(&message->object, (char*)zframe_data(message->payload), zframe_size(message->payload) , NULL))
    {

        if (message->object.data.type == MSGPACK_OBJECT_MAP)
        {
            message->command = message_check_command(message);
            if (!message->command)
            {
                message_destroy(message);
                log_err("message","no command in message");
                return NULL;
            }
            message->actionid = message_check_actionid(message);
            if (!message->actionid)
            {
                message_destroy(message);
                log_err("message","no actionid in message");
                return NULL;
            }
            return message;
        } else {
            log_err("message","wrong message structure (not map)");
            message_destroy(message);
            return NULL;
        }
    }
    log_err("message","failed to unpack message from MSGPACK structure");
    message_destroy(message);
    return NULL;
}


message_t* message_new()
{
    message_t* message = (message_t*) zmalloc(sizeof(message_t));
    if (!message)
        return NULL;

    message->actionid = NULL;
    message->command = NULL;
    message->identity = NULL;

    message->in_timestamp = getTimeStamp();
    message->out_timestamp = 0;
    message->process_timestamp = 0;
    
    message->routed_via = NULL;
	message->iface = NULL;
	message->globals = NULL;
	
    msgpack_unpacked_init(&message->object);;

    return message;
}

void message_destroy(message_t *msg_p)
{
    if (msg_p) {
        msgpack_unpacked_destroy(&msg_p->object);
        zframe_destroy(&msg_p->payload);
        if (msg_p->actionid)
            free (msg_p->actionid);
        if (msg_p->command)
            free (msg_p->command);
        if (msg_p->identity)
            free (msg_p->identity);
        if (msg_p->module)
            free (msg_p->module);
        if (msg_p->function)
            free (msg_p->function);
		if (msg_p->globals)
			zlist_destroy(&msg_p->globals);
		
        msg_p->actionid = NULL;
        msg_p->command = NULL;
        msg_p->identity = NULL;
        msg_p->module = NULL;
		msg_p->function = NULL;
		msg_p->routed_via = NULL;
		msg_p->iface = NULL;
		msg_p->globals = NULL;
				
        free(msg_p);
    }
}


zmsg_t *message_tomsg(const message_t* message)
{
    zmsg_t *msg = zmsg_new();

    if (!message)
    {
        log_err("message","cannot pack empty message");
        return msg;
    }

    msgpack_sbuffer* pIntBuffer = msgpack_sbuffer_new();
    msgpack_packer* pk = msgpack_packer_new( pIntBuffer, msgpack_sbuffer_write );

    msgpack_pack_object(pk, message->object.data);

    zmsg_pushmem(msg, pIntBuffer->data, pIntBuffer->size);

    msgpack_sbuffer_free( pIntBuffer );
    msgpack_packer_free( pk );
    return msg;
}


zmsg_t* message_introduce(const char *actionid)
{
    zmsg_t *msg = zmsg_new();

    msgpack_sbuffer* pIntBuffer = msgpack_sbuffer_new();
    msgpack_packer* pk = msgpack_packer_new( pIntBuffer, msgpack_sbuffer_write );

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
                m_pack_raw(pk, "modrouter");

                m_pack_raw(pk, "desc");
                m_pack_raw(pk, "modrouter module");

                m_pack_raw(pk, "debug");
                msgpack_pack_true(pk);

                m_pack_raw(pk, "provides");
                msgpack_pack_map(pk, 1);
                {
                    m_pack_raw(pk, "router");
                    msgpack_pack_double(pk, 0.1);
                }
            }

            m_pack_raw(pk, "actionid");
            m_pack_raw(pk, actionid);
        }

    } else {
        return NULL;
    }

    zmsg_pushmem(msg, pIntBuffer->data, pIntBuffer->size);

    msgpack_sbuffer_free( pIntBuffer );
    msgpack_packer_free( pk );

    return msg;
}

zmsg_t* message_response(const char *data, const char *actionid)
{
    zmsg_t *msg = zmsg_new();

    msgpack_sbuffer* pIntBuffer = msgpack_sbuffer_new();
    msgpack_packer* pk = msgpack_packer_new( pIntBuffer, msgpack_sbuffer_write );

    if (actionid && data)
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
                    m_pack_raw(pk, "introduce");
                    m_pack_raw(pk, data);
                }
                m_pack_raw(pk, "seqno");
                msgpack_pack_uint64(pk, 0);
            }
            m_pack_raw(pk, "actionid");
            m_pack_raw(pk, actionid);
        }
    }
    else {
        return NULL;
    }

    zmsg_pushmem(msg, pIntBuffer->data, pIntBuffer->size);

    msgpack_sbuffer_free( pIntBuffer );
    msgpack_packer_free( pk );

    return msg;
}


zmsg_t* message_ifacelist_response(zlist_t *ifaces, const char *actionid)
{
    char 	*iface_path;
    zlist_t *list = zlist_new();
    zlist_autofree(list);
    
	iface_t *iface = (iface_t*)zlist_first(ifaces);
	while(iface)
	{
		if (zlist_size(iface->id_local_list)>0) 
		//only iface with local modules will be populated
		{
			log_debug("message: packing interface [%s:%s:%d]",
							iface->mod, iface->func, iface->ver);
			char iface_path[256];
			sprintf(iface_path, "%s:%s:%ld", iface->mod, iface->func, iface->ver);
			zlist_append(list, iface_path);
		}
		iface = (iface_t*)zlist_next(ifaces);
	}
	log_debug("message","message: going to pack ifaces size %d\n", zlist_size(list));
    
    zmsg_t *msg = zmsg_new();

    msgpack_sbuffer* pIntBuffer = msgpack_sbuffer_new();
    msgpack_packer* pk = msgpack_packer_new( pIntBuffer, msgpack_sbuffer_write );

    if (actionid && ifaces)
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
                    m_pack_raw(pk, "IfaceList");
                    {
                        msgpack_pack_array(pk, zlist_size(list));
                        iface_path = (char*)zlist_first(list);
						while(iface_path)
						{
							m_pack_raw(pk, iface_path);
							iface_path = (char*)zlist_next(list);
						}
                    }
                }
                m_pack_raw(pk, "seqno");
                msgpack_pack_uint64(pk, 0);
            }
            m_pack_raw(pk, "actionid");
            m_pack_raw(pk, actionid);
        }
    } else {
        return NULL;
    }

    zmsg_pushmem(msg, pIntBuffer->data, pIntBuffer->size);

    msgpack_sbuffer_free( pIntBuffer );
    msgpack_packer_free( pk );
    zlist_destroy(&list);

    return msg;
}

zmsg_t* message_ifacelist_request(const char *actionid)
{
    zmsg_t *msg = zmsg_new();

    msgpack_sbuffer* pIntBuffer = msgpack_sbuffer_new();
    msgpack_packer* pk = msgpack_packer_new( pIntBuffer, msgpack_sbuffer_write );

    if (actionid)
    {
        msgpack_pack_map( pk, 3 );
        {
            m_pack_raw(pk, "command");
            m_pack_raw(pk, "request");

            m_pack_raw(pk, "body");
            msgpack_pack_map( pk, 1 );
            {
                m_pack_raw(pk, "command");
                m_pack_raw(pk, "IfaceList");
            }
            m_pack_raw(pk, "actionid");
            m_pack_raw(pk, actionid);
        }
    }
    else {
        return NULL;
    }

    zmsg_pushmem(msg, pIntBuffer->data, pIntBuffer->size);

    msgpack_sbuffer_free( pIntBuffer );
    msgpack_packer_free( pk );

    return msg;
}

zmsg_t* message_stop(const char *actionid)
{
    zmsg_t *msg = zmsg_new();

    msgpack_sbuffer* pIntBuffer = msgpack_sbuffer_new();
    msgpack_packer* pk = msgpack_packer_new( pIntBuffer, msgpack_sbuffer_write );

    if (actionid)
    {
        msgpack_pack_map( pk, 3 );
        {
            m_pack_raw(pk, "command");
            m_pack_raw(pk, "stop");

            m_pack_raw(pk, "body");
            msgpack_pack_map( pk, 1 );
            {
                m_pack_raw(pk, "time");
                m_pack_raw(pk, "0");
            }
            m_pack_raw(pk, "actionid");
            m_pack_raw(pk, actionid);
        }
    }
    else {
        return NULL;
    }

    zmsg_pushmem(msg, pIntBuffer->data, pIntBuffer->size);

    msgpack_sbuffer_free( pIntBuffer );
    msgpack_packer_free( pk );

    return msg;
}

zmsg_t* message_exception(const char *actionid, const char *ex_msg, int ex_num)
{
    zmsg_t *msg = zmsg_new();

    msgpack_sbuffer* pIntBuffer = msgpack_sbuffer_new();
    msgpack_packer* pk = msgpack_packer_new( pIntBuffer, msgpack_sbuffer_write );

    if (actionid)
    {
        msgpack_pack_map( pk, 3 );
        {
            m_pack_raw(pk, "command");
            m_pack_raw(pk, "exception");

            m_pack_raw(pk, "body");
            msgpack_pack_map( pk, 2 );
            {
                m_pack_raw(pk, "string");
                m_pack_raw(pk, ex_msg);

                m_pack_raw(pk, "exception");
                msgpack_pack_fix_int32(pk, ex_num);
            }
            m_pack_raw(pk, "actionid");
            m_pack_raw(pk, actionid);
        }
    }
    else {
        return NULL;
    }

    zmsg_pushmem(msg, pIntBuffer->data, pIntBuffer->size);

    msgpack_sbuffer_free( pIntBuffer );
    msgpack_packer_free( pk );

    return msg;
}


char *message_check_command(message_t *message)
{
    if (!message)
        return NULL;
    char *value = m_get_string(&message->object.data, "command");
    if (!value)
    {
        log_err("message","no command in message");
        return NULL;
    }
    if (streq(value, "introduce") |
            streq(value, "ping") |
            streq(value, "pong") |
            streq(value, "stop") |
            streq(value, "call") |
            streq(value, "request") |
            streq(value, "exception") |
            streq(value, "result") )
    {
        return value;
    } else {
        log_err("unknown command %s", value);
    }
    return NULL;
}

char *message_check_actionid(message_t *message)
{
    if (!message)
        return NULL;
    char *value = (char*)m_get_string(&message->object.data, "actionid");
    if (!value)
    {
        log_err("message","no actionid in message");
        return NULL;
    }
    if (strlen(value)<5)
    {
        /*TODO make some actionid checks*/
        log_err("message","malformed actionid");
        return NULL;
    } else
        return value;

}

zlist_t *message_getIfaceList(message_t *message)
{
    if (!message)
        return NULL;

    msgpack_object *body = NULL;
    msgpack_object *result = NULL;
    msgpack_object *modlist = NULL;

    body = m_find_map(message->object.data, "body");
    if (body) {
        result = m_find_map(*body, "result");
        if (result)
            modlist = m_find_map(*result, "IfaceList");
    }
    if (!modlist)
        return NULL;

    zlist_t *ifaces = zlist_new();
    zlist_autofree(ifaces);
    m_unpack_array(ifaces, *modlist);

    return ifaces;
}


char *message_getReqCommand(message_t *message)
{
    char *value = NULL;

    if (!message)
        return value;

    msgpack_object *body = NULL;

    body = m_find_map(message->object.data, "body");
    if (!body)
        return value;

    value = m_get_string(body, "command");

    return value;
}

char *message_getFunc(message_t *message)
{
    char *value = NULL;

    if (!message)
        return value;

    msgpack_object *body = NULL;

    body = m_find_map(message->object.data, "body");
    if (!body)
        return value;

    value = m_get_string(body, "function");
    if (!value)
        log_err("message","no function found");

    return value;

}

char *message_getModule(message_t *message)
{
    char *value = NULL;
   
    if (!message)
        return value;

    msgpack_object *body = NULL;

    body = m_find_map(message->object.data, "body");
    if (!body)
        return value;

    value = m_get_string(body, "module");
    if (!value)
        log_err("message","no module command found");

    return value;

}

char *message_getExceptionString(message_t *message)
{
    char *value = NULL;

    if (!message)
        return value;

    msgpack_object *body = NULL;

    body = m_find_map(message->object.data, "body");
    if (!body)
        return value;

    value = m_get_string(body, "string");

    return value;
}

int message_getExceptionCode(message_t *message)
{
    int value = 0;

    if (!message)
        return value;

    msgpack_object *body = NULL;

    body = m_find_map(message->object.data, "body");
    if (!body)
        return value;

    value = m_get_int(body, "exception");

    return value;
}

char *message_getMessage(message_t *message)
{
    char *value = NULL;

    if (!message)
        return value;

    msgpack_object *body = NULL;

    body = m_find_map(message->object.data, "body");
    if (!body)
        return value;

    value = m_get_string(body, "message");

    return value;
}

pid_t message_getPid(message_t *message)
{
    if (!message)
        return 0;

    msgpack_object *body = NULL;

    body = m_find_map(message->object.data, "body");
    if (!body)
        return 0;

    pid_t value = (pid_t)m_get_int(body, "pid");

    return value;
}

char *message_getName(message_t *message)
{
    char *value = NULL;

    if (!message)
        return value;

    msgpack_object *body = NULL;

    body = m_find_map(message->object.data, "body");
    if (!body)
        return value;

    value = m_get_string(body, "name");

    return value;
}

char *message_getDesc(message_t *message)
{
    char *value = NULL;

    if (!message)
        return value;

    msgpack_object *body = NULL;

    body = m_find_map(message->object.data, "body");
    if (!body)
        return value;

    value = m_get_string(body, "desc");

    return value;
}

zhash_t *message_getProvides(message_t *message)
{
    zhash_t *hash=NULL;
    
    if (!message)
		return NULL;

    msgpack_object *body = NULL;
    msgpack_object *provides = NULL;

    body = m_find_map(message->object.data, "body");
    if (body) {
        provides = m_find_map(*body, "provides");
        if (provides) {
            hash = zhash_new();
            m_unpack_map(hash, *provides);
        }
    }
    return hash;
}

uint64_t message_getIfVersion(message_t *message)
{
    uint64_t value = 0;

    if (!message)
        return value;

    msgpack_object *body = NULL;

    body = m_find_map(message->object.data, "body");
    if (!body)
        return value;
	
    value = m_get_int(body, "version");

    return (uint64_t)value;

}

void message_dump(message_t *message) 
{
	if (message) {
		msgpack_object_print(stdout, message->object.data);
        fprintf(stdout, "\n");
    } else {
		fprintf(stdout, "wrong message or no msgpack struct in message\n");
	}
}

void free_message(void *data)
{
    if (data) {
        message_t *message = (message_t*)data;
        message_destroy(message);
    }
    data = NULL;
}
