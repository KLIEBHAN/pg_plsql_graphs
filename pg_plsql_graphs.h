#include "plpgsql.h"
#include "nodes/pg_list.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <igraph/igraph.h>


//* this is the definition of the anonymous function */
#define lambda(l_ret_type, l_arguments, l_body)         \
({                                                      \
 l_ret_type l_anonymous_functions_name l_arguments      \
 l_body                                                 \
 &l_anonymous_functions_name;                           \
 })


#define MAXDOTFILESIZE 4096

#define eos(s) ((s)+strlen(s))


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
 * Functions in pg_plsql_stmt_ops.c
 * ----------
 */
char* varnumberToVarname(int varno,PLpgSQL_datum** datums, int ndatums);
bool containsSameVariable(Bitmapset* bms1,Bitmapset* bms2,PLpgSQL_function* function);

/* ----------
 * Functions in pg_plsql_graph_ops.c
 * ----------
 */
void connectNodeToParents(int nodeid, List* parents);
struct node* getNodeById(List* nodes,long int currentId);
struct edge* getNthEdgeFromNode(List* nodes, long int nodeid, int n);


/* ----------
 * Functions in pg_plsql_plstmts2igraph.c
 * ----------
 */
igraph_t* createFlowGraph(PLpgSQL_function* function,PLpgSQL_execstate *estate);
struct graph_status* initStatus(int initnodeid);
void appendNewNodeAndConnectParents(int* newnodeid,
                                    struct graph_status* status,
                                    PLpgSQL_stmt* stmt);
void createProgramGraph(int* newnodeid,
                    struct graph_status* status,
                    List* statements,
                    PLpgSQL_function* function);
igraph_t* buildIGraph(List* nodes,PLpgSQL_function* function,PLpgSQL_execstate * estate);

/* ----------
 * Functions in pg_plsql_igraph_export.c
 * ----------
 */
void printReadsAndWrites(igraph_t* graph, long nodeid, Datum* arguments, Datum* result, bool breakIfFound);
char* convertGraphToDotFormat(  igraph_t* graph,
                                List* edgeTypes,
                                bool edgeLabels,
                                bool sameLevel,
                                char* additionalGeneralConfiguration,
                                char* additionalNodeConfiguration,
                                int maxDotFileSize);
void appendNodeToDot(igraph_t* graph, long nodeid, Datum* arguments, Datum* result, bool breakIfFound);
void appendEdgeToDot(igraph_t* graph, long eid, long from, long to, Datum* arguments, Datum* result, bool breakIfFound);
void buildRank(igraph_t* graph, long nodeid, Datum* arguments, Datum* result, bool lastElement);
void iterateEdgesBridge(igraph_t* igraph, long nodeid, Datum* arguments, Datum* results, bool breakIfFound);


/* ----------
 * Functions in pg_plsql_igraph_ops.c
 * ----------
 */
void setIGraphGlobalAttrP(igraph_t* igraph, const char* name, void* pointer);
void* getIGraphGlobalAttrP(igraph_t* igraph, const char* name);
void setIGraphGlobalAttrL(igraph_t* igraph, const char* name, long value);
long getIGraphGlobalAttrL(igraph_t* igraph, const char* name);
void setIGraphGlobalAttrS(igraph_t* igraph, const char* name, char* value);
const char* getIGraphGlobalAttrS(igraph_t* igraph, const char* name);
void setIGraphNodeAttrP(igraph_t* igraph, const char* name, long nodeid, void* pointer);
void* getIGraphNodeAttrP(igraph_t* igraph, const char* name, long nodeid);
void setIGraphNodeAttrL(igraph_t* igraph, const char* name, long nodeid, long value);
long getIGraphNodeAttrL(igraph_t* igraph, const char* name, long nodeid);
void setIGraphNodeAttrS(igraph_t* igraph, const char* name, long nodeid, char* string);
const char* getIGraphNodeAttrS(igraph_t* igraph, const char* name, long nodeid);
void setIGraphEdgeAttrS(igraph_t* igraph, const char* name, long edgeid, char* string);
const char* getIGraphEdgeAttrS(igraph_t* igraph, const char* name, long edgeid);
void addEdgeWithAttr(igraph_t* igraph, long sourceNodeId, long targetNodeId, char* attr, char* value);
void iterateIGraphNodes(igraph_t* igraph, void (*callback)(igraph_t*, long, Datum*, Datum*, bool),Datum* arguments, Datum* results, bool breakIfFound);
void iterateIGraphOutEdges(igraph_t* igraph, long nodeid, void (*callback)(igraph_t*, long, long, long, Datum*, Datum*, bool),Datum* arguments, Datum* results, bool breakIfFound);
void iterateReachableEdges(igraph_t* graph, void (*callback)(igraph_t*, long, long, long, Datum*, Datum*, bool),Datum* arguments, Datum* results, bool breakIfFound);
/* ----------
 * Functions in pg_plsql_igraphanalysis.c
 * ----------
 */
void addLabels(int nodeid, igraph_t* igraph);
void setReadsAndWrites(int nodeid, igraph_t* igraph);
void addDependenceEges(igraph_t* igraph, long nodeid, Datum* argument, Datum* result, bool lastElem);
void retrieveNodeNumber(igraph_t* igraph, long nodeId, Datum* argument, Datum* result, bool lastElem);
long getNodeNumberToStmt(PLpgSQL_stmt* stmt1, igraph_t* graph);
int dependenceConflict(int node1, int node2, igraph_t* igraph);
int conflict(PLpgSQL_stmt* stmt1, PLpgSQL_stmt* stmt2, igraph_t* igraph);
void addProgramDependenceEdges(igraph_t* igraph);

/* ----------
 * Functions in pg_plsql_list_ops.c
 * ----------
 */
List* copy_list(List* parents);
Bitmapset* intArrayToBitmapSet(int* array, int size);

/* ----------
 * Functions in pg_plsql_string_ops.c
 * ----------
 */
void removeSubstring(char *s,const char *toremove);
char* removeFromString(char* string,char* toRemove);
char* removeFromStringN(const char* string,char* toRemove);



/* ----------
 * Functions in pg_plsql_expr_evaluation.c
 * ----------
 */

Bitmapset* getParametersOfQueryExpr(PLpgSQL_expr*         expr,
                                    PLpgSQL_function*     surroundingFunction,
                                    PLpgSQL_execstate*     estate);
void pg_plsql_parser_setup(struct ParseState *pstate, PLpgSQL_expr *expr);
