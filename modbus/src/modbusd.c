#include "modbusd.h"

static int          f_lock;
static config_t    *conf;
static proxy_t	   *proxy;
static pid_t		fab_pid;

typedef struct {
	char *command;
	pid_t pid;
} procman_command;

static int send_message(zmsg_t **msg, char *identity);

//Termination handle
static int stopall_item(const char *key, void *item, void *arg)
{
	module_t *mod = (module_t*)item;
	assert(mod);
	char *actionid = getActionid();
	zmsg_t *msg = message_stop(actionid);
	send_message(&msg, mod->identity);
	zmsg_destroy(&msg);
	free(actionid);
	return 0;
}

static int closefabs_item(const char *key, void *item, void *arg)
{
	fabric_t *fab = (fabric_t*)item;
	assert(fab);
	if (fab->peer) 
	{
		log_info("fabric", "fab [%s] will be disconnected", fab->identity);
		zsocket_disconnect(fab->peer, "%s",fab->identity);
	}
	return 0;
}

static void finalize(int status)
{
	if (zctx_interrupted == 1)
		exit(0);
	zstr_send(proxy->procman,"stop_all");
	if (zhash_size(proxy->modules)>0) {
			log_info("fabric", "send STOP to all processes");
			zhash_foreach(proxy->modules, stopall_item, NULL);
	}
	if (proxy->ifaces && zlist_size(proxy->ifaces)>0) {
		log_info("fabric", "destroy all interfaces");
		//iface_t *iface = (iface_t*)zlist_first(proxy->ifaces);
		//while(iface)
		//{
		//	iface_destroy(iface);
		//	iface = (iface_t*)zlist_next(proxy->ifaces);
		//}
		zlist_destroy(&proxy->ifaces);
	}
	zclock_sleep(2000); //wait 2 sec
    zstr_send(proxy->procman,"killall");
	zclock_sleep(2000); //wait 2 sec
	log_info("fabric", "all processes killed");
    
    log_info("fabric", "set zmq context interrupted");
    zctx_interrupted = 1;
    log_info("fabric", "destroying main loop");
    zloop_destroy (&proxy->main_loop);
	log_info("fabric", "removing all foreign fabrics");
	zhash_foreach(proxy->fab_list, closefabs_item, NULL);
    log_info("fabric", "removing queues");
    zhash_destroy(&proxy->fab_list);

    zhash_destroy(&proxy->queue);
    zhash_destroy(&proxy->modules);
    log_info("fabric", "destroying context");
    zctx_destroy (&proxy->ctx);
	log_info("fabric", "freeing resources");
	free(proxy); 
    if (lockf(f_lock,F_ULOCK,0) != 0) 
    {
	        log_err("fabric", "unable to unlock pid-file, code=%d (%s)",
	                    errno, strerror(errno) );
	} else {
			log_info("fabric", "unlock pid-file ok");
	}
	if(unlink(conf->pid_file) == 0) {
	log_info("fabric", "fabric", "%s pid-file removed", conf->pid_file);
	} else {
	        log_err("fabric", "error remove pid-file %s, code=%d (%s)",conf->pid_file,
	          errno, strerror(errno));
	}
	log_info("fabric", "all objects closed, fabric exit");
	config_delete(conf);
	exit(0);
}

static void s_signal_handler (int signal_value)
{
    log_info("fabric", "handler got [%s]", strsignal(signal_value));
    finalize(EXIT_SUCCESS);
}

static void parent_handler(int signum) 
{
	if (signum == SIGCHLD)
		exit(EXIT_SUCCESS);
	else
		exit(EXIT_FAILURE);
}

static int send_message(zmsg_t **msg, char *identity)
{
    assert(identity);
    assert(*msg);
    int rc = -1;
    
    void *socket = NULL;
   
    if (*msg) 
    {
		int size = 0;
        if (strlen(identity)>10) // global
        {
			fabric_t *fab = (fabric_t*)zhash_lookup(proxy->fab_list, identity);
			if (fab) 
			{
				socket = fab->peer;
			} else {
				log_err("fabric", "global fabric [%s] not found in list", identity);
			}
		} else { //local
			zframe_t *id_frame;
			const char *pos = identity;
			unsigned char val[5];
			size_t count = 0;
		
			//convert key to identity - ?
			for(count = 0; count < sizeof(val)/sizeof(val[0]); count++) {
				sscanf(pos, "%2hhx", &val[count]);
				pos += 2 * sizeof(char);
			}
			id_frame = zframe_new(&val, sizeof(val));
			zmsg_push(*msg, id_frame);
			socket = proxy->local_fe;
		}
		if (socket) 
		{
			size = zmsg_content_size (*msg);
			rc = zmsg_send(msg, socket);
			if (rc == -1) {
				log_err("fabric", "send to [%s] fail - %s", identity, zmq_strerror (errno));
			} else {
				log_debug("fabric", "send to [%s] ok, size [%d]", identity, size);
			}
		} else {
			return -1;
		}
        zmsg_destroy(msg);
    }
    return rc;
}

static iface_t* check_interface(char *mod, char *func, uint64_t ver)
{
	assert(mod);
	assert(func);
	
	//log_debug("fabric", " interface list size [%d] will be checked for [%s:%s:%d]",
	//					zlist_size(proxy->ifaces), mod, func, ver);
	iface_t *iface = (iface_t*)zlist_first(proxy->ifaces);
	while(iface)
	{
		//log_debug("fabric", "interface [%s:%s:%d] comparing to [%s:%s:%d]",
		//					iface->mod, iface->func, iface->ver,
		//					mod, func, ver);
		if (streq(iface->mod, mod) && streq(iface->func, func) && iface->ver >= ver)
		{
			return iface;
		}
		iface = (iface_t*)zlist_next(proxy->ifaces);
	}
	return iface;
}

static int placeCall(const char *key, void *item, void *arg)
{
	//key - actionid
	//item - message
	//arg - interface
	message_t *message = (message_t*)item;
	assert(message);
	//iface_t *iface = NULL;
	//if (arg) {
	//	iface = (iface_t*)arg;
	//}
	
	if (message->routed_via) //no need to touch if already routed
	{
		log_debug("fabric", "message [%s] already routed", message->actionid);
		return 0;
    } else {
		log_debug("fabric", "message [%s] need to be routed", message->actionid);
		if (!message->iface)
		{
			log_debug("fabric", "first call [%s]", message->actionid);
			message->iface = check_interface(message->module, message->function, message->version);
			if (!message->iface) 
			{
				log_debug("fabric", "no suitable interface for [%s]", message->actionid);
				return -1;
			} else {
				log_debug("fabric", "interface [%p] for [%s] assigned", message->iface, message->actionid);
			}
			if (message->local) 
			{
				if (message->iface->id_global_list && 
					zlist_size(message->iface->id_global_list)>0) //make a copy of global list from interface
				{
					message->globals = zlist_new();
					zlist_autofree(message->globals);
					char *fab = (char*)zlist_first(message->iface->id_global_list);
					while (fab)
					{
						zlist_append(message->globals, (char*)fab);
						fab = (char*)zlist_next(message->iface->id_global_list);
					}
					log_debug("fabric", "globals appended to [%s] size [%ld]", 
						message->actionid, zlist_size(message->globals));
					zlist_first(message->globals);//set the cursor on head
				}
			}
		} 
		//if (message->iface == iface)
		if (message->iface)
		{
			if (zlist_size(message->iface->id_local_list)>0) 
			{
				//try to find non-busy module
				char *id = (char*)zlist_first(message->iface->id_local_list);
				while(id)
				{
					log_debug("fabric", "check module [%s] for state", id); 
					module_t *mod = zhash_lookup(proxy->modules, id);
					if (mod->state == IDLE)
					{
						message->routed_via = mod->identity;
						log_debug("fabric", "module [%s] state IDLE, will be used for processing [%s]", 
								id, message->actionid); 
					}
					id = (char*)zlist_next(message->iface->id_global_list);
				}
			} else {
				if ( message->local &&
					 message->globals && 
				     zlist_size(message->globals)>0 ) 
				{
					message->routed_via = (char*)zlist_next(message->globals);
					if (!message->routed_via)
					{
						message->routed_via = (char*)zlist_first(message->globals);
					}
					if (message->routed_via) 
					{
						log_debug("fabric", "message [%s] will be routed globally to [%s]", 
							message->actionid, message->routed_via);
					} else {
						log_debug("fabric", "message [%s] have no globals left", 
							message->actionid);
					}
				} else {
					log_debug("fabric", "message [%s] cannot be routed now, no free locals or globals", 
							message->actionid);
				}
			}
			//send routed message
			if (message->routed_via) 
			{
				 log_tnx(message->actionid, message->identity, message->routed_via, \
					"%s CALL routed", 
					(message->local)?"local":"global");
                module_t *mod = zhash_lookup(proxy->modules, message->routed_via);
                if (mod && (mod->state==IDLE)) {
					mod->state = BUSY;
					log_mod(mod->name, mod->pid, mod->identity, \
							"module state changed IDLE=>BUSY");
					zmsg_t *msg = message_tomsg(message);
					message->process_timestamp = getTimeStamp();
					send_message(&msg, message->routed_via);
				} else { //looks like should be routed globally
					zmsg_t *msg = message_tomsg(message);
					message->out_timestamp = getTimeStamp();
					send_message(&msg, message->routed_via);
				}
			} 
		} else {
			log_debug("fabric", "message [%s] still have no iface", 
				message->actionid);
		}
	}
	return 0;
}

//Queue process

static int process_queue_item(const char *key, void *item, void *arg)
{

    message_t *message = (message_t*)item;
	zmsg_t *msg = NULL;
	//in timeout - message put in queue
	if ((getInTimeDelta(message) > conf->in_timeout * 1000) && (message->process_timestamp==0))
	{
			log_tnx(message->actionid, "-", message->identity, \
                    "EXCEPTION - cannot placed in %ld ms",
                    conf->in_timeout);
			msg = message_exception(message->actionid, "All modules are busy", MODULES_BUSY);
	}
	//out timeout - messages sent to fabric", "replace call
	if ((message->out_timestamp!=0) && 
		(getOutTimeDelta(message) > conf->out_timeout * 1000) && 
		 message->routed_via)
	{
			log_tnx(message->actionid, "-", message->identity, \
							"EXCEPTION - cannot return from %s in time %d", \
							message->routed_via, conf->out_timeout);
			msg = message_exception(message->actionid, "Global fabric not answered in time", GLOBAL_TIMEOUT);
	}
	//proc timeout
	if ((message->process_timestamp!=0) && 
	     (getProcessTimeDelta(message) > conf->process_timeout * 1000) && 
	     message->routed_via!=NULL)
	{
			module_t* module = (module_t*)zhash_lookup(proxy->modules, message->routed_via);
			log_tnx(message->actionid, "-", message->identity, \
							"EXCEPTION - cannot return from %s in time %d", \
							message->routed_via, conf->out_timeout);
			msg = message_exception(message->actionid, "Process timeout", PROCESS_TIMEOUT);
			if (module && !conf->debug)
			{//assuming module is stuck
				log_mod(module->name, module->pid, module->identity, \
						"module not answer, restart");
				zstr_sendx(proxy->procman, "restart", module->s_pid, NULL);
                iface_remove(proxy->ifaces, module->identity);
				iface_t *iface = (iface_t*)zlist_first(proxy->ifaces);
				while (iface)
				{
					iface_print(iface);
					iface = (iface_t*)zlist_next(proxy->ifaces);
				}
				zhash_delete(proxy->modules, module->identity);
			}
	}
	
	if (msg) {
		send_message(&msg, message->identity);
		zhash_delete(proxy->queue, key);
		log_info("fabric", "queue size [%d]", zhash_size(proxy->queue));
	} //else no need to do something
    
    return 0;
}

static int process_queue (zloop_t *loop, int timer_id, void *arg)
{
	int rc = 0;
	if (zhash_size(proxy->queue)>0) {
		rc = zhash_foreach(proxy->queue, process_queue_item, NULL);
	}
	return rc;
}

//Module list event
static int getIfaceList_event_item (const char *key, void *item, void *arg)
{
    fabric_t *fab = (fabric_t *) item;
    if (!fab) {
        return 0;
    }

    proxy_t *self = (proxy_t *) arg;
    assert (self);

    char *actionid = getActionid();
    zmsg_t *msg = message_ifacelist_request(actionid);
    free(actionid);

    if (fab->alive) {
        log_debug( "fabric", "fab [%s] will be asked for module list", key);
        send_message(&msg, fab->identity);
        fab->alive = false;
    } else {
        log_info( "fabric", "fab [%s] didn't asked for prev poll, delete", key);
        zsocket_disconnect(fab->peer, "%s", key);
        zsocket_destroy(self->ctx, fab->peer);
        iface_remove(self->ifaces, fab->identity);
        iface_t *iface = (iface_t*)zlist_first(proxy->ifaces);
		while (iface)
		{
			iface_print(iface);
			iface = (iface_t*)zlist_next(proxy->ifaces);
		}
        zhash_delete(self->fab_list, key);
    }
    zmsg_destroy(&msg);
    return 0;
}

static int getIfaceList_event(zloop_t *loop, int timer_id, void *arg)
{

    proxy_t *self = (proxy_t*)arg;
    assert(self);
    assert(loop);
    log_debug("fabric", "getmodulelist event - fab list size [%d]", zhash_size(self->fab_list));
    if (zhash_size(self->fab_list)>0)
    {
        zhash_foreach (self->fab_list, getIfaceList_event_item, arg);
    }
    log_debug("fabric", "getmodulelist event complete", zhash_size(self->fab_list));

    return 0;
}

//Command processing
void processCommand(message_t* message)
{
    if (message->command)
    {
        if (streq(message->command, "request")) {
            processRequest(message);
            return;
        }

        if (streq(message->command, "result")) {
            processResult(message);
            return;
        }

        if (streq(message->command, "call")) {
            processCall(message);
            return;
        }

        if (streq(message->command, "exception")) {
            processException(message);
            return;
        }

        if (streq(message->command, "introduce")) {
            processIntroduce(message);
            return;
        }

        if (streq(message->command, "ping")) {
            //processPing(message, self);
            return;
        }

        if (streq(message->command, "pong")) {
            //processPong(message, self);
            return;
        }

        if (streq(message->command, "message")) {
            processMessage(message);
            return;
        }

        if (streq(message->command, "stop")) {
            processStop(message);
            return;
        }

    } else {
        log_err("fabric", "unknown message command [%s]", message->actionid);
        message_destroy(message);
    }
}

void processIntroduce(message_t* message)
{
    char *name;
    pid_t pid;
    module_t *module;

    log_info("fabric", "introduce received from [%s]", message->identity);
    name = message_getName(message);
    if (!name) {
        log_err("fabric", "module [%s] have no name, skip", message->identity);
        message_destroy(message);
        return;
    }

    pid = message_getPid(message);
    if (pid == 0) {
        log_err("fabric", "module [%s] have no pid, skip", message->identity);
        message_destroy(message);
        free(name);
        return;
    }
    zstr_send(proxy->procman, "check_pid");
    zstr_sendf(proxy->procman, "%d", pid);
	char *proc_com = zstr_recv(proxy->procman);
	char *proc_answer = zstr_recv(proxy->procman);
	if (!conf->debug) 
	{
		if (streq(proc_answer, "no"))
		{
			log_err("fabric", "can't process [%s] - unknown pid [%ld]",name, pid);
			free(name);
			free(proc_answer);
			free(proc_com);
			return;
		}
	}
	free(proc_answer);
	free(proc_com);
	
    module = module_new(message->identity, name);
    free(name);
    if (module)
    {
        module->pid = pid;
        sprintf(module->s_pid, "%ld", (long int)pid);
        module->desc = message_getDesc(message);
        module->provides = message_getProvides(message);
        log_mod(module->name, module->pid, module->identity, \
							"new module created");
        if (module->provides) 
        {
            zlist_t *functions = zhash_keys(module->provides);
            if (functions && zlist_size(functions)>0) 
            {
                char *function = (char*)zlist_next(functions);
                while (function)
                {
                    uint64_t *ver = (uint64_t*)zhash_lookup(module->provides, function);
                    log_mod(module->name, module->pid, module->identity, \
							"[%s:%lld] ", function, *ver);
					iface_t *iface = check_interface(module->name, function, *ver);
					if (!iface) {
						iface = iface_new(module->name, function, *ver);
						zlist_append(proxy->ifaces, (void*)iface);
						log_mod(module->name, module->pid, module->identity, \
							"new interface [%p] created [%s:%s:%d] for [%s]", 
							 iface, module->name, iface->func, iface->ver, module->identity);
					} else {
					     log_mod(module->name, module->pid, module->identity, \
							"appended to [%p] interface [%s:%s:%d]=[l%d:g%d] for [%s]", 
					    iface, module->name, iface->func, iface->ver,
					     zlist_size(iface->id_local_list), zlist_size(iface->id_global_list), module->identity); 
					}
					iface_append_local(iface, module->identity);
					iface_print(iface);
					function = (char*)zlist_next(functions);
                }
                //tell procman we have new module 
                
				zstr_send(proxy->procman, "register");
				zstr_send(proxy->procman, module->s_pid);
				zstr_send(proxy->procman, module->name);
				
                //insert it into module list
                zhash_insert(proxy->modules, module->identity, (void*)module);
                zhash_freefn(proxy->modules, module->identity, free_module);
                //send ok to module
                zmsg_t *resp = message_response("ok", message->actionid);
                send_message(&resp, message->identity);
                zlist_destroy(&functions);
                module->state = IDLE;
                 log_mod(module->name, module->pid, module->identity, \
							"state IDLE");
            } else {
				log_err("module [%s] have empty provides list", module->name );
			}
		} else {
			log_err("module [%s] have no provides info", module->name );
		}
    }
    message_destroy(message);
}

void processRequest(message_t* message)
{

    char *command = message_getReqCommand(message);
    if (!command) {
        log_err("fabric", "no request command in [%s]", message->identity);
        message_destroy(message);
        return;
    }
    if (streq(command, "IfaceList"))
    {
        processReq_IfaceList(message);
    } else {
        log_err("fabric", "unknown request command in [%s]", message->identity);
    }
    if (command)
        free(command);
    message_destroy(message);
}

void processException(message_t* message)
{
    message_t *stored = (message_t*)zhash_lookup(proxy->queue, message->actionid);
    
    if (stored) //call here
    { 
		log_tnx(message->actionid, message->identity, stored->identity, \
						"EXCEPTION");
		if (message->local && stored->local) //Module->Module
		{
			zmsg_t *msg = message_tomsg(message);
			assert(msg);
			module_t *mod = zhash_lookup(proxy->modules, message->identity);
			if (mod) 
			{ 
				send_message(&msg, stored->identity);
				iface_t *iface = stored->iface;
				zhash_delete(proxy->queue, stored->actionid);
				if (mod->need_restart)
				{
					zstr_sendx(proxy->procman, "restart", mod->s_pid, NULL);
				} else {
					mod->state = IDLE;
					log_mod(mod->name, mod->pid, mod->identity, \
						"module state changed BUSY=>IDLE");
					//zlist_append(iface->id_queue, mod->identity);
					zhash_foreach(proxy->queue, placeCall, (void*)iface); 
				}
			} else {
				log_err("fabric", "unknown module [%s]", message->identity);
			}
		} else
		if (!message->local && stored->local ) //Fabric->Module
		{
			zmsg_t *msg = message_tomsg(message);
			assert(msg);
			int code = message_getExceptionCode(message);
			if (code == MODULES_BUSY)
			{ //need to replace call
				log_tnx(message->actionid, message->identity, stored->identity, \
				"REPLACEMENT - modules busy");
				message->routed_via = NULL;
				message->out_timestamp = 0;
				placeCall(NULL, stored, stored->iface);
			} else { 
				//pass exception to caller module
				send_message(&msg, stored->identity);
				zhash_delete(proxy->queue, stored->actionid);
			}
		} else
		if (message->local && !stored->local) //Module->Fabric
		{   
			zmsg_t *msg = message_tomsg(message);
			assert(msg);
			fabric_t *fab = (fabric_t*)zhash_lookup(proxy->fab_list, stored->identity);
			iface_t *iface = stored->iface;
			if (fab) 
			{
				send_message(&msg, fab->identity);
			} else {
				log_err("fabric", "no fab [%s] for exception [%s]", stored->identity, message->actionid);
			}
			zhash_delete(proxy->queue, stored->actionid);
			module_t *mod = (module_t*)zhash_lookup(proxy->modules, message->identity);
			if (mod) { //put module back to idlers
				if (mod->need_restart)
				{
					zstr_sendx(proxy->procman, "restart", mod->s_pid, NULL);
				} else {
					mod->state = IDLE;
					log_mod(mod->name, mod->pid, mod->identity, \
						"module state changed BUSY=>IDLE");
					//zlist_append(iface->id_queue, mod->identity);
					zhash_foreach(proxy->queue, placeCall, (void*)iface);
				}
			} else {
				log_err("fabric", "unknown module [%s]", message->identity);
			}
		} else
		if (!message->local && !stored->local) //Fabric->fabric", "wrong
		{
			log_err("fabric", "invalid state [%] for [%s-%s]", message->actionid, stored->identity, message->identity);
			zhash_delete(proxy->queue, stored->actionid);
		}
        log_info("fabric", "queue size [%d]", zhash_size(proxy->queue));
        message_destroy(message);
        return;
    } else {
		log_info( "fabric", "unknown exception from [%s] received, message may be timeout: [%s]",
                message->identity,
                message_getExceptionString(message));
	}
	message_destroy(message);
}

void processResult(message_t* message)
{
    message_t *stored = (message_t*)zhash_lookup(proxy->queue, message->actionid);

    if (stored) //call here
    { 
		log_tnx(message->actionid, message->identity, stored->identity, \
						"RESULT");
		if (message->local && stored->local) //Module->Module
		{
			zmsg_t *msg = message_tomsg(message);
			send_message(&msg, stored->identity);
			iface_t *iface = stored->iface;
			zhash_delete(proxy->queue, stored->actionid);
			module_t *mod = (module_t*)zhash_lookup(proxy->modules, message->identity);
			if (mod) { //put module back to idlers
				if (mod->need_restart)
				{
					zstr_sendx(proxy->procman, "restart", mod->s_pid, NULL);
				} else {
					mod->state = IDLE;
					log_mod(mod->name, mod->pid, mod->identity, \
						"module state changed BUSY=>IDLE");
					//zlist_append(iface->id_queue, mod->identity);
					zhash_foreach(proxy->queue, placeCall, (void*)iface);
				}
			} else {
				log_err("fabric", "unknown module [%s]", message->identity);
			}
		} else
		if (!message->local && stored->local ) //Fabric->Module
		{

			zmsg_t *msg = message_tomsg(message);
			assert(msg);
			send_message(&msg, stored->identity);
			zhash_delete(proxy->queue, stored->actionid);
			
		} else 
		if (message->local && !stored->local)   //Module->Fabric
		{ 
			zmsg_t *msg = message_tomsg(message);
			assert(msg);
			fabric_t *fab = (fabric_t*)zhash_lookup(proxy->fab_list, stored->identity);
			iface_t *iface = stored->iface;
			if (fab) 
			{
				send_message(&msg, fab->identity);
			} else {
				log_err("fabric", "no fab [%s] for result [%s]", stored->identity, message->actionid);
			}
			zhash_delete(proxy->queue, stored->actionid);
			module_t *mod = (module_t*)zhash_lookup(proxy->modules, message->identity);
			if (mod) { //put module back to idlers
				if (mod->need_restart)
				{
					zstr_sendx(proxy->procman, "restart", mod->s_pid, NULL);
				} else {
					mod->state = IDLE;
					log_mod(mod->name, mod->pid, mod->identity, \
						"module state changed BUSY=>IDLE");
					
					//zlist_append(iface->id_queue, mod->identity);
					zhash_foreach(proxy->queue, placeCall, (void*)iface); 
				}
			} else {
				log_err("fabric", "unknown module [%s]", message->identity);
			}
		} else
		if (!message->local && !stored->local) //Fabric->fabric", "wrong
		{
			log_err("fabric", "invalid state [%] for [%s-%s]", 
					message->actionid, stored->identity, message->identity);
			zhash_delete(proxy->queue, stored->actionid);
		}
        log_info("fabric", "queue size [%d]", zhash_size(proxy->queue));
        message_destroy(message);
        return;
    } else { //assume it is a IfaceList
		log_info( "fabric", "IfaceList result actionid [%s]", message->actionid);
	    zlist_t *iflist = message_getIfaceList(message);
        if (iflist) 
        {
            fabric_t *fab = zhash_lookup(proxy->fab_list, message->identity);
            if (!fab) 
            {
                log_err( "fabric", "unknown fab IfaceList [%s]:", fab->identity);
            } else {
	            if (zlist_size(iflist)>0)
	            {
	                log_info( "fabric", "IfaceList result detected from [%s]", fab->identity);
					char *iface_path = (char*)zlist_first(iflist);
					while (iface_path) {
						char *module = strtok(iface_path, ":");
						char *function = strtok(NULL, ":");
						char *ver	   = strtok(NULL, ":");
						if (module && function && ver) 
						{
							iface_t *iface = check_interface(module, function, atoi(ver));
							if (!iface) {
								iface = iface_new(module, function, atoi(ver));
								zlist_append(proxy->ifaces, (void*)iface);
								log_info("fabric", "for fabric [%s] new interface created [%s:%s:%d]", 
									fab->identity, iface->mod, iface->func, iface->ver);
							} 
							//check iface already in list
							if (iface_append_global(iface, fab->identity)) 
							{
								log_info("fabric", "fabric [%s] appended to interface [%s:%s:%d]", 
									fab->identity, iface->mod, iface->func, iface->ver); 
							}
							iface_print(iface);
						} else {
							log_err( "fabric", "unknown interface [%s]:", iface_path);
						}
						iface_path = (char*)zlist_next(iflist);
					}
	            }
	      }
	      zlist_destroy(&iflist);
        } else {
			log_err( "fabric", "result actionid [%s] unknown, message may be timeout", message->actionid);
		}
    }
    message_destroy(message);

}

void processCall(message_t* message)
{

    message->version = message_getIfVersion(message);
    message->module = message_getModule(message);
    message->function = message_getFunc(message);

    if (!message->module || !message->function || message->version == 0) {
        log_err( "fabric", "CALL [%s] have wrong structure, skip", message->identity);
        zmsg_t *msg = message_exception(message->actionid, "Wrong CALL structure", 21);
        send_message(&msg, message->identity);
        message_destroy(message);
        return;
    }

    log_tnx(message->actionid, message->identity, "-", \
			   "received %s CALL [%s:%s:%d]", 
				(message->local)?"local":"global",
				message->module,
				message->function,
				message->version);
    
	int rc = 0;
	rc = placeCall(NULL, message, NULL);
	if (rc != -1 )
	{
		if (!message->local && !message->routed_via)
		{//global messages should be routed immediatly, else exception
			log_info("fabric", "global message [%s] cannot be placed locally, return exception", message->actionid);
			zmsg_t *msg = message_exception(message->actionid, "No modules found", MODULES_BUSY);
			send_message(&msg, message->identity);
			message_destroy(message);
			return;
		}
		zhash_insert(proxy->queue, message->actionid, message);
		zhash_freefn(proxy->queue, message->actionid, free_message);
		log_info("fabric", "call [%s] added to queue, queue size [%d]", message->actionid, zhash_size(proxy->queue));
	} else {
		zmsg_t *msg = message_exception(message->actionid, "No modules found", MODULES_BUSY);
		log_tnx(message->actionid, "-", message->identity, \
			   "exception for %s CALL %s - no modules found", 
				(message->local)?"local":"global",
				message->module);
		send_message(&msg, message->identity);
		message_destroy(message);		
	}
 }

void processMessage(message_t* message)
{
	module_t *mod = (module_t*)zhash_lookup(proxy->modules, message->identity);
	char *text = message_getMessage(message);
	if (mod) {
		log_msg (mod->name, mod->pid, mod->identity, \
			text);
    }
    message_destroy(message);
}

void processStop(message_t* message)
{
    //TODO
}

void processReq_IfaceList(message_t* message)
{
   
	char *actionid = getActionid();
		
	fabric_t *fab = zhash_lookup(proxy->fab_list, message->identity);
	if (!fab) {
		log_err("fabric", "cannot find fabric [%s]", message->identity);
	} else {
		zmsg_t *msg = message_ifacelist_response(proxy->ifaces, message->actionid);
		send_message(&msg, fab->identity);
	}
	free(actionid);
}

//sockets events
static int s_global_event(zloop_t *loop, zmq_pollitem_t *item, void *arg)
{

    assert(arg);
    assert(loop);
    log_debug( "fabric", "GLOBAL_EVENT ");
    proxy_t *self = (proxy_t *) arg;
    assert (self);

    zmsg_t *msg = NULL;
    msg = zmsg_recv(item->socket);

    if (!msg) {
		log_err("fabric", "no msg on global socket");
        return 0;
    }
	
    message_t* message = NULL;
    message = message_fromzmq(msg, false);

    if (!message)
    {
        log_err( "fabric", "problem parsing GLOBAL message, skipping");
        zmsg_dump(msg);
        zmsg_destroy(&msg);
        return 0;
    }
    log_debug( "fabric", "message from [%s] received", message->identity);
	if (conf->log_level == MOD_LOG_DEBUG) {
		fprintf(stdout,"GLOBAL>");
		message_dump(message);
	}
    fabric_t *fab = zhash_lookup(self->fab_list, message->identity);
    if (fab)
        fab->alive = true;
	zmsg_destroy(&msg);
    processCommand(message);
    return 0;
}

static int s_local_event(zloop_t *loop, zmq_pollitem_t *item, void *arg)
{

    assert(arg);
    assert(loop);
    log_debug( "fabric", "LOCAL_EVENT ");
    proxy_t *self = (proxy_t *) arg;
    assert (self);

    zmsg_t *msg = NULL;
    msg = zmsg_recv(item->socket);

    if (!msg) {
        log_err("fabric", "no msg on local socket");
        return 0;
    }

    message_t* message = NULL;
    message = message_fromzmq(msg, true);
    

    if (!message)
    {
		log_err( "fabric", "problem parsing LOCAL message, skipping");
		zmsg_dump(msg);
		zmsg_destroy(&msg);
        return 0;
    } else {
		log_debug("fabric", "got message [%s] from [%s]", message->actionid, message->identity);
		if (conf->log_level == MOD_LOG_DEBUG) 
		{
			fprintf(stdout,"LOCAL>");
			message_dump(message);
		}
		zmsg_destroy(&msg);
	}
    processCommand(message);
    return 0;
}

static int procman_checkpid(const char *key, void *item, void *arg)
{
	procman_command *com = (procman_command*)arg;
	module_t *mod = (module_t*)item;
	assert(com);
	
	if (mod && mod->pid == com->pid) {
		if (streq(com->command, "remove")) 
	    {
			log_mod (mod->name, mod->pid, mod->identity, \
					"not running, remove");
			iface_remove(proxy->ifaces, mod->identity);
			iface_t *iface = (iface_t*)zlist_first(proxy->ifaces);
			while (iface)
			{
				iface_print(iface);
				iface = (iface_t*)zlist_next(proxy->ifaces);
			}
			zhash_delete(proxy->modules, mod->identity);
	    }
	    if (streq(com->command, "restart")) 
	    {
			if (mod->state == IDLE) 
			{
				mod->state = RESTARTING;
				log_mod (mod->name, mod->pid, mod->identity, \
					"restart now");
				zstr_sendx(proxy->procman, "restart", mod->s_pid, NULL);
				
			} else {
				log_mod (mod->name, mod->pid, mod->identity, \
					"restart sheduled");
				mod->need_restart = true;
			}
	    }
	    return -1;
	}
	return 0;
}

static int s_procman_event(zloop_t *loop, zmq_pollitem_t *item, void *arg)
{
	assert(arg);
    assert(loop);
    log_debug( "fabric", "PROCMAN_EVENT ");
    proxy_t *self = (proxy_t *) arg;
    assert (self);
    
    procman_command *com = (procman_command*)zmalloc(sizeof(procman_command));
    com->command = zstr_recv(item->socket);
    char *c_pid = zstr_recv(item->socket);
    com->pid = (pid_t)atoi(c_pid);
    free(c_pid);
	
    if (!com->pid || !com->command) {
		log_err("fabric", "no command or pid");
        return 0;
    }
    log_debug( "fabric", "procman send [%s - %ld] ", com->command, com->pid);
    zhash_foreach(self->modules, procman_checkpid, (void*)com);
    free(com);
	return 0;
}

static void createPidFile(pid_t pid, config_t *conf)
{
    char buf[10];

    /*try to open pid file*/
    f_lock = open(conf->pid_file, O_RDWR|O_CREAT|O_EXCL, 0640);
    /*this file is exist*/
    if (f_lock == -1)
    {
        zclock_log( "unable to create pid-file %s, code=%d (%s)",
                    conf->pid_file, errno, strerror(errno) );
        exit(1);
    } else
    {
        zclock_log( "new pid-file created %s", conf->pid_file);
    }
    /*lock pid file*/
    if (lockf(f_lock,F_TEST,0)<0) {
        zclock_log( "unable to lock pid-file, code=%d (%s)",
                    errno, strerror(errno) );
        exit(1);
    } else {
        if (lockf(f_lock,F_TLOCK,0)==0) {
            zclock_log( "pid-file locked");
        } else {
            zclock_log( "unable to lock pid-file, code=%d (%s)",
                        errno, strerror(errno) );
            exit(1);
        }
    }
    /*write pid to pid file*/
    snprintf(buf, 10, "%ld", (long)pid);
    if (write(f_lock, buf, strlen(buf)) != strlen(buf)) {
        zclock_log( "unable to write pid-file %s, code=%d (%s)",
                    conf->pid_file, errno, strerror(errno) );
    } else {
        zclock_log( "PID %s writed to pid-file %s", buf,
                    conf->pid_file);
    }
}

static void daemonize(config_t *conf)
{
    pid_t pid, sid;

    /* already a daemon */
    if ( getppid() == 1 ) return;

    /* Drop user if there is one, and we were run as root */
    if ((getuid() == 0 || geteuid() == 0)  && !streq(conf->runas,"")) {
        struct passwd *pw = getpwnam(conf->runas);
        if ( pw ) {
            zclock_log("setting user to %s", conf->runas );
            int rc = setuid( pw->pw_uid );
            if (!rc) 
				zclock_log("unable to set user");
        }
    }

    /* Trap signals that we expect to recieve */

    signal(SIGCHLD,parent_handler);
    signal(SIGUSR1,parent_handler);
    signal(SIGALRM,parent_handler);

    /* Fork off the parent process */
    pid = fork();
    if (pid < 0) {
        zclock_log("unable to fork daemon, code=%d (%s)",
                   errno, strerror(errno) );
        exit(EXIT_FAILURE);
    }

    /* If we got a good PID, then we can exit the parent process. */
    if (pid > 0) {
        /* Wait for confirmation from the child via SIGTERM or SIGCHLD, or
           for two seconds to elapse (SIGALRM).  pause() should not return. */
        zclock_log( "successful forked daemon with pid %i",pid);
        alarm(2);
        pause();
        exit(EXIT_SUCCESS);
    }
    /* At this point we are executing as the child process */
    fab_pid = getpid();
    createPidFile(fab_pid, conf);

    /* Change the file mode mask */
    umask(0);

    /* Create a new SID for the child process */
    sid = setsid();
    if (sid < 0) {
        zclock_log( "unable to create a new session, code %d (%s)",
                    errno, strerror(errno) );
        finalize(EXIT_FAILURE);
    }

   
    /* Redirect standard files to /dev/null */
    FILE *in, *out, *err;
    in = freopen( "/dev/null", "r", stdin);
    out = freopen( "/dev/null", "w", stdout);
    err = freopen( "/dev/null", "w", stderr);
    if (in  || err || out) {
        zclock_log("warning, error to redirect std streams");
    }
    /* Tell the parent process that all ok */
    //kill(parent,SIGUSR1);
}

int execute(config_t *conf)
{
    int rc = 0;
	zclock_log( "fabric starting as %s", conf->s_name);
    proxy = (proxy_t *) zmalloc (sizeof (proxy_t));
    assert(proxy);
    //Initialization
    
    //CONFIG
    assert(conf);
    proxy->conf = conf;

    //CONTEXT
    proxy->ctx = zctx_new ();
    assert(proxy->ctx);
    zctx_set_iothreads (proxy->ctx, 1);

	logger_init(proxy->ctx, conf);

    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags=0;
    sa.sa_handler = s_signal_handler;
    sigaction(SIGHUP, &sa, 0);
    sigaction(SIGINT, &sa, 0);
    sigaction(SIGQUIT, &sa, 0);
    sigaction(SIGABRT, &sa, 0);
    sigaction(SIGTERM, &sa, 0);

    //GLOABL FAB LIST
    proxy->fab_list = zhash_new();
    assert(proxy->fab_list);

    //LOCAL MODULES
    proxy->modules = zhash_new();
    assert(proxy->modules);

    //LOCAL_QUEUE
    proxy->queue = zhash_new();
    assert(proxy->queue);

	//LOCAL INTERFACES
	proxy->ifaces = zlist_new();
	assert(proxy->ifaces);
	
    //PROCESS MANAGER init
    proxy->procman = zthread_fork(proxy->ctx, procman_thread, proxy->conf);
    assert(proxy->procman);


	proxy->main_loop = zloop_new ();
    assert ( proxy->main_loop);
    zloop_set_verbose ( proxy->main_loop, DEBUG);
	
	//GLOBAL socket init
	if (conf->global) 
    {
	    //UDP socket init
	    proxy->udp = zthread_fork(proxy->ctx, beacon_thread, NULL);
	    assert(proxy->udp);
	
	    //GLOBAL socket init
	    proxy->global_fe = zsocket_new(proxy->ctx, ZMQ_ROUTER);
	    assert(proxy->global_fe);
	
	    //get local IP as identity for GLOBAL socket
	    char* self_ip = zstr_recv(proxy->udp);
	    assert(self_ip);
	    
	    //End init
	
	    int port = ZSOCKET_DYNFROM;
	    int self_port = -1;
	    char bind_point[64];
	
	    //Bind to tcp endpoint
	    while (port <= ZSOCKET_DYNTO && self_port == -1)
	    {
	
	        sprintf(&bind_point[0], "tcp://%s:%d",self_ip, port++);
	        zsocket_set_identity(proxy->global_fe, &bind_point[0]);
	        //some magic pass
	        zsocket_set_router_mandatory (proxy->global_fe, 1);
	        //zsocket_set_linger(proxy->global_fe, 0);
	        self_port = zsocket_bind(proxy->global_fe, "%s",bind_point);
	    }
	    if (self_port == -1)
	    {
	        log_err( "fabric", "socket global_fe binding error [tcp://%s:*] - [%s]",
	                    self_ip, zmq_strerror (errno));
	        exit(EXIT_FAILURE);
	    }
	
	    //send to UDP binding info
	    zstr_send(proxy->udp, &bind_point[0]);
	
	    char *global_identity = zsocket_identity(proxy->global_fe);
	
	    log_info( "fabric", "socket GLOBAL binded at [tcp://%s:%d] with identity [%s]",
	                self_ip, self_port, global_identity);
		
	    free(global_identity);
		free(self_ip);
		zmq_pollitem_t poll_beacon = { proxy->udp,  0, ZMQ_POLLIN, 0 };
		zmq_pollitem_t poll_global = { proxy->global_fe,  0, ZMQ_POLLIN, 0 };
		zloop_timer ( proxy->main_loop, conf->modlist_poll, 0, getIfaceList_event, proxy);
		rc = zloop_poller ( proxy->main_loop, &poll_beacon, s_beacon_event, proxy);
		assert (rc == 0);
		rc = zloop_poller ( proxy->main_loop, &poll_global, s_global_event, proxy);
		assert (rc == 0);
	} else {
		log_info("fabric", "modbus fabric [%s] global mode disabled", conf->s_name);
	}
    //LOCAL socket init
    {
        proxy->local_fe = zsocket_new(proxy->ctx, ZMQ_ROUTER);
        if (!proxy->local_fe) {
            log_err("fabric", "error creating LOCAL socket [%s]", conf->socket);
            return -1;
        }
        rc = zsocket_bind(proxy->local_fe, "%s", conf->socket);
        if (rc == -1) {
            log_err("fabric", "error binding LOCAL socket [%s]", zmq_strerror (errno));
            return -1;
        } else {
            log_info("fabric", "binding LOCAL socket [%s] ok", conf->socket);
            struct stat st;
            if((0!=stat( conf->socket_file, &st)) ||
			   (0!=chmod(conf->socket_file, conf->socket_mode))) 
			   {
					log_err("fabric", "mode LOCAL socket [%s] change failed", conf->socket);
				} else {
					log_info("fabric", "mode LOCAL socket [%s] change ok", conf->socket);
			}
		
			struct passwd *usr;
			struct group *grp;
			usr = getpwnam(conf->socket_user);
			grp = getgrnam(conf->socket_group);

			if ((grp != 0) && (usr != 0) && getuid() == 0) {
				if( ( 0 != chown(conf->socket_file, usr->pw_uid, grp->gr_gid) ) ) {
					log_err("fabric", "ownership LOCAL socket [%s] change failed", conf->socket);
				} else {
					log_info("fabric", "ownership LOCAL socket [%s] change ok", conf->socket);
				}
			} else {
				log_info("fabric", "not valid user:group or run not under root", conf->socket);
			}
		}
		
    }
    //end LOCAL init

    zmq_pollitem_t poll_local  = { proxy->local_fe, 0, ZMQ_POLLIN, 0 };
    zmq_pollitem_t poll_procman  = { proxy->procman, 0, ZMQ_POLLIN, 0 };

    //prepare zloop reactor
    zloop_timer ( proxy->main_loop, conf->queue_poll, 0, process_queue, proxy);
    
    rc = zloop_poller ( proxy->main_loop, &poll_local, s_local_event, proxy);
    assert (rc == 0);
    rc = zloop_poller ( proxy->main_loop, &poll_procman, s_procman_event, proxy);
    assert (rc == 0);

    log_info("fabric", "modbus fabric [%s] started", conf->s_name);
    if (conf->global) 
    {
		zstr_send(proxy->udp, "start");
	} 
    zstr_send(proxy->procman, "startall");
    /*setup complete*/
    zloop_start ( proxy->main_loop);
    finalize(rc);
    return(EXIT_SUCCESS);
}

int main (int argc, char *argv [])
{
    int rc = 0;

    char *file = (argc > 1)? argv [1]: "";
    if (streq(file,""))
    {
        zclock_log("cannot start modbusd for %s", file);
        exit(-1);
    }
    conf = config_new(file);
    if (!conf) {
        zclock_log("wrong config %s", file);
        exit(-1);
    }
    if (conf->iface)
    {
		zclock_log("fabric will use interface %s", conf->iface);
		zsys_set_interface(conf->iface);
	}
    zclock_log("modbus fabric starting for %s", conf->s_name);
    if (conf->daemon) {
        daemonize(conf);
    } else {
		createPidFile(getpid(), conf);
	}
    rc = execute(conf);
    exit(rc);
}


