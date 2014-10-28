#include "process.h"

process_t  *process_new(const char *name)
{
	process_t *proc = (process_t*)zmalloc(sizeof(process_t));
	assert(proc);
	proc->name = (char*)zmalloc(strlen(name)+1);
	strcpy(proc->name, name);
	proc->pid = 0;
	proc->mod = NULL;
	proc->started = false;
	proc->on_restart = false;
	proc->start_timestamp = 0;
	
	return proc;
	
}

void process_destroy(process_t *proc)
{
    if (proc) {
		if (proc->name)
			free(proc->name);
		proc->mod = NULL;
        free(proc);
    }
}

void free_process(void *data)
{
	if (data) {
        process_t *proc = (process_t*)data;
        process_destroy(proc);
    }
}

