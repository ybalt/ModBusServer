#include "config.h"

void free_mod(void *data)
{
	config_module_t *mod = (config_module_t*)data;
	if (mod)
	{
		//free(mod->mod_exec);
		//free(mod->mod_path);
        //free(mod->mod_workpath);
		//free(mod->mod_instances);
		//free(mod->mod_owner);
		free(mod);
	}
}

config_t* config_new(char *file)
{
    config_t *config = zmalloc(sizeof(config_t));
    assert(config);
    
    if (!file)
        return NULL;
        
	long n;
	int s, k;
	char section[50];
	char module[50];
	char value[256];
	
	for (s = 0; ini_getsection(s, section, sizearray(section), file) > 0; s++) 
	{
		zclock_log("[%s]", section);
		if (streq("server", section)) 
		{
			
			n = ini_gets(section, "name", "", config->s_name, sizearray(config->s_name), file);
			if (!config->s_name) {
				config_delete(config);
				return NULL;
			}

			n = ini_gets(section, "s_path", "./", config->s_path, sizearray(config->s_path), file);
			snprintf(config->socket, 256, "ipc://%s%s.socket", config->s_path, config->s_name);
			snprintf(config->socket_file, 256, "%s%s.socket", config->s_path, config->s_name);
			zclock_log("using %s as socket file",config->socket);

			n = ini_gets(section, "s_owner", "", config->s_owner, sizearray(config->s_owner), file);
			if (!streq(config->s_owner,"")) {
				snprintf(config->socket_user, 8, "%s", strtok(config->s_owner, ":"));
				snprintf(config->socket_group, 8, "%s", strtok(NULL, ":"));
			}
			zclock_log("socket owner %s:%s", config->socket_user, config->socket_group);
	
			n = ini_gets(section, "s_mode", "0666", config->s_mode, sizearray(config->s_mode), file);
			if (config->s_mode) {
				config->socket_mode = strtol(config->s_mode, 0, 8);
			}
			zclock_log("socket mode %0o", config->socket_mode);

		    n = ini_gets(section, "pid_path", "./", config->s_pid_path, sizearray(config->s_pid_path), file);
		    snprintf(config->pid_file, 256, "%s%s.pid", config->s_pid_path, config->s_name);
		    zclock_log("using %s as pid file",config->pid_file);
		
			n = ini_gets(section, "log_path", "./", config->s_log_path, sizearray(config->s_log_path), file);
		    snprintf(config->log_file, 256, "%s%s.log", config->s_log_path, config->s_name);
		    zclock_log("using %s as log file",config->log_file);
		
		
		    n = ini_gets(section, "mod_path", "./", config->s_mod_path, sizearray(config->s_mod_path), file);
		    snprintf(config->modules_path, 256, "%s", config->s_mod_path);
		    zclock_log("using %s as module path",config->modules_path);
		
		    n = ini_gets(section, "mod_work_path", "./", config->s_mod_work_path, sizearray(config->s_mod_work_path), file);
		    snprintf(config->modules_workpath, 256, "%s", config->s_mod_work_path);
		    zclock_log("using %s as module workdir path",config->modules_workpath);
		
		
		    if (ini_gets(section, "debug", "", value, sizearray(value), file)>0) {
		        config->debug = atoi(value)? true : false;
		        zclock_log("set debug %s ",(config->debug)?"YES":"NO");
		    }
		    
		    n = ini_gets(section, "log_level", "", value, sizearray(value), file);
		    if (n) {
		        config->log_level = atoi(value);
		        zclock_log("set log_level level to %d",config->log_level);
		    }
		
		    n = ini_gets(section, "global", "", value, sizearray(value), file);
		    if (n) {
		        config->global = atoi(value);
		        zclock_log("set fabric global mode %d",config->global);
		    }
		
		    n = ini_gets(section, "daemon", "", value, sizearray(value), file);
		    config->daemon=0;
		    if (n) {
		        config->daemon = atoi(value);
		        if (config->daemon)
		        {
		            zclock_log("process will be daemonized");
		        }
		    }
		    
		    if (ini_gets(section, "queue_poll", "10", value, sizearray(value), file)>0)
			{
		        config->queue_poll = atoi(value);
		        zclock_log("set queue_poll time to %d",config->queue_poll);
		        
		    }
		    
			n = ini_gets(section, "modlist_poll", "5000", value, sizearray(value), file);
		    if (n) {
		        config->modlist_poll = atoi(value);
		        zclock_log("set modlist_poll time to %d",config->modlist_poll);
		        
		    }
		    
			n = ini_gets(section, "in_timeout", "1000", value, sizearray(value), file);
		    if (n) {
		        config->in_timeout = atoi(value);
		        zclock_log("set in_timeout time to %d",config->in_timeout);
		    }
		    
		    n = ini_gets(section, "out_timeout", "1000", value, sizearray(value), file);
		    if (n) {
		        config->out_timeout = atoi(value);
		        zclock_log("set out_timeout time to %d",config->out_timeout);
		    }
		    
		    if (ini_gets(section, "process_timeout", "10000", value, sizearray(value), file)>0) 
		    {
		        config->process_timeout = atoi(value);
		        zclock_log("set process_timeout time to %d",config->process_timeout);
		    }
		    
		    if (ini_gets(section, "runas", "", value, sizearray(value), file)>0) 
		    {
				snprintf(config->runas, 16, "%s", value);
				if (!streq(config->runas, ""))
					zclock_log("fabric will be running as %s",config->runas);
			}
			
			
		    if (ini_gets(section, "iface", "", value, sizearray(value), file)>0) 
		    {
				snprintf(config->iface, 16, "%s", value);
				if (!streq(config->iface, ""))
					zclock_log("fabric will be binded at %s",config->iface);
			}
		
		    if (ini_gets(section, "log_max_size", "0", value, sizearray(value), file)>0) 
		    {
		        config->log_max_size = atol(value);
		        zclock_log("set log_max_size to %ldMb",config->log_max_size);
		    }
		    
		    if (ini_gets(section, "log_method", "none", value, sizearray(value), file)>0) 
		    {
		        strncpy(config->log_method, value, 16);
		        zclock_log("set log_method to %s",config->log_method);
		    }
		    
		    if (ini_gets(section, "log_mode", "0666", value, sizearray(value), file)>0) 
		    {
		        config->log_mode = strtol(value, 0, 8);
		        zclock_log("log file mode %0o", config->log_mode);
		    }
		    
		    if (ini_gets(section, "mod_log_err", "./", value, sizearray(value), file)>0) 
		    {
				snprintf(config->modules_log_err, 256, "%s", value);
				zclock_log("using %s as module log stderr path",config->modules_log_err);
			}
		    
			if (ini_gets(section, "mod_log_out", "./", value, sizearray(value), file)>0) 
			{
				snprintf(config->modules_log_out, 256, "%s", value);
				zclock_log("using %s as module log stdout path",config->modules_log_out);
			}
		}
		if (streq("modules", section)) 
		{
			config->modules = zhash_new();
	        for (k = s+1; ini_getsection(k, module, sizearray(module), file)>0; k++)
	        {
                zclock_log("found module %s:", module);
                config_module_t *mod_module = (config_module_t*)zmalloc(sizeof(config_module_t));
                assert(mod_module);
				snprintf(mod_module->module_name, 32, "%s", module);
                if (ini_gets(module, "exec", "", mod_module->mod_exec, sizearray(mod_module->mod_exec), file)>0)
                {
                    snprintf(mod_module->module_path, 256, "%s%s",
                             config->modules_path,
                             mod_module->mod_exec);
                    zclock_log("\tusing %s as executable file",mod_module->module_path);

                    if (ini_gets(module, "workpath", "./", mod_module->mod_workpath, sizearray(mod_module->mod_workpath), file)>0)
                    {
						snprintf(mod_module->module_workpath, 256, "%s%s",
                             config->modules_workpath,
                             mod_module->mod_workpath);
                    }
                    zclock_log("\tusing %s as workpath",mod_module->module_workpath);
					
					ini_gets(module, "args", "", mod_module->mod_args, sizearray(mod_module->mod_args), file);
					if (!streq(mod_module->mod_args,""))
					{
						snprintf(mod_module->module_args, 256, "%s %s",
                             config->socket,
                             mod_module->mod_args); 
                    } else {
						snprintf(mod_module->module_args, 256, "%s ",
                             config->socket); 
					}
                    zclock_log("\tusing %s as args",mod_module->module_args);
					
                    if (ini_gets(module, "instances", "1", mod_module->mod_instances, sizearray(mod_module->mod_instances), file)>0)
                    {
						mod_module->module_instances = atoi(mod_module->mod_instances);
					}
					zclock_log("\tmax instances %d",mod_module->module_instances);
                    
                    if (ini_gets(module, "start_timeout", "10", value, sizearray(value), file)>0)
                    {
						mod_module->module_start_timeout = atoi(value);
					}
					zclock_log("\tstart timeout %d sec",mod_module->module_start_timeout);
                    
                    if (ini_gets(module, "maxmem", "0", value, sizearray(value), file)>0)
                    {
						mod_module->module_maxmem = atoi(value);
                    }
                    zclock_log("\tmax mem %d MB",mod_module->module_maxmem);

                    if (ini_gets(module, "owner", "", mod_module->mod_owner, sizearray(mod_module->mod_owner), file)>0)
                    {
                        snprintf(mod_module->module_user, 8, "%s", strtok(mod_module->mod_owner, ":"));
                        snprintf(mod_module->module_group, 8, "%s", strtok(NULL, ":"));
                        zclock_log("\tmodule owner %s:%s", mod_module->module_user, mod_module->module_group);
                    }
                    
                    if (ini_gets(module, "log_err", "0", value, sizearray(value), file)>0)
                    {
						mod_module->log_err = atoi(value)? true : false;
						zclock_log("\tset log err %s ",(mod_module->log_err)?"YES":"NO");
					}
					
					if (ini_gets(module, "log_out", "0", value, sizearray(value), file)>0) 
					{
						mod_module->log_out = atoi(value)? true : false;
						zclock_log("\tset log out %s ",(mod_module->log_out)?"YES":"NO");
					}
                    
                    zhash_insert(config->modules, module, mod_module);
                    zhash_freefn(config->modules, module, free_mod);
                } else {
                    zclock_log("\tmodule %s have no executable, skipping", module);
                    free(mod_module);
                }
	        }
	    }
    }
    return config;
}

void config_delete(config_t *conf)
{
	if (conf)
	{
		zhash_destroy(&conf->modules);
		zconfig_destroy(&conf->conf);
		free(conf);
	}
	
}


