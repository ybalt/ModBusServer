#include "interface.h"

iface_t	*iface_new(const char *mod, const char *func, uint64_t ver)
{
		if (!mod || !func || ver==0) {
			return NULL;
		}
		iface_t *iface = (iface_t*)zmalloc(sizeof(iface_t));
		assert(iface);
		
		iface->mod = (char*)zmalloc(sizeof(char)*strlen(mod)+1);
		strncpy(iface->mod, mod, strlen(mod));

		iface->func = (char*)zmalloc(sizeof(char)*strlen(func)+1);
		strncpy(iface->func, func, strlen(func));
		
		//iface->id_queue = zlist_new();
		iface->id_local_list= zlist_new();
		iface->id_global_list= zlist_new();
		zlist_autofree(iface->id_global_list);
		
		iface->ver = ver;
		
		return iface;
}

void	iface_destroy(iface_t *iface)
{
		if (iface->mod)
			free(iface->mod);
		if (iface->func)
			free(iface->func);
		//if (iface->id_queue)
		//	zlist_destroy(&iface->id_queue);
		if (iface->id_global_list)
			zlist_destroy(&iface->id_global_list);
		if (iface->id_local_list)
			zlist_destroy(&iface->id_local_list);

		free(iface);
}

void	free_iface(void *data)
{
	if (data) {
        iface_t *iface = (iface_t*)data;
        iface_destroy(iface);
    }
}

bool iface_append_global(const iface_t *iface, const char *global)
{
	if (!iface || !global)
		return false;
	char *found = NULL;
	char *cur = (char*)zlist_first(iface->id_global_list);
	while (cur)
	{
		if (streq(global, cur)) {
			found = cur;
			break;
		}
		cur = (char*)zlist_next(iface->id_global_list);
	}
	if (!found)
	{
		zlist_append(iface->id_global_list, (void*)global);
		zlist_freefn(iface->id_global_list, (void*)global, free_iface, 1);
		log_debug("interface", "interface [%s:%s:%ld] add module %s to global", 
				iface->mod, iface->func, iface->ver, global);
		return true;
	}
	return false;
}
bool iface_append_local(const iface_t *iface, const char *local)
{
	if (!iface || !local)
		return false;
	char *found = NULL;
	char *cur = (char*)zlist_first(iface->id_local_list);
	while (cur)
	{
		if (streq(local, cur)) {
			found = cur;
			break;
		}
		cur = (char*)zlist_next(iface->id_local_list);
	}
	if (!found)
	{
		zlist_append(iface->id_local_list, (void*)local);
		zlist_freefn(iface->id_local_list, (void*)local, free_iface, 1);
		//zlist_append(iface->id_queue, (void*)local);
		//zlist_freefn(iface->id_queue, (void*)local, free_iface, 1);
		log_debug("interface", "interface [%s:%s:%ld] add module %s to local", 
				iface->mod, iface->func, iface->ver, local);
		return true;
	}
	return false;
}
void iface_remove(zlist_t *ifaces, const char *identity)
{
	if (!ifaces || !identity)
		return;
	iface_t *iface = (iface_t*)zlist_first(ifaces);
	while (iface)
	{
		zlist_remove(iface->id_global_list, (void*)identity);
		zlist_remove(iface->id_local_list, (void*)identity);
		//zlist_remove(iface->id_queue, (void*)identity);
		if (zlist_size(iface->id_global_list)== 0 && 
			zlist_size(iface->id_local_list) == 0) 
		{
			log_debug("interface", "interface [%s:%s:%ld] empty, will be removed", 
				iface->mod, iface->func, iface->ver);
			zlist_remove(ifaces, (void*)iface);
		}
		iface = (iface_t*)zlist_next(ifaces);
	}
}

void iface_print(const iface_t *iface)
{
	char out[1024];
	sprintf(out, "dump_interface [%s:%s:%ld]", iface->mod, iface->func, iface->ver);
	strcat(out, "locals[");
	char *cur = (char*)zlist_first(iface->id_local_list);
	while (cur)
	{
		strcat(out, cur);
		strcat(out, "");
		cur = (char*)zlist_next(iface->id_local_list);
	}
	strcat(out, "] globals[");
	cur = (char*)zlist_first(iface->id_global_list);
	while (cur)
	{
		strcat(out, cur);
		strcat(out, "");
		cur = (char*)zlist_next(iface->id_global_list);
	}
	/*strcat(out, "] queue [");
	cur = (char*)zlist_first(iface->id_queue);
	while (cur)
	{
		strcat(out, cur);
		strcat(out, "");
		cur = (char*)zlist_next(iface->id_queue);
	}*/
	strcat(out, "]");
	log_debug("interface", "%s", out);
}
