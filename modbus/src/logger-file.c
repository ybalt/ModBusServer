#include "logger-file.h"

static int  file_count;
static long bytes_writed;
static int  f_log;

int logger_thread_event(zloop_t *loop, zmq_pollitem_t *item, void *arg)
{
	assert(loop);
    assert(item);
    char filename[256];
    config_t *conf = (config_t*)arg;
    zmsg_t *msg = zmsg_recv(item->socket);
    if (!msg)
		return 0;
	char *command = zmsg_popstr(msg);
    if(command)
    {
		if (streq("LOG", command)) 
		{
			char *text = zmsg_popstr(msg);
			if (text) 
			{
				char buf[MSGSZ];
				snprintf(buf, MSGSZ-2, "%s\r\n", text);
				if (f_log > 0) //file log
				{
					/*get file info*/
					struct stat log_stat;
					fstat(f_log, &log_stat);
					/*if size threshold reached - rename work file*/
					if (conf->log_max_size !=0 && log_stat.st_size >= conf->log_max_size*1024*1024) 
					{
						if (streq(conf->log_method, "roll")){
							zclock_log("log file max size threshold reached, will be rolled");
							close(f_log);
							f_log=-1;
							int rc = -1;
							while(rc != 0 && file_count<64)
							{
								sprintf(filename, "%s.%i", conf->log_file, file_count);
								file_count++;
								rc = rename(conf->log_file, filename);
							}
							if (rc == 0)
							{
								zclock_log("log file rolled to %s", filename);
								f_log=open(conf->log_file,O_RDWR|O_CREAT|O_APPEND,conf->log_mode);
								if (f_log>0) {
									bytes_writed += write(f_log, buf, strlen(buf));
								}
							} else {
								zclock_log("unable to rename log file, max files reached");
							}
						} 
						if (streq(conf->log_method,"none"))
						{
							bytes_writed += write(f_log,buf, strlen(buf));
						}
					} else {
						bytes_writed += write(f_log, buf, strlen(buf));
					}
				}
				if (!conf->daemon) //console log
				{
					printf("%s", buf);
				}
				free(text);
			}
		}
			
		free(command);
	} 
	
    zmsg_destroy(&msg);
    return 0;
}

void logger_file_thread(void *args, zctx_t *ctx, void *pipe)
{
	config_t *conf = (config_t*)args;
	file_count = 0;
	f_log = 0;
	zloop_t *loop = zloop_new ();
    assert (loop);
    zloop_set_verbose (loop, 0);
	zmq_pollitem_t poll = { pipe, 0, ZMQ_POLLIN, 0};
    zloop_poller (loop, &poll, logger_thread_event, conf);
    
    f_log=open(conf->log_file,O_RDWR|O_CREAT|O_APPEND,conf->log_mode);
    if (f_log>0)
    {
		zclock_log("log file %s open ok",conf->log_file);
	} else {
		zclock_log("error opening log file %s, code %d (%s)",conf->log_file, errno, strerror(errno));
	}
	zloop_start (loop);
    zloop_destroy (&loop);
    close(f_log);

}
