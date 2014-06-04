#include "plpgsql.h"
#include "nodes/pg_list.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <igraph/igraph.h>
#include "pg_plsql_graphs.h"
#include "storage/fd.h"
/**
 * Prints out the variables that staments read and write to
 */
void printReadsAndWrites(igraph_t* graph, long nodeid, Datum* arguments, Datum* result, bool breakIfFound){


    PLpgSQL_function* function = getIGraphGlobalAttrP(graph,"function");

    printf("\n\nOn: %s\n",VAS(graph,"label",nodeid));

    Bitmapset* bmsRead = bms_copy(getIGraphNodeAttrP(graph,"read",nodeid));
    int dno;
    while (bmsRead != NULL && !bms_is_empty(bmsRead) && (dno = bms_first_member(bmsRead)) >= 0){
        PLpgSQL_datum *datum = function->datums[dno];

        if (datum->dtype == PLPGSQL_DTYPE_VAR)
        {
            PLpgSQL_var *var = (PLpgSQL_var *) datum;
            printf("Reading: %s\n",var->refname);
        }
    }



    Bitmapset* bmsWrite = bms_copy(getIGraphNodeAttrP(graph,"write",nodeid));
    while (bmsWrite != NULL && !bms_is_empty(bmsWrite) && (dno = bms_first_member(bmsWrite)) >= 0){
        PLpgSQL_datum *datum = function->datums[dno];

        if (datum->dtype == PLPGSQL_DTYPE_VAR)
        {
            PLpgSQL_var *var = (PLpgSQL_var *) datum;
            printf("Writing: %s\n",var->refname);
        }
    }

}


/**
 * Append Node data to the dot string buffer
 */
void appendNodeToDot(igraph_t* graph, long nodeid, Datum* arguments, Datum* result, bool breakIfFound){
    char* buf = DatumGetPointer(arguments[0]);
    char* additionalAttributes = DatumGetCString(arguments[1]);

    /* if a label vertex attribute is given draw it to the current node */
    sprintf(eos(buf),"%li[label=\"%s\"]", nodeid, VAS(graph,"label",nodeid));
    if(additionalAttributes != NULL)
        sprintf(eos(buf),"%s",additionalAttributes);
    sprintf(eos(buf),";\n");
}


/**
 * Append Edge to the dot string buffer
 */
void appendEdgeToDot(igraph_t* graph, long eid, long from, long to, Datum* arguments, Datum* result, bool breakIfFound){
    /* The buffer to append the dot data to */
    char* buf = DatumGetPointer(arguments[0]);
    /* show labels attribute */
    bool showLabels = DatumGetBool(arguments[1]);
    /* edge types */
    List* edgeTypes = (List*)DatumGetPointer(arguments[2]);


    ListCell* cell;
    /* iterate given edge types that need to be added to the dot */
    foreach(cell, edgeTypes){
        List* edgeData = lfirst(cell);

        /* of the edge has the current edge type type append the dot data */
        if(strcmp(getIGraphEdgeAttrS(graph,"type",eid),linitial(edgeData)) == 0){
            /* add edge */
            sprintf(eos(buf),"%li -> %li",from,to);
            /* penwidth */
            sprintf(eos(buf),"[penwidth=0.4]");
            /* if show labes attribute is set -> add them */
            if(showLabels){

                const char* label = getIGraphEdgeAttrS(graph,"label",eid);

                if(label != NULL){
                    sprintf(eos(buf),"[label=\"\%s\"]",label);
                }
            }
            /* add the color */
            sprintf(eos(buf),"[color=%s]",lsecond(edgeData));

            /* additional properties */
            if(edgeData->length > 2){
                sprintf(eos(buf),"%s",lthird(edgeData));
            }

        }

    }
}



/**
 *  create Rank
 */
void buildRank(igraph_t* graph, long nodeid, Datum* arguments, Datum* result, bool lastElement){
    char* buf = DatumGetPointer(*arguments);

    /* if a label vertex attribute is given draw it to the current node */
    sprintf(eos(buf),"%li",nodeid);


    if(lastElement)
        sprintf(eos(buf),";");
    else
        sprintf(eos(buf),",");
}


/**
 * Converts the given igraph to dot format
 */
char* convertGraphToDotFormat(  igraph_t* graph,
                                List* edgeTypes,
                                bool edgeLabels,
                                bool sameLevel,
                                char* additionalGeneralConfiguration,
                                char* additionalNodeConfiguration,
                                int maxDotFileSize){

    /* allocate space for the dot string */
    char* buf = palloc(maxDotFileSize);
    /* datum for dot string */
    Datum bufDatum = PointerGetDatum(buf);


    strcpy(buf, "");

    /* start of digraph with a little configuration */
    sprintf(buf,"digraph g {\n");
    sprintf(eos(buf),"nodesep=0.3;\n");
    sprintf(eos(buf),"graph[pad=\"0.20,0.20\"];\n");
    sprintf(eos(buf),"edge[arrowsize=0.6,penwidth=0.6];\n");
    sprintf(eos(buf),"node[fontsize=10];\n");
    if(additionalGeneralConfiguration != NULL)
        sprintf(eos(buf),"%s",additionalGeneralConfiguration);

    /* arguments for nodes */
    Datum datumsNodes[2];
    datumsNodes[0] = bufDatum;                                      /* string buffer */
    datumsNodes[1] = CStringGetDatum(additionalNodeConfiguration);  /* Additional node conf */


    /* iterate over igraph nodes and execute covertNodeLabelToDot to create labels for nodes */
    iterateIGraphNodes(graph,&appendNodeToDot,datumsNodes,NULL,0);


    /* Arguments for edges */
    Datum datumsEdges[3];
    datumsEdges[0] = bufDatum;                      /* string buffer */
    datumsEdges[1] = BoolGetDatum(edgeLabels);      /* edge labels */
    datumsEdges[2]  = PointerGetDatum(edgeTypes);   /* visible edge types */

    /* iterate over edges and execute convertEdgeLabelToDot to create edges */
    iterateReachableEdges(graph,&appendEdgeToDot,datumsEdges,NULL,0);

    /* put nodes on the same level */
    if(sameLevel){
        sprintf(eos(buf),"\n{rank=same; ");
        iterateIGraphNodes(graph,&buildRank,&bufDatum,NULL,0);
        sprintf(eos(buf),"}\n");
    }


    /* finish the graph */
    sprintf(eos(buf),"}");

    return buf;
}



