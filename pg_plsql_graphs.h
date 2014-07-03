#include "plpgsql.h"
#include "nodes/pg_list.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <igraph/igraph.h>


#define MAXDOTFILESIZE 4096

#define eos(s) ((s)+strlen(s))




/**********************************************************************
 * Function declarations
 **********************************************************************/

/* ----------
 * Functions in pg_plsql_graphs.c
 * ----------
 */
void createGraph(PLpgSQL_function* function,PLpgSQL_execstate *estate);


