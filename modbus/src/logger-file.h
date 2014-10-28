#ifndef LOGGER_FILE_H
#define LOGGER_FILE_H

#include "czmq.h"
#include "config.h"
#include "logger.h"


void 	logger_file_thread(void *args, zctx_t *ctx, void *pipe);

#endif //LOGGER_FILE_H
