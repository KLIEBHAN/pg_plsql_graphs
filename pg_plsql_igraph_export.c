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
void printReadsAndWrites(igraph_t* graph){

    /* init and create a node iterator over all nodes */
    igraph_vit_t nodeit;
    igraph_vs_t allNodes;
    igraph_vs_all(&allNodes);
    igraph_vit_create(graph,allNodes, &nodeit);

    union dblPointer func;
    func.doublevalue = GAN(graph,"function");
    PLpgSQL_function* function = func.pointer;


    /* iterate over the nodes */
    while (!IGRAPH_VIT_END(nodeit)) {
        /* current node id */
        long int nodeid = (long int) IGRAPH_VIT_GET(nodeit);

        printf("\n\nOn: %s\n",VAS(graph,"label",nodeid));

        union dblPointer data;
        data.doublevalue = VAN(graph,"read",nodeid);
        /* filter for NaN */
        if(data.doublevalue == data.doublevalue){
            Bitmapset* bms = data.pointer;
            int dno;
            while (!bms_is_empty(bms) && (dno = bms_first_member(bms)) >= 0){
                PLpgSQL_datum *datum = function->datums[dno];

                if (datum->dtype == PLPGSQL_DTYPE_VAR)
                {
                    PLpgSQL_var *var = (PLpgSQL_var *) datum;
                    printf("Reading: %s\n",var->refname);
                }
            }

        }
        data.doublevalue = VAN(graph,"write",nodeid);


        /* filter for NaN */
        if(data.doublevalue == data.doublevalue){

            int* writeDnos = data.pointer;

            data.doublevalue  = VAN(graph,"nwrite",nodeid);

            for(int i = 0; i < data.longvalue; i++)
            {

                PLpgSQL_datum *datum = function->datums[writeDnos[i]];
                if (datum->dtype == PLPGSQL_DTYPE_VAR)
                {
                    PLpgSQL_var *var = (PLpgSQL_var *) datum;
                    printf("Writing: %s\n",var->refname);
                }
            }

        }
        /* next iteration over graph nodes */
        IGRAPH_VIT_NEXT(nodeit);
    }
    /* destroy the graph iterator */
    igraph_vit_destroy(&nodeit);
    igraph_vs_destroy(&allNodes);
}

/**
 * Converts the given igraph program dependence graph to dot format
 */
char* convertProgramDependecGraphToDotFormat(    igraph_t* graph,
                                                int sameLevel,
                                                int maxDotFileSize){

    char* buf = palloc(maxDotFileSize);
    strcpy(buf,"");


    /* start of digraph */
    sprintf(eos(buf),"digraph g {\n");/* splines=ortho; */
    sprintf(eos(buf),"splines=ortho;\n");
    sprintf(eos(buf),"nodesep=0.3;\n");
    sprintf(eos(buf),"graph[pad=\"0.20,0.20\"];\n");
    sprintf(eos(buf),"edge[arrowsize=0.6,penwidth=0.6];\n");
    sprintf(eos(buf),"node[fontsize=10];\n");



    /* init and create a node iterator over all nodes */
    igraph_vit_t nodeit;
    igraph_vs_t allNodes;
    igraph_vs_all(&allNodes);
    igraph_vit_create(graph,allNodes, &nodeit);


    /* iterate over the nodes */
    while (!IGRAPH_VIT_END(nodeit)) {
        /* current node id */
        long int nodeid = (long int) IGRAPH_VIT_GET(nodeit);

        /* if a label vertex attribute is given draw it to the current node */
        sprintf(eos(buf),"%li[label=\"%s\"];\n", nodeid, VAS(graph,"label",nodeid));



        /* next iteration over graph nodes */
        IGRAPH_VIT_NEXT(nodeit);


        if(!IGRAPH_VIT_END(nodeit)){
            sprintf(eos(buf),
                        "%li -> %li[style=invis];\n",
                        nodeid,
                        (long int) IGRAPH_VIT_GET(nodeit));
        }

    }



    /* destroy the graph iterator */
    igraph_vit_destroy(&nodeit);
    igraph_vit_create(graph,allNodes, &nodeit);

    /* iterate over the nodes */
    while (!IGRAPH_VIT_END(nodeit)) {
        /* current node id */
        long int nodeid = (long int) IGRAPH_VIT_GET(nodeit);

        /* if a label vertex attribute is given draw it to the current node */
        if(igraph_cattribute_has_attr(graph,IGRAPH_ATTRIBUTE_VERTEX,"label")){
            sprintf(eos(buf),"%li[label=\"%s\"];\n", nodeid, VAS(graph,"label",nodeid));
        }
        /* init and create a iterator over the neighbor node that lie on outgoing edges */
        igraph_es_t es;
        igraph_eit_t eit;
        igraph_es_incident(&es, nodeid, IGRAPH_OUT);
        igraph_eit_create(graph, es, &eit);
        /* iterate over the neighbor node that lie on outgoing edges */
        while (!IGRAPH_VIT_END(eit)) {
            /* get the edge id */
            igraph_integer_t eid = IGRAPH_EIT_GET(eit);


            igraph_integer_t from;
            igraph_integer_t to;
            igraph_edge(graph, eid, &from, &to);

            if(strcmp(EAS(graph,"type",eid),"FLOW") == 0){
                /* print the edge */
                sprintf(eos(buf),"%i -> %i[style=dashed][penwidth=0.4];",from, to);
            }
            if(strcmp(EAS(graph,"type",eid),"RW-DEPENDENCE") == 0){
                /* print the edge */
                sprintf(eos(buf),"%i -> %i [label=\"\%s\"][color=blue];\n",
                                    from,
                                    to,
                                    EAS(graph,"label",eid));
            }
            if(strcmp(EAS(graph,"type",eid),"WR-DEPENDENCE") == 0){
                /* print the edge */
                sprintf(eos(buf),"%i -> %i [label=\"\%s\"][color=green];\n",
                                    from,
                                    to,
                                    EAS(graph,"label",eid));
            }


            if(strcmp(EAS(graph,"type",eid),"WW-DEPENDENCE") == 0){
                /* print the edge */
                sprintf(eos(buf),"%i -> %i [label=\"\%s\"][color=red];\n",
                                    from,
                                    to,
                                    EAS(graph,"label",eid));
            }


            /* next iteration over neighbor nodes */
            IGRAPH_EIT_NEXT(eit);
        }
        /* destroy the neighbor iterator */
        igraph_eit_destroy(&eit);
        igraph_es_destroy(&es);

        /* next iteration over graph nodes */
        IGRAPH_VIT_NEXT(nodeit);
    }



    /* destroy the graph iterator */
    igraph_vit_destroy(&nodeit);
    igraph_vit_create(graph,allNodes, &nodeit);


    if(sameLevel){
        sprintf(eos(buf),"\n{rank=same; ");
        /* iterate over the nodes */
        while (!IGRAPH_VIT_END(nodeit)) {
            /* current node id */
            long int nodeid = (long int) IGRAPH_VIT_GET(nodeit);
            sprintf(eos(buf),"%li",nodeid);

            /* if a label vertex attribute is given draw it to the current node */
            /* next iteration over graph nodes */
            IGRAPH_VIT_NEXT(nodeit);

            if(!IGRAPH_VIT_END(nodeit))
                sprintf(eos(buf),",");
            else
                sprintf(eos(buf),";");

        }

        sprintf(eos(buf),"}\n");
    }

    igraph_vs_destroy(&allNodes);

    /* finish the graph */
    sprintf(eos(buf),"}");

    return buf;

}

/**
 * Converts the given igraph flow graph to dot format
 */
char* convertFlowGraphToDotFormat(igraph_t* graph, int maxDotFileSize){

    char* buf = palloc(maxDotFileSize);
    strcpy(buf, "");

    /* start of digraph */
    sprintf(buf,"digraph g {\n");/*splines=ortho; */

    /* init and create a node iterator over all nodes */
    igraph_vit_t nodeit;
    igraph_vs_t allNodes;
    igraph_vs_all(&allNodes);
    igraph_vit_create(graph,allNodes, &nodeit);


    /* iterate over the nodes */
    while (!IGRAPH_VIT_END(nodeit)) {
        /* current node id */
        long int nodeid = (long int) IGRAPH_VIT_GET(nodeit);

        /* if a label vertex attribute is given draw it to the current node */
        if(igraph_cattribute_has_attr(graph,IGRAPH_ATTRIBUTE_VERTEX,"label")){
            sprintf(eos(buf),
                        "%li[label=\"%s\"][shape=box];\n",
                        nodeid,
                        VAS(graph,"label",nodeid));
        }

        /* init and create a iterator over the neighbor node that lie on outgoing edges */
        igraph_es_t es;
        igraph_eit_t eit;
        igraph_es_incident(&es, nodeid, IGRAPH_OUT);
        igraph_eit_create(graph, es, &eit);
        /* iterate over the neighbor node that lie on outgoing edges */
        while (!IGRAPH_VIT_END(eit)) {
            /* get the edge id */
            igraph_integer_t eid = IGRAPH_EIT_GET(eit);


            igraph_integer_t from;
            igraph_integer_t to;
            igraph_edge(graph, eid, &from, &to);

            if(strcmp(EAS(graph,"type",eid),"FLOW") == 0){
                /* print the edge */
                sprintf(eos(buf),"%i -> %i [label=\"\%s\"];\n",
                                    from,
                                    to,
                                    EAS(graph,"label",eid));
            }



            /* next iteration over neighbor nodes */
            IGRAPH_EIT_NEXT(eit);
        }
        /* destroy the neighbor iterator */
        igraph_eit_destroy(&eit);
        igraph_es_destroy(&es);

        /* next iteration over graph nodes */
        IGRAPH_VIT_NEXT(nodeit);
    }



    /* destroy the graph iterator */
    igraph_vit_destroy(&nodeit);
    igraph_vs_destroy(&allNodes);

    /* finish the graph */
    sprintf(eos(buf),"}");

    return buf;
}



