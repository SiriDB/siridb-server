/*
 * walker.c - Creates enter and exit nodes.
 */
#include <siri/db/walker.h>
#include <siri/err.h>

/*
 * Returns NULL and raises a SIGNAL in case an error has occurred.
 */
siridb_walker_t * siridb_walker_new(
        siridb_t * siridb,
        const uint64_t now,
        uint8_t * flags)
{
    siridb_walker_t * walker =
            (siridb_walker_t *) malloc(sizeof(siridb_walker_t));
    if (walker == NULL)
    {
        ERR_ALLOC
    }
    else
    {
        walker->siridb = siridb;
        walker->now = now;
        walker->flags = flags;
        walker->start = NULL;
        walker->enter_nodes = NULL;
        walker->exit_nodes = NULL;
    }
    return walker;
}

/*
 * Destroy walker. (parsing NULL is NOT allowed)
 */
siridb_nodes_t * siridb_walker_free(siridb_walker_t * walker)
{
    siridb_nodes_t * nodes;
    if (walker->start == NULL)
    {
        nodes = walker->exit_nodes;
    }
    else
    {
        walker->enter_nodes->next = walker->exit_nodes;
        nodes = walker->start;
    }
    free(walker);
    return nodes;
}

/*
 * Returns 0 if successful and -1 in case of an error.
 * (signal is set in case of an error)
 */
int siridb_walker_append(
        siridb_walker_t * walker,
        cleri_node_t * node,
        uv_async_cb cb)
{
    siridb_nodes_t * wnode =
            (siridb_nodes_t *) malloc(sizeof(siridb_nodes_t));
    if (wnode == NULL)
    {
        ERR_ALLOC
        return -1;
    }
    wnode->node = node;
    wnode->cb = cb;
    wnode->next = NULL;

    if (walker->start == NULL)
    {
        walker->start = wnode;
        walker->enter_nodes = wnode;
    }
    else
    {
        walker->enter_nodes->next = wnode;
        walker->enter_nodes = wnode;
    }

    return 0;
}

/*
 * Returns 0 if successful and -1 in case of an error.
 * (signal is set in case of an error)
 */
int siridb_walker_insert(
        siridb_walker_t * walker,
        cleri_node_t * node,
        uv_async_cb cb)
{
    siridb_nodes_t * current = walker->exit_nodes;

    walker->exit_nodes =
            (siridb_nodes_t *) malloc(sizeof(siridb_nodes_t));
    if (walker->exit_nodes == NULL)
    {
        ERR_ALLOC
        /* restore walker node */
        walker->exit_nodes = current;
        return -1;
    }
    walker->exit_nodes->node = node;
    walker->exit_nodes->cb = cb;
    walker->exit_nodes->next = current;

    return 0;
}
