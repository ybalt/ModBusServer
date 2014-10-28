#ifndef LOGGER_H
#define LOGGER_H

#include <sys/types.h>
#include <stdio.h>
#include <string.h>

#include <czmq.h>
#include "config.h"
#include "logger-file.h"

#define MSGSZ	1024

#define MOD_LOG_ERR   0
#define MOD_LOG_INFO  1
#define MOD_LOG_DEBUG 9

int  logger_init(zctx_t *ctx, config_t *conf);

void log_info(char *name, const char *format, ...);
void log_err(char *name, const char *format, ...);
void log_debug(char *name, const char *format, ...);

void log_mod(char *name, \
			pid_t pid, \
			char *identity, \
			const char *format, ...);
			
void log_msg(char *name, \
			pid_t pid, \
			char *identity, \
			char *message);

void log_mod_out(char *name, \
			pid_t pid, \
			char *identity, \
			char *message);

void log_mod_err(char *name, \
			pid_t pid, \
			char *identity, \
			char *message);

void log_tnx(char *actionid, \
			  char *identity_from, \
			  char *identity_to, \
			  const char *format, ...);

#endif // LOGGER_H
