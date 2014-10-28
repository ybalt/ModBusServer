#ifndef MODBUSD_H
#define MODBUSD_H

#include "common.h"
#include "config.h"
#include "fabric.h"
#include "message.h"
#include "beacon.h"
#include "module.h"
#include "process.h"
#include "procman.h"
#include "logger.h"
#include "interface.h"


#define DEBUG                    false    //zloop verbose
#define IDENTITY_LEN           	 5   		//in bytes


typedef struct
{
    zctx_t    *ctx;           	//context
    void      *procman;      	//process manager port
    void      *udp;           	//udp port
    void      *global_fe;     	//global socket
    void      *local_fe;      	//local socket

    zhash_t   *fab_list;      	//global fabric list
    zhash_t   *modules;         //modules registered on local fabric
    
    zlist_t	  *ifaces;    		//interfaces
    zhash_t   *queue;  			//message queue

    config_t  *conf;        	//config
    zloop_t   *main_loop;
} proxy_t;


typedef enum {
	NO_MODULES = 110,
	MODULES_BUSY = 111,
	PROCESS_TIMEOUT = 112,
	GLOBAL_TIMEOUT = 113,
	LOCAL_TIMEOUT = 114,
	WRONG_MESSAGE_STRUCTURE = 20, 
	WRONG_CALL_STRUCTURE = 21
} exception_code;


char *getActionid();

void processIntroduce       (message_t *message);
void processCommand         (message_t *message);
void processResult          (message_t *message);
void processRequest         (message_t *message);
void processException       (message_t *message);
void processCall            (message_t *message);
void processReq_IfaceList	(message_t *message);
void processMessage         (message_t *message);
void processStop            (message_t *message);


#endif // MODBUSD_H


