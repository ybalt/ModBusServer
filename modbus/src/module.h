#ifndef MODULE_H
#define MODULE_H

#include "modbusd.h"

typedef enum
{
	GUEST,
    IDLE,
	BUSY,
    STOPPING,
	RESTARTING
} mod_state;

typedef struct 
{
    char      *identity;
    char      *name;
    char      *desc;
    pid_t      pid;
    char	   s_pid[10];
    zhash_t   *provides; //module:ver

    mod_state state;
    
    bool	   need_restart;
    iface_t   *iface;

} module_t;


module_t     *module_new(const char *identity, const char *name);
void          module_destroy(module_t *module);
void          free_module(void *data);

#endif // MODULE_H
