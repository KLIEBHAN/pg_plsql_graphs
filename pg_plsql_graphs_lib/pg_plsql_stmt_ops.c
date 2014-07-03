#include "plpgsql.h"
#include "nodes/pg_list.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <igraph/igraph.h>
#include "pg_plsql_graphs_lib.h"
#include "catalog/pg_type.h"


/*
 * Build a row-variable data structure given the component variables.
 */
PLpgSQL_row *
build_row_from_vars(PLpgSQL_variable **vars, int numvars)
{
    PLpgSQL_row *row;
    int         i;

    row = palloc0(sizeof(PLpgSQL_row));
    row->dtype = PLPGSQL_DTYPE_ROW;
    row->rowtupdesc = CreateTemplateTupleDesc(numvars, false);
    row->nfields = numvars;
    row->fieldnames = palloc(numvars * sizeof(char *));
    row->varnos = palloc(numvars * sizeof(int));

    for (i = 0; i < numvars; i++)
    {
        PLpgSQL_variable *var = vars[i];
        Oid         typoid = RECORDOID;
        int32       typmod = -1;
        Oid         typcoll = InvalidOid;

        switch (var->dtype)
        {
            case PLPGSQL_DTYPE_VAR:
                typoid = ((PLpgSQL_var *) var)->datatype->typoid;
                typmod = ((PLpgSQL_var *) var)->datatype->atttypmod;
                typcoll = ((PLpgSQL_var *) var)->datatype->collation;
                break;

            case PLPGSQL_DTYPE_REC:
                break;

            case PLPGSQL_DTYPE_ROW:
                if (((PLpgSQL_row *) var)->rowtupdesc)
                {
                    typoid = ((PLpgSQL_row *) var)->rowtupdesc->tdtypeid;
                    typmod = ((PLpgSQL_row *) var)->rowtupdesc->tdtypmod;
                    /* composite types have no collation */
                }
                break;

            default:
                elog(ERROR, "unrecognized dtype: %d", var->dtype);
        }

        row->fieldnames[i] = var->refname;
        row->varnos[i] = var->dno;

        TupleDescInitEntry(row->rowtupdesc, i + 1,
                           var->refname,
                           typoid, typmod,
                           0);
        TupleDescInitEntryCollation(row->rowtupdesc, i + 1, typcoll);
    }

    return row;
}



PLpgSQL_variable** varnumbersToVariables(int* varnos, int nfields,PLpgSQL_function* surroundingFunc){
    PLpgSQL_variable** vars = palloc(nfields*sizeof(PLpgSQL_variable*));

    for(int i=0;i<nfields;i++){
        vars[i] = varnumberToVariable(varnos[i],surroundingFunc);
    }
    return vars;
}


PLpgSQL_variable* varnumberToVariable(int varno,PLpgSQL_function* surroundingFunc){

    /* iterate over the datums */
    for(int i=0; i<surroundingFunc->ndatums;i++){
        PLpgSQL_datum* datum = surroundingFunc->datums[i];
        /* check if datum type is a variable */
        if (datum->dtype == PLPGSQL_DTYPE_VAR) {
            PLpgSQL_variable* myvar = ((PLpgSQL_variable*)datum);
            /* check if it has the given variable number */
            if(myvar->dno == varno){
                /* the variable */
                return myvar;
            }
        }
    }
    return (PLpgSQL_variable*)NULL;
}


/**
 * Converts the variable with the given id to its refname
 */
char* varnumberToVarname(int varno,PLpgSQL_datum** datums, int ndatums){

    /* iterate over the datums */
    for(int i=0; i<ndatums;i++){
        PLpgSQL_datum* datum = datums[i];
        /* check if datum type is a variable */
        if (datum->dtype == PLPGSQL_DTYPE_VAR) {
            PLpgSQL_variable* myvar = ((PLpgSQL_variable*)datum);
            /* check if it has the given variable number */
            if(myvar->dno == varno){
                /* return the refname of the variable */
                return myvar->refname;
            }
        }
    }
    return (char*)NULL;
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
