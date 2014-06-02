#include "plpgsql.h"
#include "nodes/pg_list.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <igraph/igraph.h>
#include "pg_plsql_graphs.h"
/**
 * Build the igraph
 */
igraph_t* buildIGraph(List* nodes,
                      PLpgSQL_function* function,
                      PLpgSQL_execstate * estate){

    /* init a new igraph */
    igraph_t* graph = palloc(sizeof(igraph_t));
    /* init the edges */
    igraph_vector_t graphedges;
    igraph_vector_init(&graphedges, 0);

    /* turn on attribute handling */
    igraph_i_set_attribute_table(&igraph_cattribute_table);

    igraph_empty(graph,nodes->length,1);

    union dblPointer data;
    data.pointer = function;
    SETGAN(graph,"function",data.doublevalue);

    data.pointer = estate;
    SETGAN(graph,"estate",data.doublevalue);


    ListCell* node;
    /* iterate over nodes */
    foreach(node, nodes){
        struct node* no = lfirst(node);


        union dblPointer data;
        data.pointer = no->stmt;

        SETVAN(graph,"stmt",no->key,data.doublevalue);


        setReadsAndWrites(no->key,graph);


        ListCell* e;
        /* iterate over outgoing edges of the current node */
        foreach(e,no->edges){
            struct edge* edge = lfirst(e);

            /* Add edges to the graph */
            igraph_add_edge(graph,
                            edge->sourceid,
                            edge->targetid);

            igraph_integer_t eid;
            igraph_get_eid(graph,
                           &eid,
                           edge->sourceid,
                           edge->targetid,
                           1,
                           0);

            SETEAS(graph,"type",eid,"FLOW");

        }

        /* Add the labels to the current node and outgoing edges */
        addLabels(no->key,graph);

    }




    return graph;
}
