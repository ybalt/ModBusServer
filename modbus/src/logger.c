#include "logger.h"

static void     *logger;
static config_t *conf;


int logger_init(zctx_t *fab_ctx, config_t *config)
{
    conf = config;
    logger = zthread_fork(fab_ctx, logger_file_thread, conf);
    assert(logger);
	return 0;
}

static void log_common(char *actionid, \
					   char *identity_from, \
					   char *identity_to, \
					   pid_t pid, \
					   char *name, \
					   char *channel, \
					   char *text)
{
	char *msg = (char*)zmalloc(sizeof(char)*MSGSZ);
	assert(msg);
    
	char            fmt[64], buf[64];
    struct timeval  tv;
    struct tm       *tm;

    gettimeofday(&tv, NULL);
    if((tm = localtime(&tv.tv_sec)) != NULL)
    {
            strftime(fmt, sizeof fmt, "%Y-%m-%d %H:%M:%S.%%03u", tm);
            snprintf(buf, sizeof buf, fmt, tv.tv_usec);
    }
	
    snprintf (msg, MSGSZ-sizeof(buf), "%s;%s;%s;%s;%s;%ld;%s;%s;%s", buf, conf->s_name,
			actionid, identity_from ,identity_to, (long int)pid, name, channel, text);
	
		
	zmsg_t *zmsg = zmsg_new();
	
	zmsg_pushmem (zmsg, msg, strlen(msg));
	zmsg_pushstr(zmsg, "LOG");
	zmsg_send(&zmsg, logger);
	zmsg_destroy(&zmsg);
	free(msg);
}

void log_info(char* name, const char *format, ...)
{
	if (conf->log_level >= MOD_LOG_INFO ) {
		
		char *message = malloc(sizeof(char)*(MSGSZ-128)); //some bytes for prefix
		assert(message);
	
		va_list argptr;
		va_start (argptr, format);
			vsprintf(message, format, argptr);
		va_end (argptr);
		log_common("-", "-", "-", 0, name, "INFO", message);
		free(message);
	}
}
void log_err(char* name, const char *format, ...)
{
	if (conf->log_level >= MOD_LOG_ERR ) {
		char *message = malloc(sizeof(char)*(MSGSZ-128)); //some bytes for prefix
		assert(message);
	
		va_list argptr;
		va_start (argptr, format);
			vsprintf(message, format, argptr);
		va_end (argptr);
		log_common("-", "-","-", 0, name, "ERR", message);
		free(message);
	}
}
void log_debug(char* name, const char *format, ...)
{
	if (conf->log_level >= MOD_LOG_DEBUG ) {
		char *message = malloc(sizeof(char)*(MSGSZ-128)); //some bytes for prefix
		assert(message);
	
		va_list argptr;
		va_start (argptr, format);
			vsprintf(message, format, argptr);
		va_end (argptr);
		log_common("-", "-","-", 0, name, "DEBUG", message);
		free(message);
		
	}

}

void log_mod(char *name, \
			pid_t pid, \
			char *identity, \
			const char *format, ...)
{
	char *message = malloc(sizeof(char)*(MSGSZ-128)); //some bytes for prefix
	assert(message);
	
	va_list argptr;
	va_start (argptr, format);
		vsprintf(message, format, argptr);
	va_end (argptr);
	log_common("-", identity, "-", pid, name, "MOD", message);
	free(message);
}

void log_mod_out(char *name, \
			pid_t pid, \
			char *identity, \
			char *message)
{
	log_common("-", identity, "-", pid, name, "MOD_OUT", message);
	free(message);
}

void log_mod_err(char *name, \
			pid_t pid, \
			char *identity, \
			char *message)
{
	log_common("-", identity, "-", pid, name, "MOD_ERR", message);
	free(message);
}

void log_msg(char *name, \
			pid_t pid, \
			char *identity, \
			char *message)
{
	log_common("-", identity, "-", pid, name, "MSG", message);
	free(message);
}

void log_tnx(char *actionid, \
			  char *identity_from, \
			  char *identity_to, \
			  const char *format, ...)
{
	char *message = malloc(sizeof(char)*(MSGSZ-128)); //some bytes for prefix
	assert(message);
	
	va_list argptr;
	va_start (argptr, format);
		vsprintf(message, format, argptr);
	va_end (argptr);
	log_common(actionid, identity_from, identity_to, 0, "-", "TNX", message);
	free(message);
}


