#ifndef CONFIG_H
#define CONFIG_H

#include "ini.h"
#include "czmq.h"

typedef struct
{
    char     mod_exec[256];
    char     mod_path[256];
    char     mod_workpath[256];
    char     mod_instances[2];
    char     mod_owner[256];
    char	 mod_args[256];

    char	  module_name[32];
    char      module_path[256];
    char      module_workpath[256];
    char      module_user[8];
    char      module_group[8];
    int       module_instances;
    char	  module_args[512];
    
    int		  module_maxmem;
    
    int 	  module_start_timeout; //sec
    bool	  log_err;
    bool	  log_out;
} config_module_t;

typedef struct
{
    char        s_name[256];
    char        s_path[256];
    char        s_owner[256];
    char        s_mode[256];
    char        s_pid_path[256];
    char        s_log_path[256];
    char        s_mod_path[256];
    char        s_mod_work_path[256];
    char		s_runas[256];
    char		s_iface[256];

    char        socket[256];
    char        socket_file[256];
    char        pid_file[256];
    char		log_file[256];
    char        modules_path[256];
    char        modules_workpath[256];
    char        modules_log_err[256];
    char        modules_log_out[256];
    unsigned int    socket_mode;
    char        socket_user[8];
    char        socket_group[8];
    int         log_level;
    bool		debug;
    int			global;
    int         daemon;
    char		runas[16];
    char		iface[16];
    long 		log_max_size;
    char		log_method[16];
    unsigned int    log_mode;
    

    zhash_t     *modules;
    
    int			queue_poll;	   //timeout poll time
    int 		in_timeout;		
    int			out_timeout;
    int			process_timeout;
    int 		modlist_poll; //foreign fabric poll time
    
    zconfig_t   *conf;

} config_t;

config_t* config_new(char *file);
void	  config_delete(config_t *config);

#endif // CONFIG_H
