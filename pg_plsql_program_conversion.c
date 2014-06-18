#include "plpgsql.h"
#include "nodes/pg_list.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <igraph/igraph.h>
#include "pg_plsql_graphs.h"


void replaceStmt(PLpgSQL_stmt* oldStmt, PLpgSQL_stmt* newStmt, List* parents){


    ListCell* l;
    /* iterate over the statements */
    foreach(l, parents){
        if(oldStmt == lfirst(l)){
            lfirst(l) = newStmt;
            break;
        }
    }

}


void handleForEach(PLpgSQL_stmt_foreach_a* foreach ,List* parents, PLpgSQL_execstate *estate, PLpgSQL_function *func){
    PLpgSQL_stmt_assign* assignment = palloc(sizeof(PLpgSQL_stmt_assign));
    assignment->expr = palloc0(sizeof(PLpgSQL_expr));
    assignment->varno = 5;
    assignment->expr->dtype         = PLPGSQL_DTYPE_EXPR;
    assignment->expr->query         = "SELECT 2.5";
    assignment->cmd_type = PLPGSQL_STMT_ASSIGN;
    assignment->expr->func = func;

    replaceStmt(foreach,assignment,parents);
}


void analyse(PLpgSQL_execstate *estate, PLpgSQL_function *func){
    List* statements = func->action->body;


    ListCell* l;
    /* iterate over the statements */
    foreach(l, list_copy(statements)){

        /* Get the statement */
        PLpgSQL_stmt* stmt = lfirst(l);

        switch (stmt->cmd_type) {
            case PLPGSQL_STMT_FOREACH_A:{
                handleForEach(stmt,statements,estate,func);
                break;
            }

        }

    }



    /* convert the statements to an flow-graph */
    igraph_t* igraph = createFlowGraph(func,estate);

    /* perform depenence analysis operations on igraph */
    addProgramDependenceEdges(igraph);
}
