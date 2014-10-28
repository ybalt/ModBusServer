#include "fabric.h"

//initialize fabric
fabric_t *fab_new(char *msg)
{
    fabric_t *fab = (fabric_t *) zmalloc(sizeof(fabric_t));

    //fab->identity = (char*)zmalloc(sizeof(char)*64);
    fab->identity = NULL;
    fab->alive = false;

    char *prefix = strtok(msg, "@");
    if (!prefix)
        return NULL;

    char *identity = strtok(NULL, "@");
    if (!identity)
        return NULL;
    fab->identity = strdup(identity);
    
    return fab;
}

void fab_destroy(fabric_t *fab)
{

    if (fab->identity)
        free(fab->identity);
    free(fab);
    fab = NULL;
}

void free_fab(void *data)
{
    if (data) {
        fabric_t *fab = (fabric_t*)data;
        fab_destroy(fab);
    }
}
