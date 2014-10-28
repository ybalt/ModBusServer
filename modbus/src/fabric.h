#ifndef FABRIC_H
#define FABRIC_H

#include "modbusd.h"

typedef struct
{
    char             *identity;   //fab socket identity
    void             *peer;
    bool              alive;
} fabric_t;

fabric_t    *fab_new(char *msg);
void         fab_destroy(fabric_t *fab);
void         free_fab(void *data);

#endif // FABRIC_H
