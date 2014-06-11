#include "plpgsql.h"
#include "nodes/pg_list.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <igraph/igraph.h>
#include "pg_plsql_graphs.h"




/**
 * adds labels to the current node and outgoing edges
 */
void addLabels(int nodeid, igraph_t* igraph){



    PLpgSQL_function* function = getIGraphGlobalAttrP(igraph,"function");
    PLpgSQL_stmt* stmt = getIGraphNodeAttrP(igraph,"stmt",nodeid);


    igraph_vector_t eids;
    igraph_vector_init(&eids, 0);
    igraph_incident(igraph, &eids, nodeid, IGRAPH_OUT);

    /* Init edges labels to blank */
    for(int i=0;i<igraph_vector_size(&eids);i++){
        setIGraphEdgeAttrS(igraph,"label",VECTOR(eids)[i],"");
    }

    char* label = palloc(200*sizeof(char));

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



                sprintf(label,"%s := %s",
                        varnumberToVarname( assignment->varno,
                                            function->datums,
                                            function->ndatums),
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
                    setIGraphEdgeAttrS(igraph,"label",VECTOR(eids)[0],"1");
                }
                /* and the second with 0 */
                if(igraph_vector_size(&eids)> 1){
                    setIGraphEdgeAttrS(igraph,"label",VECTOR(eids)[1],"0");
                }
                break;
            }
            case PLPGSQL_STMT_WHILE:{
                PLpgSQL_stmt_while* whileStmt  = ((PLpgSQL_stmt_while*)stmt);
                /* remove the SELECT of the condinational query */
                label = removeFromStringN(whileStmt->cond->query,"SELECT ");

                /* label the first edge with 1 */
                if(igraph_vector_size(&eids) > 0){
                    setIGraphEdgeAttrS(igraph,"label",VECTOR(eids)[0],"1");
                }
                /* and the second with 0 */
                if(igraph_vector_size(&eids)> 1){
                    setIGraphEdgeAttrS(igraph,"label",VECTOR(eids)[1],"0");
                }
                break;
            }
            case PLPGSQL_STMT_FORI:{
                PLpgSQL_stmt_fori* foriStmt  = ((PLpgSQL_stmt_fori*)stmt);

                sprintf(label,"FOR %s in %s..%s",
                        foriStmt->var->refname,
                        foriStmt->lower->query,
                        foriStmt->upper->query);

                if(foriStmt->step)
                    sprintf(eos(label)," by %s",foriStmt->step->query);

                label = removeFromString(label,"SELECT ");



                /* label the first edge with 1 */
                if(igraph_vector_size(&eids) > 0){
                    setIGraphEdgeAttrS(igraph,"label",VECTOR(eids)[0],"1");
                }
                /* and the second with 0 */
                if(igraph_vector_size(&eids)> 1){
                    setIGraphEdgeAttrS(igraph,"label",VECTOR(eids)[1],"0");
                }

                break;
            }
            case PLPGSQL_STMT_FORS:{
                PLpgSQL_stmt_fors* forsStmt  = ((PLpgSQL_stmt_fors*)stmt);

                sprintf(label,"FOR %s IN %s",
                        forsStmt->rec->refname,
                        forsStmt->query->query);



                /* label the first edge with 1 */
                if(igraph_vector_size(&eids) > 0){
                    setIGraphEdgeAttrS(igraph,"label",VECTOR(eids)[0],"1");
                }
                /* and the second with 0 */
                if(igraph_vector_size(&eids)> 1){
                    setIGraphEdgeAttrS(igraph,"label",VECTOR(eids)[1],"0");
                }

                break;
            }
            case PLPGSQL_STMT_FOREACH_A:{
                PLpgSQL_stmt_foreach_a* foreachStmt  = ((PLpgSQL_stmt_foreach_a*)stmt);

                sprintf(label,"FOREACH %s IN %s",
                        varnumberToVarname(foreachStmt->varno,function->datums,function->ndatums),
                        foreachStmt->expr->query);

                label = removeFromString(label,"SELECT ");


                /* label the first edge with 1 */
                if(igraph_vector_size(&eids) > 0){
                    setIGraphEdgeAttrS(igraph,"label",VECTOR(eids)[0],"1");
                }
                /* and the second with 0 */
                if(igraph_vector_size(&eids)> 1){
                    setIGraphEdgeAttrS(igraph,"label",VECTOR(eids)[1],"0");
                }

                break;
            }
            case PLPGSQL_STMT_RETURN:{

                PLpgSQL_stmt_return* returnStmt  = ((PLpgSQL_stmt_return*)stmt);
                /* concatinate RETURN with the return query */

                sprintf(label,"RETURN %s ",returnStmt->expr->query);

                /* remove the SELECT */
                label = removeFromString(label,"SELECT ");
                break;
            }
            case PLPGSQL_STMT_EXECSQL:{
                PLpgSQL_stmt_execsql* execSqlStmt  = ((PLpgSQL_stmt_execsql*)stmt);


                sprintf(label,"%s INTO ",execSqlStmt->sqlstmt->query);

                for(int i=0;i<execSqlStmt->row->nfields;i++){

                    sprintf(eos(label),"%s",varnumberToVarname( execSqlStmt->row->varnos[i],
                                                                function->datums,
                                                                function->ndatums)  );

                    if(i != execSqlStmt->row->nfields-1){
                        sprintf(eos(label),",");
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
        setIGraphNodeAttrS(igraph,"label",nodeid,label);
    }
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
                setIGraphNodeAttrP(igraph,"write",nodeid,bms_copy(bms_make_singleton(assignment->varno)));

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
            case PLPGSQL_STMT_FORI:{

                PLpgSQL_stmt_fori* foriStmt  = ((PLpgSQL_stmt_fori*)stmt);


                //TODO different types of for loops

                /* Get the parameters of the query of the while statement. Those are our read variables */
                Bitmapset* bms = NULL;


                if(foriStmt->lower != NULL)
                    bms = bms_union(bms,getParametersOfQueryExpr( foriStmt->lower,
                                                                        function,
                                                                        estate));
                if(foriStmt->upper != NULL)
                    bms = bms_union(bms,getParametersOfQueryExpr( foriStmt->upper,
                                                                        function,
                                                                        estate));

                if(foriStmt->step != NULL)
                    bms = bms_union(bms,getParametersOfQueryExpr( foriStmt->step,
                                                                    function,
                                                                    estate));

                /* Set the read variables of the statement */
                setIGraphNodeAttrP(igraph,"read",nodeid,bms);



                if(foriStmt->var->dtype == PLPGSQL_DTYPE_VAR){
                    /* Set the write variables of the statement */
                    setIGraphNodeAttrP(igraph,"write",nodeid,bms_copy(bms_make_singleton(foriStmt->var->dno)));
                }

                break;
            }
            case PLPGSQL_STMT_FORS:{
                PLpgSQL_stmt_fors* forsStmt  = ((PLpgSQL_stmt_fors*)stmt);

                /* Get the parameters of the query of the statement. Those are our read variables */
                Bitmapset* bms = bms_copy(getParametersOfQueryExpr( forsStmt->query,
                                                                    function,
                                                                    estate));


                /* Set the read variables of the statement */
                setIGraphNodeAttrP(igraph,"read",nodeid,bms);


                break;
            }
            case PLPGSQL_STMT_FOREACH_A:{
                PLpgSQL_stmt_foreach_a* foreachStmt  = ((PLpgSQL_stmt_foreach_a*)stmt);

                /* Get the parameters of the query of the statement. Those are our read variables */
                Bitmapset* bms = bms_copy(getParametersOfQueryExpr( foreachStmt->expr,
                                                                    function,
                                                                    estate));


                /* Set the read variables of the statement */
                setIGraphNodeAttrP(igraph,"read",nodeid,bms);


                setIGraphNodeAttrP(igraph,"write",nodeid,bms_copy(bms_make_singleton(foreachStmt->varno)));


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



/**
 * Add WR, RW and WW dependence edges to the reachable nodes of the current node
 */
void addDependenceEges(igraph_t* igraph, long nodeid, Datum* argument, Datum* result, bool lastElem){


    PLpgSQL_function* function = getIGraphGlobalAttrP(igraph,"function");
    Bitmapset* writeBms1 = getIGraphNodeAttrP(igraph,"write",nodeid);
    Bitmapset* readBms1 = getIGraphNodeAttrP(igraph,"read",nodeid);

    if(nodeid == 0 || (writeBms1 == NULL && readBms1 == NULL)){
        return;
    }


    /* reachable nodes in dfs fashion */
    igraph_vector_t order;
    igraph_vector_init(&order, 0);
    igraph_dfs(igraph, /*root=*/nodeid, /*neimode=*/IGRAPH_OUT,
            /*unreachable=*/0, &order, 0, 0, 0, 0, 0,0);



    /* iterate over reachable edges of the current node in dfs fashion */
    for(int i=0;order.stor_begin != NULL && i<igraph_vector_size(&order);i++){
        /* current vertex id */
        unsigned int vid = VECTOR(order)[i];

        if(nodeid != vid){
            Bitmapset* readBms2 = getIGraphNodeAttrP(igraph,"read",vid);
            Bitmapset* writeBms2 = getIGraphNodeAttrP(igraph,"write",vid);

            if(containsSameVariable(writeBms1,readBms2,function)){
                /* read -> write dependency, add edge */
                addEdgeWithAttr(igraph,nodeid,vid,"type","WR-DEPENDENCE");
            }


            if(containsSameVariable(writeBms1,writeBms2,function)){
                /* write -> write dependency, add edge */
                addEdgeWithAttr(igraph,nodeid,vid,"type","WW-DEPENDENCE");
            }


            if(containsSameVariable(readBms1,writeBms2,function)){
                /* write -> read dependency, add edge */
                addEdgeWithAttr(igraph,nodeid,vid,"type","RW-DEPENDENCE");
            }
        }
    }
    igraph_vector_destroy(&order);

}


/**
 * Add dependency eges to the Graph and therefore create a program dependence graph
 */
void addProgramDependenceEdges(igraph_t* igraph){
    iterateIGraphNodes(igraph,&addDependenceEges,NULL,NULL,0);
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

    int retval = 0;


    /* iterate incident edges */
    igraph_vector_t eids;
    igraph_vector_init(&eids,0);
    igraph_incident(igraph, &eids,node1, IGRAPH_OUT);

    for(int i=0;i<igraph_vector_size(&eids);i++){
        int eid = VECTOR(eids)[i];
        igraph_integer_t from; igraph_integer_t to;
        igraph_edge(igraph, eid,&from,&to);

        /* found a edge from the first to the second node */
        if(to == node2){
            const char* type = getIGraphEdgeAttrS(igraph,"type",eid);
            /* check if type is set */
            if(type){
                /* DEPENDENCY */
                if( strcmp(type,"RW-DEPENDENCE") == 0 ||
                    strcmp(type,"WR-DEPENDENCE") == 0 ||
                    strcmp(type,"WW-DEPENDENCE") == 0)    {
                    retval = 1;
                    break;
                }
            }
        }
    }
    igraph_vector_destroy(&eids);
    return retval;
}


int conflict(PLpgSQL_stmt* stmt1, PLpgSQL_stmt* stmt2, igraph_t* igraph){
    int node1 = getNodeNumberToStmt(stmt1,igraph);
    int node2 = getNodeNumberToStmt(stmt2,igraph);
    return dependenceConflict(node1,node2,igraph);

}


