#include "plpgsql.h"
#include "nodes/pg_list.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include "pg_plsql_graphs.h"


/**
 * Copys a list
 */
List* copy_list(List* parents){
    List* newList = NIL;
    if(parents != NIL){
        newList = list_copy(parents);
    }
    return newList;
}
