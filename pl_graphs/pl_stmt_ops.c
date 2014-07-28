#include "plpgsql.h"
#include "nodes/pg_list.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <igraph/igraph.h>
#include "pl_graphs.h"
#include "catalog/pg_type.h"
#include "parser/parse_node.h"

/**
 * Workaround to get the parameters of a query as Bitmapset
 */
Bitmapset* getParametersOfQueryExpr(PLpgSQL_expr*           expr,
                                    PLpgSQL_datum**         datums,
                                    int                     ndatums,
                                    PLpgSQL_function*       surroundingFunction,
                                    PLpgSQL_execstate*      estate){

    return expr->paramnos;
}






PLpgSQL_variable** varnumbersToVariables(int* varnos, int nfields,PLpgSQL_datum** datums){
    PLpgSQL_variable** vars = palloc0(nfields*sizeof(PLpgSQL_variable*));

    for(int i=0;i<nfields;i++){
        vars[i] = varnumberToVariable(varnos[i],datums);
    }

    return vars;
}


PLpgSQL_variable* varnumberToVariable(int varno,PLpgSQL_datum** datums){

    PLpgSQL_datum* datum = datums[varno];
    /* check if datum type is a variable */
    if (datum->dtype == PLPGSQL_DTYPE_VAR) {
        PLpgSQL_variable* myvar = ((PLpgSQL_variable*)datum);
        return myvar;
    }
    return (PLpgSQL_variable*)NULL;
}



/**
 * Converts the variable with the given id to its refname
 */
char* varnumberToVarname(int varno,PLpgSQL_datum** datums){
    return varnumberToVariable(varno,datums)->refname;
}


bool containsSameVariable(Bitmapset* bms1,Bitmapset* bms2,PLpgSQL_datum** datums){

    if(bms1 == NULL || bms2 == NULL){
        return 0;
    }

    /* create the intersection of the two bitmapsets */
    Bitmapset* intersection = bms_intersect(bms1,bms2);

    /* iterate them */
    int dno;
    while (intersection != NULL &&  !bms_is_empty(intersection) && (dno = bms_first_member(intersection)) >= 0){
        /* if the intersection has a dno of type PLPGSQL_DTYPE_VAR bms1 and bms2 contain the same variable */
        if(datums[dno]->dtype == PLPGSQL_DTYPE_VAR)
            return 1;
    }
    return 0;
}
