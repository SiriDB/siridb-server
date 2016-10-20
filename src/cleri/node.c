/*
 * node.c - node is created while parsing a string. a node old the result
 *          for one element.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 08-03-2016
 *
 */
#include <cleri/node.h>
#include <stdlib.h>

static cleri_node_t Cleri_empty_node = {
        .children=NULL,
        .cl_obj=NULL,
        .len=0,
        .str=NULL
};

cleri_node_t * CLERI_EMPTY_NODE = &Cleri_empty_node;

/*
 * Returns NULL in case an error has occurred.
 */
cleri_node_t * cleri_node_new(
        cleri_object_t * cl_obj,
        const char * str,
        size_t len)
{
    cleri_node_t * node;
    node = (cleri_node_t *) malloc(sizeof(cleri_node_t));

    if (node != NULL)
    {
		node->cl_obj = cl_obj;
		node->ref = 1;

		node->str = str;
		node->len = len;

		if (cl_obj == NULL || cl_obj->tp <= CLERI_TP_THIS)
		{
			/* NULL when initializing the root node but we do need children */
			node->children = cleri_children_new();
			if (node->children == NULL)
			{
				free(node);
				return NULL;
			}
		}
		else
		{
			/* we do not need children for some objects */
			node->children = NULL;
		}
    }
    return node;
}

/*
 * Destroy node. (parsing NULL is allowed)
 */
void cleri_node_free(cleri_node_t * node)
{
    /* node can be NULL or this could be an CLERI_EMPTY_NODE */
    if (node == NULL || node == CLERI_EMPTY_NODE || --node->ref)
    {
        return;
    }
    cleri_children_free(node->children);
    free(node);
}

