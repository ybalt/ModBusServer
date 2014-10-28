#include "module.h"

module_t *module_new(const char *identity, const char *name)
{
    module_t *module = (module_t *)zmalloc(sizeof(module_t));
    assert(module);

    if (!identity || !name) {
        free(module);
        return NULL;
    }
	
    module->identity = (char*)zmalloc(sizeof(char)*strlen(identity)+1);
    strncpy(module->identity, identity, strlen(identity));

    module->name = (char*)zmalloc(sizeof(char)*strlen(name)+1);
    strncpy(module->name, name, strlen(name));
    
    module->need_restart = false;

    return module;
}

void module_destroy(module_t *module)
{
    if (module) {
        if (module->name) {
            free(module->name);
        }
        if (module->identity) {
            free(module->identity);
        }
        if (module->provides){
            zhash_destroy(&module->provides);
        }
        if (module->desc) {
			free(module->desc);
		}
		
        free(module);
    }
}

void free_module(void *data)
{
    if (data) {
        module_t *module = (module_t*)data;
        module_destroy(module);
    }
}
