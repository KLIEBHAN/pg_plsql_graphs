#include "plpgsql.h"
#include "nodes/pg_list.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <igraph/igraph.h>
#include "pl_graphs.h"



struct graph_status* initStatus(int initnodeid){
    /* create entry node for the flow graph */
    struct node* root = palloc(sizeof(struct node));
    /* Fist nodeid is 0 */
    root->key = initnodeid;
    /* set edges of root to empty list */
    root->edges = NIL;

    /* init nodes with the entry node */
    List* nodes = list_make1(root);
    /* init parents with the entry node */
    List* parents = list_make1(root);
    /* status structure will carry the nodes and current parents */
    struct graph_status* status = palloc(sizeof(struct graph_status));
    status->nodes = nodes;
    status->parents = parents;

    return status;

}


void appendNewNodeAndConnectParents(int* newnodeid,
                                    struct graph_status* status,
                                    PLpgSQL_stmt* stmt){

    /* create new node id */
    (*newnodeid)++;


    /* create new node */
    struct node* newnode = palloc(sizeof(struct node));
    /* add the new id */
    newnode->key = *newnodeid;
    /* add the statement */
    newnode->stmt = stmt;

    newnode->edges = NIL;

    /* append the new node to nodes */
    lappend(status->nodes,newnode);

    /* connect parents with the new node */
    connectNodeToParents(newnode->key,status->parents);
    /* set current node as new parent */
    status->parents = list_make1(newnode);
}

/**
 * creates a program graph to a PLpgSQL_function
 */
void createProgramGraph(int* newnodeid,
                    struct graph_status* status,
                    List* statements,
                    PLpgSQL_function* function){



    ListCell* l;
    /* iterate over the statements */
    foreach(l, statements){

        /* Get the statement */
        PLpgSQL_stmt* stmt = lfirst(l);

        switch (stmt->cmd_type) {
            case PLPGSQL_STMT_ASSIGN:
                /* Append the assign statement an connect it to the parents */
                appendNewNodeAndConnectParents(newnodeid,status,stmt);
                break;
            case PLPGSQL_STMT_RAISE:
                /* Append the raise statement an connect it to the parents */
                appendNewNodeAndConnectParents(newnodeid,status,stmt);
                break;
            case PLPGSQL_STMT_WHILE:

                /* Append the while statement an connect it to the parents */
                appendNewNodeAndConnectParents(newnodeid,status,stmt);

                /* Parents after the while node */
                List* parentsAfterWhile = copy_list(status->parents);

                /* remember the while node id */
                int whileNodeId = *newnodeid;

                /* Progress the statements in the loop */
                createProgramGraph(    newnodeid,
                                    status,
                                    ((PLpgSQL_stmt_while*)stmt)->body,
                                    function);

                /* connect the while node id to the parents after the last while node */
                connectNodeToParents(whileNodeId,status->parents);

                /* parents for upcoming statements are the parents after the while node */
                status->parents = parentsAfterWhile;
                break;
            case PLPGSQL_STMT_FORI:{
                /* Append the while statement an connect it to the parents */
                appendNewNodeAndConnectParents(newnodeid,status,stmt);

                /* Parents after the fori node */
                List* parentsAfterWhile = copy_list(status->parents);

                /* remember the fori node id */
                int whileNodeId = *newnodeid;

                /* Progress the statements in the loop */
                createProgramGraph( newnodeid,
                                    status,
                                    ((PLpgSQL_stmt_fori*)stmt)->body,
                                    function);

                /* connect the fori node id to the parents after the last fori node */
                connectNodeToParents(whileNodeId,status->parents);

                /* parents for upcoming statements are the parents after the fori node */
                status->parents = parentsAfterWhile;

                break;
            }
            case PLPGSQL_STMT_FORS:{
                /* Append the while statement an connect it to the parents */
                appendNewNodeAndConnectParents(newnodeid,status,stmt);

                /* Parents after the fori node */
                List* parentsAfterFori = copy_list(status->parents);

                /* remember the fori node id */
                int foriNodeId = *newnodeid;

                /* Progress the statements in the loop */
                createProgramGraph( newnodeid,
                                    status,
                                    ((PLpgSQL_stmt_fors*)stmt)->body,
                                    function);

                /* connect the fori node id to the parents after the last fori node */
                connectNodeToParents(foriNodeId,status->parents);

                /* parents for upcoming statements are the parents after the fori node */
                status->parents = parentsAfterFori;

                break;
            }
            case PLPGSQL_STMT_FOREACH_A:{
                /* Append the while statement an connect it to the parents */
                appendNewNodeAndConnectParents(newnodeid,status,stmt);

                /* Parents after the foreach node */
                List* parentsAfterForEach = copy_list(status->parents);

                /* remember the foreach node id */
                int foreachNodeId = *newnodeid;

                /* Progress the statements in the loop */
                createProgramGraph( newnodeid,
                                    status,
                                    ((PLpgSQL_stmt_foreach_a*)stmt)->body,
                                    function);

                /* connect the foreach node id to the parents after the last foreach node */
                connectNodeToParents(foreachNodeId,status->parents);

                /* parents for upcoming statements are the parents after the foreach node */
                status->parents = parentsAfterForEach;

                break;
            }
            case PLPGSQL_STMT_IF:

                /* Append the if node and connect it to the parents */
                appendNewNodeAndConnectParents(newnodeid,status,stmt);
                /* Parents after the if node */
                List* parentsAfterIf = copy_list(status->parents);

                /* Progress the then statements */
                createProgramGraph(    newnodeid,
                                    status,
                                    ((PLpgSQL_stmt_if*)stmt)->then_body,
                                    function);

                /* Parents after the last then statement */
                List* parentsAfterThen = copy_list(status->parents);

                /* Set parents to those after the if node for progressing the else body */
                status->parents = copy_list(parentsAfterIf);

                /* Progress the else statements */
                createProgramGraph(    newnodeid,
                                    status,
                                    ((PLpgSQL_stmt_if*)stmt)->else_body,
                                    function);

                /* Parents after the last else statement */
                List* parentsAfterElse = copy_list(status->parents);

                /*  set the parents for the upcoming statements to those */
                /*  after the last then statement concatinated */
                /*  with those after the the last else statement */
                /*  or if not present to those after the if statement. */
                if(((PLpgSQL_stmt_if*)stmt)->else_body != NULL){
                    status->parents = list_concat(parentsAfterThen,parentsAfterElse);
                }
                else{
                    status->parents = list_concat(parentsAfterThen,parentsAfterIf);
                }
                break;
            case PLPGSQL_STMT_RETURN:
                /* Append the return statement an connect it to the parents */
                appendNewNodeAndConnectParents(newnodeid,status,stmt);
                /* A return statement is never a parent */
                status->parents = NIL;
                break;
            case PLPGSQL_STMT_EXECSQL:
                /* Append the exec sql statement an connect it to the parents */
                appendNewNodeAndConnectParents(newnodeid,status,stmt);
                break;
            case PLPGSQL_STMT_PERFORM:
                /* Append the perform sql statement an connect it to the parents */
                appendNewNodeAndConnectParents(newnodeid,status,stmt);
                break;
            default:
                printf("Unsupported Statement %i\n",stmt->cmd_type);
                break;
        }

    }
}

/**
 * Build the igraph
 */
igraph_t* buildIGraph(List* nodes,
                      PLpgSQL_datum**         datums,
                      int                     ndatums,
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

    setIGraphGlobalAttrP(graph,"function",function);
    setIGraphGlobalAttrP(graph,"estate",estate);
    setIGraphGlobalAttrP(graph,"datums",datums);
    setIGraphGlobalAttrL(graph,"ndatums",ndatums);

    ListCell* node;
    /* iterate over nodes */
    foreach(node, nodes){
        struct node* no = lfirst(node);


        setIGraphNodeAttrP(graph,"stmt",no->key,no->stmt);

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

            setIGraphEdgeAttrS(graph,"type",eid,"FLOW");

        }

        /* Add the labels to the current node and outgoing edges */
        addLabels(no->key,graph);

    }




    return graph;
}

igraph_t* createFlowGraph(
                            PLpgSQL_datum**         datums,
                            int                     ndatums,
                            PLpgSQL_function* function,
                            PLpgSQL_execstate *estate){
    /* ignore errors */
    igraph_set_error_handler(igraph_error_handler_printignore);

    int newnodeid = 0;
    struct graph_status* status  = initStatus(newnodeid);
    /* create the flow graph to the given PL/SQL function */
    createProgramGraph(&newnodeid,status,function->action->body,function);


    /* convert the graph to an igraph */
    return buildIGraph(status->nodes,datums,ndatums,function,estate);

}

