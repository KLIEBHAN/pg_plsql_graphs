#include "plpgsql.h"
#include "nodes/pg_list.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <igraph/igraph.h>
#include "pg_plsql_graphs.h"
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
