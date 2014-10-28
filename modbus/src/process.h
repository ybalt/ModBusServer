#ifndef PROCESS_H
#define PROCESS_H

#include "modbusd.h"

typedef struct
{
	char     		*name;
	uint64_t 	 	 pid;
	config_module_t *mod;
	int				 instance;
	int64_t			 start_timestamp;
	bool			 started;
	bool			 on_restart;
} process_t;

process_t     *process_new(const char *name);
void           process_destroy(process_t *process);
void           free_process(void *data);

#endif // PROCESS_H
