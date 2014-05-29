#include "plpgsql.h"
#include "nodes/pg_list.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <igraph/igraph.h>



/**
 * Custom Graph data types
 */
struct edge{
	long int sourceid;
	long int targetid;
	char* label;
};


struct node{
	long int key;
	List* edges;
	char* label;
	PLpgSQL_stmt* stmt;
};

struct graph_status{
	List* parents;
	List* nodes;
};

union dblPointer{
	double doublevalue;
	long int longvalue;
	void* pointer;
};

/**********************************************************************
 * Function declarations
 **********************************************************************/

/* ----------
 * Functions in pg_plsql_graphs.c
 * ----------
 */
void createGraph(PLpgSQL_function* function,PLpgSQL_execstate *estate);

/* ----------
 * Functions in pg_plsql_graph_ops.c
 * ----------
 */
void connectNodeToParents(int nodeid, List* parents);
struct node* getNodeById(List* nodes,long int currentId);
struct edge* getNthEdgeFromNode(List* nodes, long int nodeid, int n);


/* ----------
 * Functions in pg_plsql_plstmts2graph.c
 * ----------
 */
char* varnumberToVarname(int varno,PLpgSQL_datum** datums, int ndatums);
struct graph_status* initStatus(int initnodeid);
void appendNewNodeAndConnectParents(int* newnodeid,
									struct graph_status* status,
									PLpgSQL_stmt* stmt);
void createProgramGraph(int* newnodeid,
					struct graph_status* status,
					List* statements,
					PLpgSQL_function* function);

/* ----------
 * Functions in pg_plsql_graph2igraph.c
 * ----------
 */
igraph_t* buildIGraph(List* nodes,PLpgSQL_function* function,PLpgSQL_execstate * estate);

/* ----------
 * Functions in pg_plsql_igraph_export.c
 * ----------
 */
void printReadsAndWrites(igraph_t* graph);
char* convertProgramDependecGraphToDotFormat(	igraph_t* 	graph,
												int 		sameLevel,
												int 		maxDotFileSize);
char* convertFlowGraphToDotFormat(igraph_t* graph, int maxDotFileSize);
/* ----------
 * Functions in pg_plsql_igraphanalysis.c
 * ----------
 */
void addLabels(int nodeid, igraph_t* igraph);
Bitmapset* getParametersOfQueryExpr(PLpgSQL_expr* 		expr,
									PLpgSQL_function* 	surroundingFunction,
									PLpgSQL_execstate* 	estate);
void setReadsAndWrites(int nodeid, igraph_t* igraph);
void createProgramDependenceGraph(igraph_t*  igraph);
int getNodeNumberToStmt(PLpgSQL_stmt* stmt1, igraph_t* graph);
int dependenceConflict(int node1, int node2, igraph_t* igraph);
int conflict(PLpgSQL_stmt* stmt1, PLpgSQL_stmt* stmt2, igraph_t* igraph);


/* ----------
 * Functions in pg_plsql_list_ops.c
 * ----------
 */
List* copy_list(List* parents);

/* ----------
 * Functions in pg_plsql_string_ops.c
 * ----------
 */
void removeSubstring(char *s,const char *toremove);
char* concatStrings2(char* str1, char* str2);
char* concatStrings3(char* str1, char* str2, char* str3);
char* removeFromString(char* string,char* toRemove);
char* removeFromStringN(const char* string,char* toRemove);



/* ----------
 * Functions in pg_plsql_expr_evaluation.c
 * ----------
 */
void pg_plsql_parser_setup(struct ParseState *pstate, PLpgSQL_expr *expr);