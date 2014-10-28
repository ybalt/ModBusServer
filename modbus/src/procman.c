#include "procman.h"

#define DEVNULL "/dev/null"

static zhash_t  	*procs;
static config_t 	*conf;
static bool	 		 stop_all;

void child(config_module_t *mod, bool debug)
{

	FILE *out, *err, *in;
	//restore default handler
	struct sigaction action;
	action.sa_handler = SIG_DFL;
	sigfillset (&action.sa_mask);
	action.sa_flags = 0;
	sigaction (SIGCHLD, &action, NULL);
	int rc = 0;
	//set workdir
	log_info("procman", "set workdir to %s for %s", mod->module_workpath, mod->module_workpath);
	if ((chdir(mod->module_workpath)) < 0) {
		log_err("procman", "error changing module %s workpath %s", mod->module_path, mod->module_workpath);
		exit(0);
	}
	//std redirections
	//if (debug) 
	{ //redirect to files
		char out_file[256], err_file[256]; 
		sprintf(out_file, "%s%s-out.log", conf->modules_log_out, mod->module_name);
		sprintf(err_file, "%s%s-err.log", conf->modules_log_err, mod->module_name);
		in  = freopen( DEVNULL, "a", stdin);
		if (mod->log_out)
		{
			err = freopen( out_file, "a", stdout);
			log_info("procman", "set stdout to %s", out_file);
		} else {
			err = freopen( DEVNULL, "a", stdout);
		}
		if (mod->log_err) {
			out = freopen( err_file, "a", stderr);
			log_info("procman", "set stderr to %s", err_file);
		} else {
			out = freopen( DEVNULL, "a", stderr);
		}
	} 
	
	if (out == 0 || err == 0 || in == 0) {
			log_err("procman", "warning, error to redirect std streams for %s", mod->module_name);
	}
	//exec a module
    log_info("procman", "starting %s %s", mod->module_path, mod->module_args);
    
    char *args[16];
    args[0] = mod->module_path;
    char *arg_copy = strdup(mod->module_args);
    char *arg = strtok(arg_copy, " ");
    int i=1;
    while(arg)
    {
		log_debug("procman", "arg %s found", arg);
		args[i++] = arg;
		arg = strtok(NULL, " ");

	} 
	args[i++] = NULL;
	if ((getuid() == 0 || geteuid() == 0) && 
		!streq(mod->module_user,"") && !streq(mod->module_group,"")) {
        struct passwd *pw = getpwnam(mod->module_user);
        if ( pw ) {
            log_info("procman", "setting user:group to %s:%s", mod->module_user, mod->module_group );
            rc = seteuid( pw->pw_uid );
            if (!rc) 
				log_err("procman","unable to set user");
			rc = setegid( pw->pw_gid );
            if (!rc) 
				log_err("procman","unable to set group");
        }
    }
    rc = execv(mod->module_path, args);
    if (rc) {
		log_err( "procman", "unable to start file %s, code=%d (%s)",
                  mod->module_path, errno, strerror(errno) );
                  exit(0);
    }
}

char *strpid(uint64_t pid)
{
	const int n = snprintf(NULL, 0, "%ld", pid);
	assert(n > 0);
	char *buf = (char*)zmalloc(sizeof(char)*n+1);
	int c = snprintf(buf, n+1, "%ld", pid);
	assert(buf[n] == '\0');
	assert(c == n);
	return buf;
}

void start_proc(process_t *proc) 
{
	log_info("procman", "starting [%s] instance [%d]", proc->name, proc->instance);
	proc->start_timestamp = zclock_time();
	uint64_t fork_rv = fork();
	if (fork_rv == 0) {
		child(proc->mod, conf->debug);
	}
	if (fork_rv > 0) {

		char *pid = strpid(fork_rv);
		log_info("procman", "process [%s] forked with pid [%s]", proc->name, pid);

		proc->pid = fork_rv;
		zhash_insert(procs, pid, proc);
		zhash_freefn(procs, pid, free_process);
		free(pid);
	}
	if (fork_rv == -1) {
		log_err("procman", "fork fail for [%s]", proc->name);
		free(proc);
	}
}

static int startall_item (const char *key, void *item, void *arg)
{
	assert(procs);
    int count;

	config_t *conf = (config_t*)arg;
	assert(conf);

	config_module_t *mod = (config_module_t*)item;
	assert(mod);

    if( access( mod->module_path, F_OK ) == -1 ) 
	{
		log_err("procman", "unable to access module file %s, code=%d (%s)",
                  mod->module_path, errno, strerror(errno) );
                  return 0;
	}
    
	for (count = 1; count <= mod->module_instances; count ++)
	{
		process_t *proc = process_new(key);
		proc->mod = mod;
		proc->instance = count;
		start_proc(proc);
		zclock_sleep(10);
    }
	return 0;
}

static int checkpid_item (const char *key, void *item, void *arg)
{
	char fname[256];
	process_t* proc = (process_t*)item;

	if (!proc || stop_all || proc->on_restart)
	{
		return 0;
	}
	
	if (proc && 
		proc->mod &&
		!proc->started && 
		proc->start_timestamp &&
		(zclock_time()-proc->start_timestamp)>proc->mod->module_start_timeout*1000 )
	{
		log_err("procman", "process %s is not started in time %ld s", 
			key, proc->mod->module_start_timeout);
		pid_t pid = (pid_t)strtol(key, 0, 10);
		kill(pid, 9);
		zhash_delete(procs, key);
		return 0;
	}
	sprintf(fname, "/proc/%s/cmdline", key);
	if( access( fname, F_OK ) == -1 ) {
		log_info("procman", "no process with pid [%s], removing", key);
		zstr_sendx(arg, "restart", key, NULL);
		proc->on_restart = true;
		return 0;
	}
	if (proc->started && 
		proc->mod->module_maxmem != 0) { //will check for process mem usage
		FILE *statm;
		sprintf(fname, "/proc/%s/statm", key);
		statm = fopen(fname, "r");
		if (statm) {
			long rss = 0L;
			int i = fscanf( statm, "%ld", &rss );
			fclose(statm);
			if (i) {
				uint64_t overall = (((size_t)rss * (size_t)sysconf( _SC_PAGESIZE)))/(1024*1024);
				//log_debug("procman: module [%s] use [%lld] MB maxmem [%lld]", key, 
				//			overall, proc->mod->module_maxmem);
				if (overall > proc->mod->module_maxmem) {
					log_info("procman", "process [%s] reached [%lld] maxmem threshold [%lld]", 
							key, overall, proc->mod->module_maxmem);
					
					zstr_sendx(arg, "restart", key, NULL);
					return 0;
				}

			}
		} else {
			log_err("procman", "can't open statm file for %s", key);
		}
	}
	return 0;
}

static int killall_item(const char *key, void *item, void *arg)
{
	process_t *proc = (process_t*)item;
	log_info("procman", "send SIGTERM to %s with pid %ld", proc->name, proc->pid);
	if (!proc->pid)
		return 0;
	assert(proc);
	kill(proc->pid, 3);
	zclock_sleep(10);
	kill(proc->pid, 9);
	return 0;
}

static int checkpid_event(zloop_t *loop, int timer_id, void *arg)
{
	assert(arg);
	if (zhash_size(procs)>0)
	{
		zhash_foreach(procs, checkpid_item, arg);
	}
	return 0;
}

static int s_main_event(zloop_t *loop, zmq_pollitem_t *item, void *arg)
{
	char *command = zstr_recv(item->socket);
	log_debug("procman", "received %s command", command);
	if (streq(command, "stop_all"))
	{
		stop_all = true;
	}
	if (streq(command, "check_pid")) {
		char *spid = zstr_recv(item->socket);
		assert(spid);
		process_t *proc = (process_t*)zhash_lookup(procs, spid);
		if (!proc) {
			log_err("procman", "process [%s] check_pid fault", spid );
			zstr_sendx(item->socket, "check_pid","no", NULL);
		} else {
			log_info("procman", "process [%s] check_pid ok", spid );
			proc->started = 1;
			zstr_sendx(item->socket, "check_pid","yes", NULL);
		}
		free(spid);
		free(command);
		return 0;
	}
	if (streq(command, "register")) {
		char *spid = zstr_recv(item->socket);
		char *name = zstr_recv(item->socket);
		process_t *proc = (process_t*)zhash_lookup(procs, spid);
		assert(spid);
		assert(name);

		if (!proc) {
			log_info("procman", "process [%s] with pid [%s] will be registered", name, spid );
			proc = process_new(name);
			assert(proc);
			zhash_insert(procs, spid, (void*)proc);
		} else {
			log_info("procman", "process [%s] with pid [%s] already present", name, spid );
		}
		free(command);
		free(spid);
		free(name);
		return 0;
	}
	if (streq(command, "kill")) {
		char *spid = zstr_recv(item->socket);
		process_t *proc = (process_t*)zhash_lookup(procs, spid);
		if (proc) {
			pid_t pid = (pid_t)strtol(spid, 0, 10);
			log_info("procman", "process with pid [%s] will be killed", spid );
			int rc = kill(pid, 3);//soft
			if (rc)
				log_err("procman", "kill3 fail - code=%d (%s)", errno, strerror(errno));
			sleep(1);
			rc = kill(pid, 3);//hard
			if (rc)
				log_err("procman", "kill9 fail - code=%d (%s)", errno, strerror(errno));
			zhash_delete(procs, spid);
		} else {
			log_err("procman", "process with pid [%s] can't be killed - no proc found", spid );
		}
		free(spid);
	}
	if (streq(command, "restart")) {
		char *spid = zstr_recv(item->socket);
		process_t *proc = (process_t*)zhash_lookup(procs, spid);
		if (proc) {
			pid_t pid = (pid_t)strtol(spid, 0, 10);
			log_info("procman", "process [%s] with pid [%s] will be restarted", 
						proc->name, spid );
			kill(pid, 3);//soft kill
			sleep(1);
			kill(pid, 9);//hard kill
			zstr_sendx(item->socket, "remove", spid, NULL);
			
			process_t *new_proc = process_new(proc->name);
			new_proc->mod = proc->mod;
			zhash_delete(procs, spid);
			start_proc(new_proc);
		} else {
			log_err("procman", "process with pid [%s] can't be restarted - no proc found", spid );
		}
		free(spid);
	}
	if (streq(command, "killall")) {
		if (zhash_size(procs)>0) {
			log_info("procman", "all %ld process will be killed", zhash_size(procs));
			zhash_foreach(procs, killall_item, NULL);
		} else {
			log_info("procman", "no processes in list");
		}
		
	}
	free (command);
	return 0;
}

void procman_thread (void *args, zctx_t *ctx, void *pipe)
{
    assert(ctx);
	stop_all = false;
    conf = (config_t*)args;
    assert(conf);

    int rc = 0;
	procs = zhash_new();

	//set SIGCHLD
    struct sigaction action;
	action.sa_handler = SIG_IGN;
	sigfillset (&action.sa_mask);
	action.sa_flags = 0;
 	sigaction (SIGCHLD, &action, NULL);

	//wait for start
    char *start = zstr_recv(pipe);
    if (start) {
		free(start);
		log_info("procman", "starting all modules", start);
		zhash_foreach(conf->modules, startall_item, conf);
	}

    zmq_pollitem_t poll_main = { pipe, 0, ZMQ_POLLIN, 0};

    zloop_t *procman_loop = zloop_new ();
    assert (procman_loop);
    zloop_set_verbose (procman_loop, DEBUG);

    rc = zloop_poller (procman_loop, &poll_main, s_main_event, NULL);
    assert (rc == 0);

	zloop_timer (procman_loop, 10, 0, checkpid_event, pipe);
    //start reactor
    zloop_start (procman_loop);
    zloop_destroy (&procman_loop);

}


