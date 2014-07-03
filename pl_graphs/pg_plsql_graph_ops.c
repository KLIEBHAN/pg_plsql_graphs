#include "plpgsql.h"
#include "nodes/pg_list.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <igraph/igraph.h>
#include "pg_plsql_graphs_lib.h"


/**
 * Connect the node with the given id to its parents given as List of nodes
 */
void connectNodeToParents(int nodeid, List* parents){

    ListCell* l;
    /* Iterate over parents */
    foreach(l, parents){
        struct node* parent = lfirst(l);
        /* create new edge */
        struct edge* e = palloc(sizeof(struct edge));
        /* set the source node to the parent */
        e->sourceid = parent->key;
        /* set the target node to the current node */
        e->targetid = nodeid;
        /* append the new edge to the edges of the parent node */
        parent->edges = lappend(parent->edges,e);
    }
}



/**
 * get the node of the given list with a specific id
 */
struct node* getNodeById(List* nodes,long int currentId){

    ListCell* item;
    /* iterate over nodes */
    foreach(item,nodes){
        struct node* node = lfirst(item);

        /* if the key of the node is the search id return the node */
        if(node->key == currentId){
            return node;
        }
    }
    return NULL;
}

/**
 * returns the nth node of a list or NULL if n is not valid
 */
struct edge* getNthEdgeFromNode(List* nodes, long int nodeid, int n){
    /* Get the edges of the given node */
    List* edges = getNodeById(nodes,nodeid)->edges;

    /* get the nth edge */
    struct edge* ed = list_nth(edges,n);

    /* if it is valid return it, otherwise NULL */
    if(ed){
        return ed;
    }
    else{
        return NULL;
    }
}

