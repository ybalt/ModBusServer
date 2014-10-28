#ifndef INTERFACE_H
#define INTERFACE_H

#include "common.h"
#include "logger.h"

typedef struct {
	char 	*mod, *func;
	uint64_t ver;
	
	//zlist_t 		*id_queue; 		        //queue local modules id's
	zlist_t			*id_global_list; 		//list of global fabric id's
	zlist_t 		*id_local_list; 		//list of local module's id
} iface_t;

iface_t     *iface_new(const char *mod, const char *func, uint64_t ver);
void         iface_destroy(iface_t *iface);
void         free_iface(void *data);

bool		 iface_append_global(const iface_t *iface, const char *global);
bool		 iface_append_local(const iface_t *iface, const char *global);

void		 iface_remove(zlist_t *ifaces, const char *global);

void		 iface_print(const iface_t *iface);

#endif //INTERFACE_H
