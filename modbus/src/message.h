#ifndef MESSAGE_H
#define MESSAGE_H

#include "modbus_helper.h"
#include "interface.h"

typedef struct
{
    char             	 *identity;
    char             	 *command;
    char             	 *actionid;
	bool		      	  local;

    msgpack_unpacked  	  object;
    zframe_t             *payload;

    uint64_t         	  in_timestamp;
    uint64_t         	  out_timestamp;
    uint64_t         	  process_timestamp;

	//for call only
    char                 *routed_via;  	   //identity currently routed to
	iface_t			 	 *iface;           //iface assigned

    uint64_t              version;
    char                 *function;
    char                 *module;
	
    zlist_t 		  	 *globals;		   //list of globals copied from iface

} message_t;

//Timeoutes
uint64_t   getInTimeDelta(message_t *msg);
uint64_t   getOutTimeDelta(message_t *msg);
uint64_t   getProcessTimeDelta(message_t *msg);

uint64_t   getTimeStamp();

//Create & destroy
message_t    *message_new();
void          message_destroy(message_t *msg_p);
void 	      free_message(void *data);

//msg conversion
message_t    *message_fromzmq(zmsg_t *msg, bool local);
zmsg_t       *message_tomsg(const message_t *message);

//make specific messages
zmsg_t	*message_response(const char *data, const char *actionid);
zmsg_t	*message_introduce(const char *actionid);
zmsg_t	*message_ifacelist_response(zlist_t *provides, const char *actionid);
zmsg_t	*message_ifacelist_request(const char *actionid);
zmsg_t	*message_stop(const char *actionid);
zmsg_t	*message_exception(const char *actionid, const char *ex_msg, int ex_num);

//checks for message
char      *message_check_command(message_t *message);
char      *message_check_actionid(message_t *message);

zlist_t    *message_getIfaceList(message_t *message);
zhash_t    *message_getProvides(message_t *message);
char       *message_getReqCommand(message_t *message);
char       *message_getFunc(message_t *message);
char       *message_getModule(message_t *message);
char       *message_getExceptionString(message_t *message);
int         message_getExceptionCode(message_t *message);
char       *message_getMessage(message_t *message);
pid_t       message_getPid(message_t *message);
char       *message_getName(message_t *message);
char       *message_getDesc(message_t *message);
uint64_t    message_getIfVersion(message_t *message);

char       *m_get_string(msgpack_object *o, const char *key);

void		message_dump(message_t *message);

#endif // MESSAGE_H

