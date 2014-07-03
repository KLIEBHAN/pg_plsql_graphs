#include "plpgsql.h"
#include "nodes/pg_list.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <igraph/igraph.h>
#include "pg_plsql_graphs.h"
#include "postgres.h"
#include "nodes/nodes.h"
#include "parser/gramparse.h"
#include "parser/parser.h"
#include "nodes/pg_list.h"


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


char* valToStringEscape(Value* val, bool escape){
    char* value = palloc0(64);
    switch (val->type) {
        case T_String:
            if(escape){
                sprintf(value,"'%s'",strVal(val));
            }
            else{
                sprintf(value,"%s",strVal(val));
            }
            break;
        case T_Float:
            sprintf(value,"%f",floatVal(val));
            break;
        case T_Integer:
            sprintf(value,"%li",intVal(val));
            break;
        default:
            elog(ERROR,"Unkown node type:%s\n", nodeToString(val->type));
            break;
    }
    return value;
}
char* valToString(Value* val){
    return valToStringEscape(val,0);
}




char* concatMultipleValues(List* values1, List* values2, char* infix, char* suffix, char* prefix1, char* prefix2){
    char* vals = palloc0(64);

    ListCell* value1 = values1->head;
    ListCell* value2 = values2->head;

    while(value1 && value2){
        char* val1 = valToString(lfirst(value1));
        char* val2 = valToString(lfirst(value2));


        sprintf(eos(vals),"%s.%s %s %s.%s",prefix1,val1,infix,prefix2,val2);


        if(value1->next && value2->next)
            sprintf(eos(vals)," %s ",suffix);


        value1 = value1->next;
        value2 = value2->next;
    }
    return vals;
}


char* concatValuesWithPrefix(List* values, char* seperator, char* prefix){
    char* vals = palloc0(64);

    ListCell* cell;
    foreach(cell, values){
        if(prefix)
            sprintf(eos(vals),"%s.",prefix);


        sprintf(eos(vals),"%s",valToString(lfirst(cell)));
        if(cell->next)
            sprintf(eos(vals),"%s",seperator);
    }
    return vals;
}


char* concatValues(List* values, char* seperator){
    return concatValuesWithPrefix(values,seperator,NULL);
}


char* resTargetToString(ResTarget* target){

    char* vars = palloc0(1024);
    if(target->name){
        sprintf(eos(vars),"%s",target->name);
    }
    else{

        if(IsA(target->val,ColumnRef)){
            ColumnRef* ref = target->val;
            char* cri_name = valToString(lfirst(ref->fields->head));

            sprintf(eos(vars),"%s",cri_name);
        }
        else if(IsA(target->val,A_Const)){
            A_Const* c = target->val;
            sprintf(eos(vars),"%s",valToStringEscape(&c->val,1));

        }
        else if(IsA(target->val,FuncCall)){
            //VERY DIRTY -> FIX
            FuncCall* c = target->val;

            ColumnRef* colref = lfirst(c->args->head);
            sprintf(eos(vars),"%s(%s) as __%s%i",valToString(lfirst(c->funcname->head)),concatValues(colref->fields,"."),valToString(lfirst(c->funcname->head)),c->location);
        }
        else{
            elog(ERROR,"Unkown node type:%s\n", nodeToString(target));
        }


    }

    return vars;
}


List* resolveQueryRefsSkipN(char* query, int n){
    SelectStmt* select  = ((SelectStmt*)lfirst(raw_parser(query)->head));

    List* list = NIL;

    int i = 0;

    ListCell* l;
    foreach(l,select->targetList){
        i++;

        if(i <= n)
            continue;

        list = lappend(list,makeString(resTargetToString(lfirst(l))));


    }

    return list;
}


List* resolveQueryRefs(char* query){
    return resolveQueryRefsSkipN(query,0);
}



Value* resolveTargetRefStrToCorrespVarno(int varno, int* vars, int nvars,char* query){
    List* queryRefs = resolveQueryRefs(query);

    for(int i=0;i<nvars;i++){
        int curvar = vars[i];

        if(curvar == varno){
            return linitial(queryRefs);
        }



        queryRefs = list_delete_first(queryRefs);
    }
    return NULL;

}



List* resolveBmsRefs(Bitmapset* bms_in, PLpgSQL_execstate *estate, PLpgSQL_function *func){
    Bitmapset* bms = bms_copy(bms_in);


    List* list = NIL;

    while(!bms_is_empty(bms)){
        int varno = bms_first_member(bms);
        char* varname = varnumberToVarname(varno,func->datums,func->ndatums);
        list = lappend(list,varname);
    }
    return list;
}

Value* makeStringWithAlias(char* string, int aliasId){
    char* newstring = palloc0(64);

    sprintf(newstring, "%s __ALIAS_%i",string,aliasId);

    return makeString(newstring);
}

/**
 * For i ∈ {1,...,m}, j ∈ {1,...,n} : c′i′ := cj if c′i = vj else c′i′ := c′i
 */
List* resolveQueryColumns(Bitmapset* bms_in, int* vars, int nvars, char* query, PLpgSQL_execstate *estate, PLpgSQL_function *func, bool aliases){
    Bitmapset* bms = bms_copy(bms_in);



    List* list = NIL;

    while(!bms_is_empty(bms)){
        int varno = bms_first_member(bms);

        Value* val = resolveTargetRefStrToCorrespVarno(varno,vars,nvars,query);


        if(val == NULL){
            if(aliases){
                val = makeStringWithAlias(varnumberToVarname(varno,func->datums,func->ndatums),varno);
            }
            else{
                val = makeString(varnumberToVarname(varno,func->datums,func->ndatums));
            }
        }
        list = lappend(list,val);
    }
    return list;



}


char* qb(char* queryToBatch, char* parameterQuery){
    char* newQuery = palloc0(1024);

    char* paramQueryLabel = "S";

    char* parameterRefs = concatValuesWithPrefix(resolveQueryRefs(parameterQuery),",",paramQueryLabel);
    char* inputQueryRefs = concatValues(resolveQueryRefs(queryToBatch),",");


    sprintf(newQuery,"SELECT %s, %s ",parameterRefs,inputQueryRefs);
    sprintf(eos(newQuery),"FROM (%s) %s LEFT OUTER JOIN PART P  ",parameterQuery,paramQueryLabel);
    sprintf(eos(newQuery),"ON %s.s_size = P.p_size  ",paramQueryLabel);
    sprintf(eos(newQuery),"GROUP BY %s  ",concatValuesWithPrefix(resolveQueryRefs(parameterQuery),",",paramQueryLabel));


    return newQuery;
}


char* project(char* baseQuery, char* newSelects, char* subQueryAlias){
    char* query = palloc0(1024);

    sprintf(query,"SELECT %s FROM (%s) %s",newSelects,baseQuery,subQueryAlias);

    return query;
}

char* leftOuterJoin(char* baseQuery, char* secondQuery, List* params, char* aliasLeft, char* aliasRight){


    char* query = palloc0(1024);

    sprintf(query,"(%s) %s LEFT OUTER JOIN (%s) %s ON %s ",baseQuery,aliasLeft,secondQuery,aliasRight,concatMultipleValues(params,params," = "," AND ",aliasLeft,aliasRight));


    return query;
}

PLpgSQL_stmt_fors* createFors(char* query, int* varnos, int nfields ,List* body,PLpgSQL_stmt_fors* oldFors, PLpgSQL_function *func){

    PLpgSQL_stmt_fors* newFors = palloc(sizeof(PLpgSQL_stmt_fors));
    newFors->cmd_type = PLPGSQL_STMT_FORS;
    newFors->query = palloc0(sizeof(PLpgSQL_expr));
    newFors->query->dtype = PLPGSQL_DTYPE_EXPR;
    newFors->query->query = query;
    newFors->query->func = func;
    newFors->query->ns = oldFors->query->ns;
    newFors->rec = NULL;
    newFors->row = oldFors->row;
    PLpgSQL_row* a = build_row_from_vars(varnumbersToVariables(varnos,nfields,func), nfields);
    newFors->row->fieldnames = a->fieldnames;
    newFors->row->nfields = a->nfields;
    newFors->row->refname = a->refname;
    newFors->row->rowtupdesc = a->rowtupdesc;
    newFors->row->varnos = a->varnos;


    newFors->body = body;
    return newFors;
}


void handleFors(PLpgSQL_stmt_fors* fors ,List* parents, PLpgSQL_execstate *estate, PLpgSQL_function *func){
    PLpgSQL_stmt* firstStmt = lfirst(fors->body->head);
    if(firstStmt && firstStmt->cmd_type == PLPGSQL_STMT_EXECSQL){
        PLpgSQL_stmt_execsql* execsql = firstStmt;
        if(execsql->row && execsql->into){

                PLpgSQL_expr* q = execsql->sqlstmt;
                PLpgSQL_expr* qO = fors->query;

                //Parameters of the assignment query q: c′1,...,c′m
                Bitmapset* c_ = getParametersOfQueryExpr(q,func,estate);
                int m = bms_num_members(c_);


                //palloc space for a new query
                char* qbquery = palloc0(1024);
                /*
                 * The part πc′1′ ,...,c′m′ (qO) accesses the qO query and projects to the previous input parameters c′1,...,c′m of the assignment-EXECSQL in the FOR-loop.
                 * If one of these parameters is a plsql- variable bound by the input FORS statement it is not available at this point.
                 * So we have to replace it with the corresponding projected column of the query qO.
                 */

                List* c_new = resolveQueryColumns(          c_,
                                                            fors->row->varnos,
                                                            fors->row->nfields,
                                                            qO->query,
                                                            estate,
                                                            func,
                                                            1);

                sprintf(qbquery,project(  qO->query,
                                          concatValues( c_new,
                                                        ","),
                                          "_sub"));

                c_new = resolveQueryColumns(          c_,
                                                                            fors->row->varnos,
                                                                            fors->row->nfields,
                                                                            qO->query,
                                                                            estate,
                                                                            func,
                                                                            0);

                /*
                 * The result of this step is that we now have a batched version of the different input parameters of query q.
                 * This batched input parameters can now be passed to the batched version qb of query q.
                 * When executing, the result will be table with m + k columns where for every row the last k columns correspond
                 * to the result of executing query q using the first m columns as input parameters.
                 */
                sprintf(qbquery,qb(q->query,qbquery));


                /**
                 * c1,...,ck
                 */
                List* c = resolveQueryRefs(qO->query);
                /**
                 * c′′1,...,c′′k
                 */
                List* c__ = resolveQueryRefsSkipN(qbquery,m);



                /**
                 * Project to the columns c1,...,cn,c′′1,...,c′′k
                 */
                sprintf(qbquery,"SELECT %s,%s FROM %s",concatValuesWithPrefix(c,",","___a"),
                                                        concatValuesWithPrefix(c__,",","___b"),
                                                                        leftOuterJoin(
                                                                        qO->query,
                                                                        qbquery,
                                                                        c_new,
                                                                        "___a",//Problems with outer scope
                                                                        "___b"));



                int* varnosFor = palloc((fors->row->nfields+execsql->row->nfields)*sizeof(int));
                int nfields = fors->row->nfields+execsql->row->nfields;


                /**
                 * bind columns c1,...,cn,c′′1,...,c′′k to the variables
                 * v1,...,vn,v′′1,...,v′′k
                 */
                for(int i=0;i<fors->row && i < fors->row->nfields;i++){
                    int vi = fors->row->varnos[i];
                    varnosFor[i] = vi;
                }
                for(int i=0;i<execsql->row && i < execsql->row->nfields;i++){
                    int v__i = execsql->row->varnos[i];
                    varnosFor[i+fors->row->nfields] = v__i;
                }


                PLpgSQL_stmt_fors* newFors = createFors(qbquery,varnosFor,nfields,list_copy_tail(fors->body,1),fors,func);


                /**
                 * Now the new FORS loop is correctly assembled and can replace the input FORS loop and the single-row assignment-EXECSQL statement.
                 */
                replaceStmt(fors,newFors,parents);




        }
    }

}


void analyse(PLpgSQL_execstate *estate, PLpgSQL_function *func){


    if(strcmp(func->fn_signature,"dotest1()") == 0){

        List* statements = func->action->body;


        ListCell* l;
        /* iterate over the statements */
        foreach(l, list_copy(statements)){

            /* Get the statement */
            PLpgSQL_stmt* stmt = lfirst(l);

            switch (stmt->cmd_type) {
                case PLPGSQL_STMT_FORS:{
                    handleFors(stmt,statements,estate,func);
/*
                    PLpgSQL_stmt_fors* fors = stmt;


                    PLpgSQL_stmt* firstStmt = lfirst(fors->body->head);
                    PLpgSQL_stmt_execsql* execsql = firstStmt;

                    SelectStmt* select  = ((SelectStmt*)lfirst(raw_parser(execsql->sqlstmt->query)->head));

                    printf("%i\n",fors->query->ns->itemno);
*/

                    break;
                }

            }

        }



        /* convert the statements to an flow-graph */
        igraph_t* igraph = createFlowGraph(func,estate);

        /* perform depenence analysis operations on igraph */
        addProgramDependenceEdges(igraph);

    }
}
