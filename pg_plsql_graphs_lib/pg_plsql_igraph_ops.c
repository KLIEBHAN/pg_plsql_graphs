#include "plpgsql.h"
#include "nodes/pg_list.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <igraph/igraph.h>
#include "pg_plsql_graphs_lib.h"




void setIGraphGlobalAttrP(igraph_t* igraph, const char* name, void* pointer){
    union dblPointer data;
    data.pointer = pointer;;

    if(data.doublevalue == data.doublevalue)
        SETGAN(igraph,name,data.doublevalue);
}


void* getIGraphGlobalAttrP(igraph_t* igraph, const char* name){
    union dblPointer data;
    data.doublevalue = GAN(igraph,name);

    if(data.doublevalue == data.doublevalue)/* valid pointer */
        return data.pointer;
    else
        return NULL;
}



void setIGraphGlobalAttrL(igraph_t* igraph, const char* name, long value){
    union dblPointer data;
    data.longvalue = value;

    SETGAN(igraph,name,data.doublevalue);
}

long getIGraphGlobalAttrL(igraph_t* igraph, const char* name){
    union dblPointer data;
    data.doublevalue = GAN(igraph,name);

    if(data.doublevalue == data.doublevalue)
        return data.longvalue;
    else
        return 0;
}


void setIGraphGlobalAttrS(igraph_t* igraph, const char* name, char* value){

    SETGAS(igraph,name,value);
}

const char* getIGraphGlobalAttrS(igraph_t* igraph, const char* name){
    return GAS(igraph,name);
}


void setIGraphNodeAttrP(igraph_t* igraph, const char* name, long nodeid, void* pointer){
    union dblPointer data;
    data.pointer = pointer;


    if(data.doublevalue == data.doublevalue)/* valid pointer */
        SETVAN(igraph,name,nodeid,data.doublevalue);
}

void* getIGraphNodeAttrP(igraph_t* igraph, const char* name, long nodeid){
    union dblPointer data;
    data.doublevalue = VAN(igraph,name,nodeid);


    if(data.doublevalue == data.doublevalue)
        return data.pointer;
    else
        return NULL;
}


void setIGraphNodeAttrL(igraph_t* igraph, const char* name, long nodeid, long value){
    union dblPointer data;
    data.longvalue = value;


    SETVAN(igraph,name,nodeid,data.doublevalue);
}


long getIGraphNodeAttrL(igraph_t* igraph, const char* name, long nodeid){
    union dblPointer data;
    data.doublevalue = VAN(igraph,name,nodeid);

    if(data.doublevalue == data.doublevalue)
        return data.longvalue;
    else
        return 0;
}


void setIGraphNodeAttrS(igraph_t* igraph, const char* name, long nodeid, char* string){
    SETVAS(igraph,name,nodeid,string);
}


const char* getIGraphNodeAttrS(igraph_t* igraph, const char* name, long nodeid){
    return VAS(igraph,name,nodeid);
}


void setIGraphEdgeAttrS(igraph_t* igraph, const char* name, long edgeid, char* string){
    SETEAS(igraph,name,edgeid,string);
}


const char* getIGraphEdgeAttrS(igraph_t* igraph, const char* name, long edgeid){
    return EAS(igraph,name,edgeid);
}



void addEdgeWithAttr(igraph_t* igraph, long sourceNodeId, long targetNodeId, char* attr, char* value){

    igraph_integer_t eid;
    /* Add edge to the graph */
    igraph_add_edge(igraph,sourceNodeId,targetNodeId);
    igraph_get_eid(igraph, &eid,sourceNodeId,targetNodeId,1,0);
    /* set dependence attribute */
    setIGraphEdgeAttrS(igraph,attr,eid,value);
}


/**
 * iterates over the iGraph Nodes and calls a given callback function
 */
void iterateIGraphNodes(igraph_t* igraph, void (*callback)(igraph_t*, long, Datum*, Datum*, bool),Datum* arguments, Datum* results, bool breakIfFound){

    /* init and create a node iterator over all nodes */
    igraph_vit_t nodeit;
    igraph_vs_t allNodes;
    igraph_vs_all(&allNodes);
    igraph_vit_create(igraph,allNodes, &nodeit);


    /* iterate over the nodes */
    while (!IGRAPH_VIT_END(nodeit)) {
        /* current node id */
        long int nodeid = (long int) IGRAPH_VIT_GET(nodeit);


        /* next iteration over graph nodes */
        IGRAPH_VIT_NEXT(nodeit);



        /* call the callback function */
        (*callback)(igraph,nodeid,arguments,results,IGRAPH_VIT_END(nodeit));

        if(breakIfFound && DatumGetPointer(*results) != NULL){
            break;
        }

    }
    /* destroy the graph iterator */
    igraph_vit_destroy(&nodeit);
    igraph_vs_destroy(&allNodes);
}

/**
 * iterates over the iGraph Edges To the given node and calls a given callback function
 */
void iterateIGraphOutEdges(igraph_t* igraph, long nodeid, void (*callback)(igraph_t*, long, long, long, Datum*, Datum*, bool),Datum* arguments, Datum* results, bool breakIfFound){

    /* init and create a iterator over the neighbor node that lie on outgoing edges */
    igraph_es_t es;
    igraph_eit_t eit;
    igraph_es_incident(&es, nodeid, IGRAPH_OUT);
    igraph_eit_create(igraph, es, &eit);
    /* iterate over the neighbor node that lie on outgoing edges */
    while (!IGRAPH_VIT_END(eit)) {
        /* get the edge id */
        igraph_integer_t eid = IGRAPH_EIT_GET(eit);


        igraph_integer_t from;
        igraph_integer_t to;
        igraph_edge(igraph, eid, &from, &to);


        /* next iteration over neighbor nodes */
        IGRAPH_EIT_NEXT(eit);


        (*callback)(igraph,eid,from,to,arguments,results,IGRAPH_VIT_END(eit));

        if(breakIfFound && DatumGetPointer(*results) != NULL){
            break;
        }
    }
    /* destroy the neighbor iterator */
    igraph_eit_destroy(&eit);
    igraph_es_destroy(&es);/* call the callback function */
}


void iterateEdgesBridge(igraph_t* igraph, long nodeid, Datum* arguments, Datum* results, bool breakIfFound){
    iterateIGraphOutEdges(igraph,nodeid,(void (*)(igraph_t *, long, long, long, Datum *, Datum *, bool))DatumGetPointer(arguments[0]),(Datum*)DatumGetPointer(arguments[1]),results,breakIfFound);
}


void iterateReachableEdges(igraph_t* graph, void (*callback)(igraph_t*, long, long, long, Datum*, Datum*, bool),Datum* arguments, Datum* results, bool breakIfFound){

    Datum datums[2];

    datums[0] = PointerGetDatum(callback);
    datums[1] = PointerGetDatum(arguments);

    iterateIGraphNodes(graph,&iterateEdgesBridge,datums,results,breakIfFound);
}
