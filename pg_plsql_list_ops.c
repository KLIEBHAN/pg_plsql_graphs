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


Bitmapset* intArrayToBitmapSet(int* array, int size){
    Bitmapset* bms = NULL;
    for(int i=0;i<size;i++){
        bms = bms_add_member(bms,array[i]);
    }
    return bms;
}
