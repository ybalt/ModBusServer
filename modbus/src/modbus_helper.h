#ifndef MSGPACK_HELPER_H
#define MSGPACK_HELPER_H

#include <msgpack.h>
#include <stdio.h>
#include <czmq.h>

char  *getActionid();
void  *m_lookup(msgpack_object o, const char *key);

void m_unpack_map(zhash_t *ptr, msgpack_object o);
void m_unpack_array(zlist_t *ptr, msgpack_object o);

void m_pack_raw (msgpack_packer *pk, const char *name);
void m_pack_list(msgpack_packer *pk, zlist_t *list);

msgpack_object *m_find_map(msgpack_object o, const char *key);

char       *m_get_string(msgpack_object *o, const char *key);
uint64_t    m_get_int(msgpack_object *o, const char *key);
double      m_get_double(msgpack_object *o, const char *key);
bool        m_get_bool(msgpack_object *o, const char *key);

#endif // MSGPACK_HELPER_H


