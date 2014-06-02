#include "plpgsql.h"
#include "nodes/pg_list.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <igraph/igraph.h>
#include "pg_plsql_graphs.h"
#include "storage/fd.h"
#define eos(s) ((s)+strlen(s))

/**
 * Prints out the variables that staments read and write to
 */
void printReadsAndWrites(igraph_t* graph, long nodeid, Datum* arguments, Datum* result){


    PLpgSQL_function* function = getIGraphGlobalAttrP(graph,"function");

    printf("\n\nOn: %s\n",VAS(graph,"label",nodeid));

    Bitmapset* bms = bms_copy(getIGraphNodeAttrP(graph,"read",nodeid));
    int dno;
    while (bms != NULL && !bms_is_empty(bms) && (dno = bms_first_member(bms)) >= 0){
        PLpgSQL_datum *datum = function->datums[dno];

        if (datum->dtype == PLPGSQL_DTYPE_VAR)
        {
            PLpgSQL_var *var = (PLpgSQL_var *) datum;
            printf("Reading: %s\n",var->refname);
        }
    }

    int* writeDnos = getIGraphNodeAttrP(graph,"write",nodeid);
    long nwrite  = getIGraphNodeAttrL(graph,"nwrite",nodeid);

    for(int i = 0; i < nwrite; i++)
    {

        PLpgSQL_datum *datum = function->datums[writeDnos[i]];
        if (datum->dtype == PLPGSQL_DTYPE_VAR)
        {
            PLpgSQL_var *var = (PLpgSQL_var *) datum;
            printf("Writing: %s\n",var->refname);
        }
    }
}



void covertNodeLabelToDot(igraph_t* graph, long nodeid, Datum* arguments, Datum* result){
    char* buf = DatumGetPointer(arguments[0]);
    char* additionalAttributes = DatumGetCString(arguments[1]);

    /* if a label vertex attribute is given draw it to the current node */
    sprintf(eos(buf),"%li[label=\"%s\"]", nodeid, VAS(graph,"label",nodeid));
    if(additionalAttributes != NULL)
        sprintf(eos(buf),"%s",additionalAttributes);
    sprintf(eos(buf),";\n");
}


void convertEdgeLabelToDot(igraph_t* graph, long eid, long from, long to, Datum* arguments, Datum* result){
    char* buf = DatumGetPointer(arguments[0]);
    bool showLabels = DatumGetBool(arguments[1]);

    List* list = DatumGetPointer(arguments[2]);


    ListCell* cell;
    foreach(cell, list){
        List* l = lfirst(cell);


        if(strcmp(EAS(graph,"type",eid),linitial(l)) == 0){
            sprintf(eos(buf),"%li -> %li",from,to);
            sprintf(eos(buf),"[penwidth=0.4]");
            if(showLabels){
                sprintf(eos(buf),"[label=\"\%s\"]",EAS(graph,"label",eid));
            }
            sprintf(eos(buf),"[color=%s]",lsecond(l));


            if(l->length > 2){
                sprintf(eos(buf),"%s",lthird(l));
            }

        }

    }




}




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
 * Converts the given igraph program dependence graph to dot format
 */
char* convertProgramDependecGraphToDotFormat(    igraph_t* graph,
                                                int sameLevel,
                                                int maxDotFileSize){

    char* buf = palloc(maxDotFileSize);
    Datum bufDatum = PointerGetDatum(buf);
    strcpy(buf,"");


    /* start of digraph */
    sprintf(eos(buf),"digraph g {\n");/* splines=ortho; */
    sprintf(eos(buf),"splines=ortho;\n");
    sprintf(eos(buf),"nodesep=0.3;\n");
    sprintf(eos(buf),"graph[pad=\"0.20,0.20\"];\n");
    sprintf(eos(buf),"edge[arrowsize=0.6,penwidth=0.6];\n");
    sprintf(eos(buf),"node[fontsize=10];\n");




    Datum datumsNodes[2];
    datumsNodes[0] = bufDatum;
    datumsNodes[1] = CStringGetDatum(NULL);

    iterateIGraphNodes(graph,&covertNodeLabelToDot,datumsNodes,NULL,0);

    Datum datums[7];
    datums[0] = bufDatum;
    datums[1] = BoolGetDatum(0);


    List* types = list_make4(
                                list_make3("FLOW",         "black", "[style=dashed]"),
                                list_make2("RW-DEPENDENCE","blue"),
                                list_make2("WR-DEPENDENCE","green"),
                                list_make2("WW-DEPENDENCE","red"));

    datums[2]  = PointerGetDatum(types);

    iterateReachableEdges(graph,&convertEdgeLabelToDot,datums,NULL,0);





    if(sameLevel){
        sprintf(eos(buf),"\n{rank=same; ");

        iterateIGraphNodes(graph,&buildRank,&bufDatum,NULL,0);


        sprintf(eos(buf),"}\n");
    }


    /* finish the graph */
    sprintf(eos(buf),"}");

    return buf;

}

/**
 * Converts the given igraph flow graph to dot format
 */
char* convertFlowGraphToDotFormat(igraph_t* graph, int maxDotFileSize){

    char* buf = palloc(maxDotFileSize);
    Datum bufDatum = PointerGetDatum(buf);


    strcpy(buf, "");

    /* start of digraph */
    sprintf(buf,"digraph g {\n");/*splines=ortho; */
    sprintf(eos(buf),"nodesep=0.3;\n");
    sprintf(eos(buf),"graph[pad=\"0.20,0.20\"];\n");
    sprintf(eos(buf),"edge[arrowsize=0.6,penwidth=0.6];\n");
    sprintf(eos(buf),"node[fontsize=10];\n");

    Datum datumsNodes[2];
    datumsNodes[0] = bufDatum;
    datumsNodes[1] = CStringGetDatum("[shape=box]");

    iterateIGraphNodes(graph,&covertNodeLabelToDot,datumsNodes,NULL,0);


    Datum datumsEdges[3];
    datumsEdges[0] = bufDatum;
    datumsEdges[1] = BoolGetDatum(1);
    List* types = list_make1(
                                list_make2("FLOW",         "black"));

    datumsEdges[2]  = PointerGetDatum(types);
    iterateReachableEdges(graph,&convertEdgeLabelToDot,datumsEdges,NULL,0);


    /* finish the graph */
    sprintf(eos(buf),"}");

    return buf;
}



