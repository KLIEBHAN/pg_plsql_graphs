#include "plpgsql.h"
#include "nodes/pg_list.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <igraph/igraph.h>
#include "pg_plsql_graphs.h"




void* setIGraphGlobalAttrP(igraph_t* igraph, const char* name, void* pointer){
    union dblPointer data;
    data.pointer = pointer;;

    if(data.doublevalue == data.doublevalue)
        SETGAN(igraph,name,data.doublevalue);
    else
        return NULL;
}


void* getIGraphGlobalAttrP(igraph_t* igraph, const char* name){
    union dblPointer data;
    data.doublevalue = GAN(igraph,name);

    if(data.doublevalue == data.doublevalue)/* valid pointer */
        return data.pointer;
    else
        return NULL;
}



void* setIGraphGlobalAttrL(igraph_t* igraph, const char* name, long value){
    union dblPointer data;
    data.longvalue = value;

    SETGAN(igraph,name,data.doublevalue);
}

long getIGraphGlobalAttrL(igraph_t* igraph, const char* name){
    union dblPointer data;
    data.doublevalue = GAN(igraph,name);

    if(data.doublevalue == data.doublevalue)
        return data.longvalue;
    else
        return 0;
}



void* setIGraphNodeAttrP(igraph_t* igraph, const char* name, long nodeid, void* pointer){
    union dblPointer data;
    data.pointer = pointer;


    if(data.doublevalue == data.doublevalue)/* valid pointer */
        SETVAN(igraph,name,nodeid,data.doublevalue);
    else
        return NULL;
}

void* getIGraphNodeAttrP(igraph_t* igraph, const char* name, long nodeid){
    union dblPointer data;
    data.doublevalue = VAN(igraph,name,nodeid);


    if(data.doublevalue == data.doublevalue)
        return data.pointer;
    else
        return NULL;
}


void* setIGraphNodeAttrL(igraph_t* igraph, const char* name, long nodeid, long value){
    union dblPointer data;
    data.longvalue = value;


    SETVAN(igraph,name,nodeid,data.doublevalue);
}


long getIGraphNodeAttrL(igraph_t* igraph, const char* name, long nodeid){
    union dblPointer data;
    data.doublevalue = VAN(igraph,name,nodeid);

    if(data.doublevalue == data.doublevalue)
        return data.longvalue;
    else
        return 0;
}



/**
 * iterates over the iGraph Nodes and calls a given callback function
 */
void iterateIGraphNodes(igraph_t* igraph, void (*callback)(igraph_t*, long, Datum*, Datum*, bool),Datum* arguments, Datum* results, bool breakIfFound){

    /* init and create a node iterator over all nodes */
    igraph_vit_t nodeit;
    igraph_vs_t allNodes;
    igraph_vs_all(&allNodes);
    igraph_vit_create(igraph,allNodes, &nodeit);


    /* iterate over the nodes */
    while (!IGRAPH_VIT_END(nodeit)) {
        /* current node id */
        long int nodeid = (long int) IGRAPH_VIT_GET(nodeit);


        /* next iteration over graph nodes */
        IGRAPH_VIT_NEXT(nodeit);



        /* call the callback function */
        (*callback)(igraph,nodeid,arguments,results,IGRAPH_VIT_END(nodeit));

        if(breakIfFound && DatumGetPointer(*results) != NULL){
            break;
        }

    }
    /* destroy the graph iterator */
    igraph_vit_destroy(&nodeit);
    igraph_vs_destroy(&allNodes);
}

/**
 * iterates over the iGraph Edges To the given node and calls a given callback function
 */
void iterateIGraphOutEdges(igraph_t* igraph, long nodeid, void (*callback)(igraph_t*, long, long, long, Datum*, Datum*, bool),Datum* arguments, Datum* results, bool breakIfFound){

    /* init and create a iterator over the neighbor node that lie on outgoing edges */
    igraph_es_t es;
    igraph_eit_t eit;
    igraph_es_incident(&es, nodeid, IGRAPH_OUT);
    igraph_eit_create(igraph, es, &eit);
    /* iterate over the neighbor node that lie on outgoing edges */
    while (!IGRAPH_VIT_END(eit)) {
        /* get the edge id */
        igraph_integer_t eid = IGRAPH_EIT_GET(eit);


        igraph_integer_t from;
        igraph_integer_t to;
        igraph_edge(igraph, eid, &from, &to);


        /* next iteration over neighbor nodes */
        IGRAPH_EIT_NEXT(eit);


        (*callback)(igraph,eid,from,to,arguments,results,IGRAPH_VIT_END(eit));

        if(breakIfFound && DatumGetPointer(*results) != NULL){
            break;
        }
    }
    /* destroy the neighbor iterator */
    igraph_eit_destroy(&eit);
    igraph_es_destroy(&es);/* call the callback function */
}


void iterateEdgesBridge(igraph_t* igraph, long nodeid, Datum* arguments, Datum* results, bool breakIfFound){
    iterateIGraphOutEdges(igraph,nodeid,DatumGetPointer(arguments[0]),DatumGetPointer(arguments[1]),results,breakIfFound);
}


void iterateReachableEdges(igraph_t* graph, void (*callback)(igraph_t*, long, long, long, Datum*, Datum*, bool),Datum* arguments, Datum* results, bool breakIfFound){

    Datum datums[2];

    datums[0] = PointerGetDatum(callback);
    datums[1] = PointerGetDatum(arguments);

    iterateIGraphNodes(graph,&iterateEdgesBridge,datums,results,breakIfFound);
}



/**
 * adds labels to the current node and outgoing edges
 */
void addLabels(int nodeid, igraph_t* igraph){



    PLpgSQL_function* function = getIGraphGlobalAttrP(igraph,"function");
    PLpgSQL_stmt* stmt = getIGraphNodeAttrP(igraph,"stmt",nodeid);


    igraph_vector_t eids;
    igraph_vector_init(&eids, 0);
    igraph_incident(igraph, &eids, nodeid, IGRAPH_OUT);

    char* label = NULL;

    if(nodeid == 0){
        label = "entry";
    }
    else if(!stmt){
        label = "unknown";
    }
    else{
        switch (stmt->cmd_type) {
            case PLPGSQL_STMT_ASSIGN:{
                PLpgSQL_stmt_assign* assignment  = ((PLpgSQL_stmt_assign*)stmt);
                PLpgSQL_expr* expr = assignment->expr;

                /* Concat Varibalename with := and the expression */
                label = concatStrings3(
                                            varnumberToVarname( assignment->varno,
                                                                function->datums,
                                                                function->ndatums),
                                            " := ",
                                            expr->query);
                /* remove the SELECT */
                label = removeFromString(label,"SELECT ");
                break;
            }
            case PLPGSQL_STMT_RAISE:{
                label = "RAISE";
                break;
            }
            case PLPGSQL_STMT_IF:{

                PLpgSQL_stmt_if* ifStmt  = ((PLpgSQL_stmt_if*)stmt);



                /* remove the SELECT of the condinational query */
                label = removeFromStringN(ifStmt->cond->query,"SELECT ");

                /* label the first edge with 1 */
                if(igraph_vector_size(&eids) > 0){
                    SETEAS(igraph,"label",VECTOR(eids)[0],"1");
                }
                /* and the second with 0 */
                if(igraph_vector_size(&eids)> 1){
                    SETEAS(igraph,"label",VECTOR(eids)[1],"0");
                }
                break;
            }
            case PLPGSQL_STMT_WHILE:{
                PLpgSQL_stmt_while* whileStmt  = ((PLpgSQL_stmt_while*)stmt);
                /* remove the SELECT of the condinational query */
                label = removeFromStringN(whileStmt->cond->query,"SELECT ");

                /* label the first edge with 1 */
                if(igraph_vector_size(&eids) > 0){
                    SETEAS(igraph,"label",VECTOR(eids)[0],"1");
                }
                /* and the second with 0 */
                if(igraph_vector_size(&eids)> 1){
                    SETEAS(igraph,"label",VECTOR(eids)[1],"0");
                }
                break;
            }
            case PLPGSQL_STMT_RETURN:{

                PLpgSQL_stmt_return* returnStmt  = ((PLpgSQL_stmt_return*)stmt);
                /* concatinate RETURN with the return query */
                label = concatStrings2("RETURN ",returnStmt->expr->query);

                /* remove the SELECT */
                label = removeFromString(label,"SELECT ");
                break;
            }
            case PLPGSQL_STMT_EXECSQL:{
                PLpgSQL_stmt_execsql* execSqlStmt  = ((PLpgSQL_stmt_execsql*)stmt);

                label = palloc(100*sizeof(char));

                strcat(label,execSqlStmt->sqlstmt->query);
                strcat(label," INTO ");

                for(int i=0;i<execSqlStmt->row->nfields;i++){
                    strcat(label,
                            varnumberToVarname(    execSqlStmt->row->varnos[i],
                                                function->datums,
                                                function->ndatums));
                    if(i != execSqlStmt->row->nfields-1){
                        strcat(label,",");
                    }

                }
                break;
            }
            default:{
                /* unsupported command */
                label = "unknown";
                break;
           }
        }
    }

    if(label){
        /* set the label of the current node as attribute  */
        /* to the corresponding node of the igraph */
        SETVAS(igraph,"label",nodeid,label);
    }
}



/**
 * Workaround to get the parameters of a query as Bitmapset
 */
Bitmapset* getParametersOfQueryExpr(PLpgSQL_expr*         expr,
                                    PLpgSQL_function*     surroundingFunction,
                                    PLpgSQL_execstate*     estate){

    expr->func = surroundingFunction;
    SPI_prepare_params(expr->query,
                              (ParserSetupHook) pg_plsql_parser_setup,
                              (void *) expr,
                              0);


    return expr->paramnos;
}



/**
 * sets the reads and writes of statements in the graph nodes
 */
void setReadsAndWrites(int nodeid, igraph_t* igraph){

    if(nodeid == 0)
        return;

    PLpgSQL_function* function = getIGraphGlobalAttrP(igraph,"function");
    PLpgSQL_execstate* estate = getIGraphGlobalAttrP(igraph,"estate");
    PLpgSQL_stmt* stmt = getIGraphNodeAttrP(igraph,"stmt",nodeid);

    /* switch statement type */
    if(stmt && stmt->cmd_type){
        switch (stmt->cmd_type) {
            case PLPGSQL_STMT_ASSIGN:{
                PLpgSQL_stmt_assign* assignment  = ((PLpgSQL_stmt_assign*)stmt);

                /* Set the wites variables of the statement to the variable the assignment writes */
                setIGraphNodeAttrP(igraph,"write",nodeid,bms_make_singleton(assignment->varno));

                /* Get the parameters of the query of the assignment. Those are our read variables */
                Bitmapset* bms = bms_copy(getParametersOfQueryExpr( assignment->expr,
                                                                    function,
                                                                    estate));
                /* Set the read variables of the statement */
                setIGraphNodeAttrP(igraph,"read",nodeid,bms);
                break;
            }
            case PLPGSQL_STMT_RAISE:{
                break;
            }
            case PLPGSQL_STMT_IF:{
                PLpgSQL_stmt_if* ifStmt  = ((PLpgSQL_stmt_if*)stmt);

                /* Get the parameters of the query of the if statement. Those are our read variables */
                Bitmapset* bms = bms_copy(getParametersOfQueryExpr( ifStmt->cond,
                                                                    function,
                                                                    estate));
                 /* Set the read variables of the statement */
                setIGraphNodeAttrP(igraph,"read",nodeid,bms);
                break;
            }
            case PLPGSQL_STMT_WHILE:{
                PLpgSQL_stmt_while* whileStmt  = ((PLpgSQL_stmt_while*)stmt);


                /* Get the parameters of the query of the while statement. Those are our read variables */
                Bitmapset* bms = bms_copy(getParametersOfQueryExpr( whileStmt->cond,
                                                                    function,
                                                                    estate));
                /* Set the read variables of the statement */
               setIGraphNodeAttrP(igraph,"read",nodeid,bms);
                break;
            }
            case PLPGSQL_STMT_RETURN:{
                PLpgSQL_stmt_return* returnStmt  = ((PLpgSQL_stmt_return*)stmt);

                /* Get the parameters of the query of the return statement. Those are our read variables */
                Bitmapset* bms = bms_copy(getParametersOfQueryExpr( returnStmt->expr,
                                                                    function,
                                                                    estate));
                /* Set the read variables of the statement */
                setIGraphNodeAttrP(igraph,"read",nodeid,bms);
                break;
            }
            case PLPGSQL_STMT_EXECSQL:{

                PLpgSQL_stmt_execsql* execSqlStmt  = ((PLpgSQL_stmt_execsql*)stmt);



                /* get the variables the exec stmt writes as Bitmapset and set them as our write variables */
                Bitmapset* bmsWrite = intArrayToBitmapSet(execSqlStmt->row->varnos,execSqlStmt->row->nfields);
                setIGraphNodeAttrP(igraph,"write",nodeid,bmsWrite);


                /* Get the parameters of the query of the sql statement. Those are our read variables */
                Bitmapset* bmsRead = bms_copy(getParametersOfQueryExpr( execSqlStmt->sqlstmt,
                                                                    function,
                                                                    estate));
                /* Set the read variables of the statement */
                setIGraphNodeAttrP(igraph,"read",nodeid,bmsRead);


                break;
            }
            default:
                break;
        }
    }


}


bool containsSameVariable(Bitmapset* bms1,Bitmapset* bms2,PLpgSQL_function* function){

    if(bms1 == NULL || bms2 == NULL){
        return 0;
    }

    /* create the intersection of the two bitmapsets */
    Bitmapset* intersection = bms_intersect(bms1,bms2);

    /* iterate them */
    int dno;
    while (intersection != NULL &&  !bms_is_empty(intersection) && (dno = bms_first_member(intersection)) >= 0){
        /* if the intersection has a dno of type PLPGSQL_DTYPE_VAR bms1 and bms2 contain the same variable */
        if(function->datums[dno]->dtype == PLPGSQL_DTYPE_VAR)
            return 1;
    }
    return 0;
}


/**
 * Add WR, RW and WW dependence edges to the reachable nodes of the current node and therefore create a program depence graph
 */
void createProgramDependenceGraph(igraph_t* igraph, long nodeid, Datum* argument, Datum* result, bool lastElem){


    PLpgSQL_function* function = getIGraphGlobalAttrP(igraph,"function");
    PLpgSQL_stmt* stmt = getIGraphNodeAttrP(igraph,"stmt",nodeid);
    Bitmapset* writeBms1 = getIGraphNodeAttrP(igraph,"write",nodeid);
    Bitmapset* readBms1 = getIGraphNodeAttrP(igraph,"read",nodeid);

    if(nodeid == 0 || (writeBms1 == NULL && readBms1 == NULL)){
        return;
    }


    igraph_vector_t order;
    igraph_vector_init(&order, 0);

    igraph_dfs(igraph, /*root=*/nodeid, /*neimode=*/IGRAPH_OUT,
            /*unreachable=*/0, &order, 0, 0, 0, 0, 0,0);



    /* iterate over reachable edges of the current node */
    for(int i=0;order.stor_begin != NULL && i<igraph_vector_size(&order);i++){
        /* current vertex id */
        unsigned int vid = VECTOR(order)[i];

        if(nodeid != vid){
            Bitmapset* readBms2 = getIGraphNodeAttrP(igraph,"read",vid);
            Bitmapset* writeBms2 = getIGraphNodeAttrP(igraph,"write",vid);

            if(containsSameVariable(writeBms1,readBms2,function)){

                /* read -> write dependency, add edge */

                /* Add edge to the graph */
                igraph_add_edge(igraph,nodeid,vid);
                igraph_integer_t eid;
                igraph_get_eid(igraph, &eid,nodeid,vid,1,0);

                SETEAS(igraph,"type",eid,"WR-DEPENDENCE");
            }


            if(containsSameVariable(writeBms1,writeBms2,function)){

                /* write -> write dependency, add edge */

                /* Add edge to the graph */
                igraph_add_edge(igraph,nodeid,vid);
                igraph_integer_t eid;
                igraph_get_eid(igraph, &eid,nodeid,vid,1,0);

                SETEAS(igraph,"type",eid,"WW-DEPENDENCE");
            }


            if(containsSameVariable(readBms1,writeBms2,function)){

                /* write -> read dependency, add edge */

                /* Add edge to the graph */
                igraph_add_edge(igraph,nodeid,vid);
                igraph_integer_t eid;
                igraph_get_eid(igraph, &eid,nodeid,vid,1,0);

                SETEAS(igraph,"type",eid,"RW-DEPENDENCE");
            }





        }





    }
    igraph_vector_destroy(&order);

}

/**
 * if the statement of the current node equals the given node set the result to the node number
 */
void retrieveNodeNumber(igraph_t* igraph, long nodeId, Datum* argument, Datum* result, bool lastElem){
    PLpgSQL_stmt* argStmt = (PLpgSQL_stmt*)DatumGetPointer(*argument);

    PLpgSQL_stmt* stmt = getIGraphNodeAttrP(igraph,"stmt",nodeId);


    if(argStmt == stmt){
        Datum datum = Int64GetDatum(nodeId);
        *result = datum;
    }

}


long getNodeNumberToStmt(PLpgSQL_stmt* stmt1, igraph_t* graph){
    Datum stmt     = PointerGetDatum(stmt1);
    Datum resDatum = PointerGetDatum(NULL);


    iterateIGraphNodes(graph,&retrieveNodeNumber,&stmt,&resDatum,1);

    return DatumGetInt64(resDatum);
}



int dependenceConflict(int node1, int node2, igraph_t* igraph){

    /* if there is no node for the statement we must assume a conflict */
    if(node1 == -1 || node2 == -1){
        return 1;
    }



    igraph_vector_t eids;
    igraph_vector_init(&eids,0);
    igraph_incident(igraph, &eids,node1, IGRAPH_OUT);

    int retval = 0;

    for(int i=0;i<igraph_vector_size(&eids);i++){
        int eid = VECTOR(eids)[i];
        igraph_integer_t from; igraph_integer_t to;
        igraph_edge(igraph, eid,&from,&to);

        /* found a edge from the first to the second node */
        if(to == node2){

            const char* type = EAS(igraph,"type",eid);
            /* check if type is set */
            if(type){
                /* RW-DEPENDENCY */
                if(strcmp(type,"RW-DEPENDENCE") == 0){
                    printf("RW-DEPENDENCE found from %s to %s!\n",
                                VAS(igraph,"label",from),
                                VAS(igraph,"label",to));
                    retval = 1;
                    break;
                }
                /* WR-DEPENDENCY */
                if(strcmp(type,"WR-DEPENDENCE") == 0){
                    printf("WR-DEPENDENCE found from %s to %s!\n",
                                VAS(igraph,"label",from),
                                VAS(igraph,"label",to));
                    retval = 1;
                    break;
                }
                /* WW-DEPENDENCY */
                if(strcmp(type,"WW-DEPENDENCE") == 0){
                    printf("WW-DEPENDENCE found from %s to %s!\n",
                                VAS(igraph,"label",from),
                                VAS(igraph,"label",to));
                    retval = 1;
                    break;
                }

            }


        }

    }

    if(retval == 0){
        printf("No confict found from %s to %s!\n",
                    VAS(igraph,"label",node1),
                    VAS(igraph,"label",node2));
    }

    igraph_vector_destroy(&eids);
    return retval;
}


int conflict(PLpgSQL_stmt* stmt1, PLpgSQL_stmt* stmt2, igraph_t* igraph){
    int node1 = getNodeNumberToStmt(stmt1,igraph);
    int node2 = getNodeNumberToStmt(stmt2,igraph);
    return dependenceConflict(node1,node2,igraph);

}


