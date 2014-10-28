#include "modbus_helper.h"

char* getActionid()
{
       
    zuuid_t *uuid = zuuid_new ();
    char *aid = (char*)zmalloc(strlen(zuuid_str(uuid))+1);
    strcpy(aid, zuuid_str(uuid)); 
    zuuid_destroy(&uuid);
    return aid;

}

void *m_lookup(msgpack_object o, const char *key)
{
    //zclock_log("m_lookup, key %s", key);
    if (o.type == MSGPACK_OBJECT_MAP)
    {
        msgpack_object_kv* p = o.via.map.ptr;
        msgpack_object_kv* const pend = o.via.map.ptr + o.via.map.size;
        for(; p < pend; ++p) {
    		    if (p->val.type == MSGPACK_OBJECT_RAW)
				{
					 if (memcmp(p->key.via.raw.ptr, key, p->key.via.raw.size) == 0) 
					 {
						char* val = (char*)zmalloc(sizeof(char)*(p->val.via.raw.size)+1);
						memcpy(val, p->val.via.raw.ptr, p->val.via.raw.size);
						val[p->val.via.raw.size]='\0';
						return val;
					 }
				}
				if (p->val.type == MSGPACK_OBJECT_POSITIVE_INTEGER)
				{
					 if (memcmp(p->key.via.raw.ptr, key, p->key.via.raw.size) == 0) 
					 {
						return &p->val.via.u64;
					 }
				}
	            if (p->val.type == MSGPACK_OBJECT_NEGATIVE_INTEGER)
	            {
	                if (memcmp(p->key.via.raw.ptr, key, p->key.via.raw.size) == 0) 
	                {
						return &p->val.via.i64;
					}
	            }
	            if (p->val.type == MSGPACK_OBJECT_MAP)
	            {
					if (memcmp(p->key.via.raw.ptr, key, p->key.via.raw.size) == 0) 
					{
						return &p->val;
					} else {
						void *data;
						data = m_lookup(p->val, key);
						if (data)
							return data;
					}
	            }
	            if (p->val.type == MSGPACK_OBJECT_ARRAY)
	            {
	                if (memcmp(p->key.via.raw.ptr, key, p->key.via.raw.size) == 0) 
	                {
						return &p->val;
					}
	            }
			}
    } else {
 
    }
    return NULL;
}


static void free_raw(void *data)
{
	if (data)
		free(data);
}

void m_unpack_map(zhash_t *ptr, msgpack_object o)
{

    if (o.type == MSGPACK_OBJECT_MAP)
    {
        msgpack_object_kv* p = o.via.map.ptr;
        msgpack_object_kv* const pend = o.via.map.ptr + o.via.map.size;
        for(; p < pend; ++p) {
            char* key = (char*)zmalloc(sizeof(char)*(p->key.via.raw.size)+1);
            memcpy(key, p->key.via.raw.ptr, p->key.via.raw.size);
            key[p->key.via.raw.size]='\0';
            if (p->val.type == MSGPACK_OBJECT_RAW)
            {
                char* val = (char*)zmalloc(sizeof(char)*(p->val.via.raw.size)+1);
                memcpy(val, p->val.via.raw.ptr, p->val.via.raw.size);
                val[p->val.via.raw.size]='\0';
                zhash_insert(ptr, key, val);
                zhash_freefn(ptr, key, free_raw);
                //log_debug("msgpack:%s=>%s", key, (char*)zhash_lookup(ptr, key));
            }

            if (p->val.type == MSGPACK_OBJECT_POSITIVE_INTEGER)
            {
                uint64_t *num = (uint64_t*)zmalloc(sizeof(uint64_t));
                *num = p->val.via.u64;
                zhash_insert(ptr, key, num);
                zhash_freefn(ptr, key, free_raw);
                //log_debug( "msgpack:(pi)%s=>%lld", key, *(uint64_t*)zhash_lookup(ptr, key));
            }
            if (p->val.type == MSGPACK_OBJECT_NEGATIVE_INTEGER)
            {
                int64_t *num = (int64_t*)zmalloc(sizeof(int64_t));
                *num = p->val.via.i64;
                zhash_insert(ptr, key, num);
                zhash_freefn(ptr, key, free_raw);
                //log_debug( "msgpack:(ni)%s=>%s", key, (char*)zhash_lookup(ptr, key));
            }
            if (p->val.type == MSGPACK_OBJECT_MAP)
            {
                zhash_t *new_obj = zhash_new();
                zhash_insert(ptr, key, (void*)new_obj);
                //log_debug( "msgpack:%s=>map", key);
                m_unpack_map(new_obj, p->val);
            }
            if (p->val.type == MSGPACK_OBJECT_ARRAY)
            {
                zlist_t *new_obj = zlist_new();
                zhash_insert(ptr, key, (void*)new_obj);
                m_unpack_array(new_obj, p->val);
                //log_debug( "msgpack:%s=>array", key);
            }
            free(key);
        }
    }
}

void m_unpack_array(zlist_t *ptr, msgpack_object o)
{
    if (o.type == MSGPACK_OBJECT_ARRAY)
    {
        if(o.via.array.size != 0) {
            msgpack_object* p = o.via.array.ptr;
            msgpack_object* const pend = o.via.array.ptr + o.via.array.size;
            for(; p < pend; ++p) {
                if (p->type == MSGPACK_OBJECT_RAW)
                {
                    char* val = (char*)zmalloc(sizeof(char)*(p->via.raw.size)+1);
                    memcpy(val, p->via.raw.ptr, p->via.raw.size);
                    val[p->via.raw.size]='\0';
                    zlist_append(ptr, val);
                    zlist_freefn(ptr, val, free_raw, 1);
                }

                if (p->type == MSGPACK_OBJECT_POSITIVE_INTEGER)
                {
                    uint64_t *num = (uint64_t*)zmalloc(sizeof(uint64_t));
                    *num = p->via.u64;
                    zlist_append(ptr, num);
                    zlist_freefn(ptr, num, free_raw, 1);
                    //log_debug( "msgpack:(pi)%d", *num);
                }
                if (p->type == MSGPACK_OBJECT_NEGATIVE_INTEGER)
                {
                    int64_t *num = (int64_t*)malloc(sizeof(int64_t));
                    *num = p->via.i64;
                    zlist_append(ptr, num);
                    zlist_freefn(ptr, num, free_raw, 1);
                    //log_debug( "msgpack:(ni)%d", *num);
                }
                if (p->type == MSGPACK_OBJECT_MAP)
                {
                    //zhash_t *new_obj = zhash_new();
                    //zlist_append(ptr, (void*)new_obj);
                    //log_debug( "msgpack:map");
                    //m_unpack_map(new_obj, *p);
                }
                if (p->type == MSGPACK_OBJECT_ARRAY)
                {
                    //zlist_t *new_obj = zlist_new();
                    //zlist_append(ptr, (void*)new_obj);
                    //m_unpack_array(new_obj, *p);
                    //log_debug( "msgpack:array");
                }
            }
        }
    }
}

void m_pack_raw(msgpack_packer *pk, const char *name)
{
    if (!pk) return;
    if (!name) return;

    msgpack_pack_raw( pk, strlen(name) );
    msgpack_pack_raw_body( pk, name, strlen(name) );
}

msgpack_object *m_find_map(msgpack_object o, const char *key)
{
    if (o.type == MSGPACK_OBJECT_MAP)
    {
        msgpack_object_kv* p = o.via.map.ptr;
        msgpack_object_kv* const pend = o.via.map.ptr + o.via.map.size;
        for(; p < pend; ++p) {
            if (memcmp(p->key.via.raw.ptr, key, p->key.via.raw.size) == 0)
            {    
                return &p->val;
            }
        }
    }

    return NULL;
}

char *m_get_string(msgpack_object *o, const char *key)
{
    char *value = NULL;

    if (!o || !key)
        return value;

    msgpack_object_kv* p = o->via.map.ptr;
    msgpack_object_kv* const pend = o->via.map.ptr + o->via.map.size;

    for(; p < pend; ++p) {
        if (p->key.via.raw.size == strlen(key) &&
            memcmp(p->key.via.raw.ptr, key , strlen(key)) == 0)
        {
            if (p->val.type == MSGPACK_OBJECT_RAW)
            {
                value = (char*)zmalloc(sizeof(char) * (p->val.via.raw.size+1));
                memcpy(value, p->val.via.raw.ptr, p->val.via.raw.size);
                value[p->val.via.raw.size] = '\0';
                return value;
            }
        }
        if (p->val.type == MSGPACK_OBJECT_MAP)
        {
			value = m_get_string(&p->val, key);
			if (value)
				return value;
        }
    }
    return value;
}

uint64_t m_get_int(msgpack_object *o, const char *key)
{

    if (!o || !key)
        return 0;

    msgpack_object_kv* p = o->via.map.ptr;
    msgpack_object_kv* const pend = o->via.map.ptr + o->via.map.size;

    for(; p < pend; ++p) {
        if (p->key.via.raw.size == strlen(key) &&
                memcmp(p->key.via.raw.ptr, key , strlen(key)) == 0)
        {
            if (p->val.type == MSGPACK_OBJECT_POSITIVE_INTEGER)
            {
                return (uint64_t)p->val.via.u64;
            }

            if (p->val.type == MSGPACK_OBJECT_NEGATIVE_INTEGER)
            {
                return (uint64_t)p->val.via.i64;
            }
        }
        if (p->val.type == MSGPACK_OBJECT_MAP)
        {
			m_get_int(&p->val, key);
        }
    }
    return 0;
}

double m_get_double(msgpack_object *o, const char *key)
{

    if (!o || !key)
        return 0;

    msgpack_object_kv* p = o->via.map.ptr;
    msgpack_object_kv* const pend = o->via.map.ptr + o->via.map.size;

    for(; p < pend; ++p) {
        if (p->key.via.raw.size == strlen(key) &&
                memcmp(p->key.via.raw.ptr, key , strlen(key)) == 0)
        {
            if (p->val.type == MSGPACK_OBJECT_DOUBLE)
            {
                return (double)p->val.via.dec;
            }
        }
        if (p->val.type == MSGPACK_OBJECT_MAP)
		{
			m_get_double(&p->val, key);
		}
    }
   
    return 0;
}

bool m_get_bool(msgpack_object *o, const char *key)
{
    if (!o || !key)
        return false;

    msgpack_object_kv* p = o->via.map.ptr;
    msgpack_object_kv* const pend = o->via.map.ptr + o->via.map.size;

    for(; p < pend; ++p) {
        if (p->key.via.raw.size == strlen(key) &&
                memcmp(p->key.via.raw.ptr, key , strlen(key)) == 0)
        {
            if (p->val.type == MSGPACK_OBJECT_BOOLEAN)
            {
                return p->val.via.boolean ? true : false;
            }
        }
        if (p->val.type == MSGPACK_OBJECT_MAP)
		{
			m_get_bool(&p->val, key);
		}
    }
    
    return false;
}
