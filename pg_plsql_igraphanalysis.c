#include "server/plpgsql.h"
#include "nodes/pg_list.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <igraph/igraph.h>
#include "pg_plsql_graphs.h"


/**
 * adds labels to the current node and outgoing edges
 */
void addLabels(int nodeid, igraph_t* igraph){


	union dblPointer data;
	data.doublevalue = VAN(igraph,"stmt",nodeid);
	PLpgSQL_stmt* stmt = data.pointer;

	data.doublevalue = GAN(igraph,"function");
	PLpgSQL_function* function = data.pointer;

	igraph_vector_t eids;
	igraph_vector_init(&eids, 0);
	igraph_incident(igraph, &eids, nodeid, IGRAPH_OUT);

	char* label = NULL;

	if(nodeid == 0){
		label = "entry";
	}
	else if (stmt && stmt->cmd_type == PLPGSQL_STMT_ASSIGN) {
		PLpgSQL_stmt_assign* assignment  = ((PLpgSQL_stmt_assign*)stmt);
		PLpgSQL_expr* expr = assignment->expr;

		/* Concat Varibalename with := and the expression */
		label = concatStrings3(
									varnumberToVarname(	assignment->varno,
														function->datums,
														function->ndatums),
									" := ",
									expr->query);
		/* remove the SELECT */
		label = removeFromString(label,"SELECT ");

	}
	else if (stmt && stmt->cmd_type == PLPGSQL_STMT_RAISE) {
		label = "RAISE";
	}
	else if(stmt && stmt->cmd_type == PLPGSQL_STMT_IF){
		PLpgSQL_stmt_if* ifStmt  = ((PLpgSQL_stmt_if*)stmt);



		/* remove the SELECT of the condinational query */
		label = removeFromStringN(ifStmt->cond->query,"SELECT ");

		/* label the first edge with 1 */
		if(igraph_vector_size(&eids) > 0){
			SETEAS(igraph,"label",VECTOR(eids)[0],"1");
		}
		/* and the second with 0 */
		if(igraph_vector_size(&eids)> 1){
			SETEAS(igraph,"label",VECTOR(eids)[1],"0");
		}

	}

	else if(stmt && stmt->cmd_type == PLPGSQL_STMT_WHILE){
		PLpgSQL_stmt_while* whileStmt  = ((PLpgSQL_stmt_while*)stmt);
		/* remove the SELECT of the condinational query */
		label = removeFromStringN(whileStmt->cond->query,"SELECT ");

		/* label the first edge with 1 */
		if(igraph_vector_size(&eids) > 0){
			SETEAS(igraph,"label",VECTOR(eids)[0],"1");
		}
		/* and the second with 0 */
		if(igraph_vector_size(&eids)> 1){
			SETEAS(igraph,"label",VECTOR(eids)[1],"0");
		}

	}
	else if (stmt && stmt->cmd_type == PLPGSQL_STMT_RETURN) {

		PLpgSQL_stmt_return* returnStmt  = ((PLpgSQL_stmt_return*)stmt);
		/* concatinate RETURN with the return query */
		label = concatStrings2("RETURN ",returnStmt->expr->query);

		/* remove the SELECT */
		label = removeFromString(label,"SELECT ");
	}
	else if(stmt && stmt->cmd_type == PLPGSQL_STMT_EXECSQL) {

		PLpgSQL_stmt_execsql* execSqlStmt  = ((PLpgSQL_stmt_execsql*)stmt);

		label = palloc(100*sizeof(char));

		strcat(label,execSqlStmt->sqlstmt->query);
		strcat(label," INTO ");

		for(int i=0;i<execSqlStmt->row->nfields;i++){
			strcat(label,
					varnumberToVarname(	execSqlStmt->row->varnos[i],
										function->datums,
										function->ndatums));
			if(i != execSqlStmt->row->nfields-1){
				strcat(label,",");
			}

		}

	}
	if(label){
		/* set the label of the current node as attribute  */
		/* to the corresponding node of the igraph */
		SETVAS(igraph,"label",nodeid,label);
	}
}



/**
 * Workaround to get the parameters of a query as Bitmapset
 */
Bitmapset* getParametersOfQueryExpr(PLpgSQL_expr* 		expr,
									PLpgSQL_function* 	surroundingFunction,
									PLpgSQL_execstate* 	estate){

	expr->func = surroundingFunction;
	SPI_prepare_params(expr->query,
							  (ParserSetupHook) pg_plsql_parser_setup,
							  (void *) expr,
							  0);


	return expr->paramnos;
}



/**
 * sets the reads and writes of statements in the graph nodes
 */
void setReadsAndWrites(int nodeid, igraph_t* igraph){

	union dblPointer data;
	data.doublevalue = VAN(igraph,"stmt",nodeid);

	if(nodeid != 0){
		PLpgSQL_stmt* stmt = data.pointer;

		data.doublevalue = GAN(igraph,"function");
		PLpgSQL_function* function = data.pointer;


		data.doublevalue = GAN(igraph,"estate");
		PLpgSQL_execstate* estate = data.pointer;


		union dblPointer writeData;
		writeData.longvalue = 0;

		if (stmt && stmt->cmd_type && stmt->cmd_type == PLPGSQL_STMT_ASSIGN) {
			PLpgSQL_stmt_assign* assignment  = ((PLpgSQL_stmt_assign*)stmt);

			int* intPointer = &assignment->varno;

			writeData.pointer = intPointer;
			SETVAN(igraph,"write",nodeid,writeData.doublevalue);
			writeData.longvalue = 1;
			SETVAN(igraph,"nwrite",nodeid,writeData.doublevalue);

			data.pointer = bms_copy(getParametersOfQueryExpr(	assignment->expr,
																function,
																estate));
			SETVAN(igraph,"read",nodeid,data.doublevalue);
		}
		else if (stmt && stmt->cmd_type == PLPGSQL_STMT_RAISE) {
		}
		else if(stmt && stmt->cmd_type == PLPGSQL_STMT_IF){
			PLpgSQL_stmt_if* ifStmt  = ((PLpgSQL_stmt_if*)stmt);

			data.pointer = bms_copy(getParametersOfQueryExpr(ifStmt->cond,function,estate));
			SETVAN(igraph,"read",nodeid,data.doublevalue);


		}
		else if(stmt && stmt->cmd_type == PLPGSQL_STMT_WHILE){
			PLpgSQL_stmt_while* whileStmt  = ((PLpgSQL_stmt_while*)stmt);


			data.pointer = bms_copy(getParametersOfQueryExpr(	whileStmt->cond,
																function,
																estate));
			SETVAN(igraph,"read",nodeid,data.doublevalue);
		}
		else if (stmt && stmt->cmd_type == PLPGSQL_STMT_RETURN) {

			PLpgSQL_stmt_return* returnStmt  = ((PLpgSQL_stmt_return*)stmt);


			data.pointer = bms_copy(getParametersOfQueryExpr(	returnStmt->expr,
																function,
																estate));
			SETVAN(igraph,"read",nodeid,data.doublevalue);
		}

		else if(stmt && stmt->cmd_type == PLPGSQL_STMT_EXECSQL) {

			PLpgSQL_stmt_execsql* execSqlStmt  = ((PLpgSQL_stmt_execsql*)stmt);


			data.pointer = bms_copy(getParametersOfQueryExpr(	execSqlStmt->sqlstmt,
																function,
																estate));
			SETVAN(igraph,"read",nodeid,data.doublevalue);

			writeData.pointer = execSqlStmt->row->varnos;
			SETVAN(igraph,"write",nodeid,writeData.doublevalue);
			writeData.longvalue = execSqlStmt->row->nfields;
			SETVAN(igraph,"nwrite",nodeid,writeData.doublevalue);

		}
	}

}


void createProgramDependenceGraph(igraph_t*  igraph){


	union dblPointer func;
	func.doublevalue = GAN(igraph,"function");
	PLpgSQL_function* function = func.pointer;


	/* init and create a node iterator over all nodes */
	igraph_vit_t nodeit;
	igraph_vs_t allNodes;
	igraph_vs_all(&allNodes);
	igraph_vit_create(igraph,allNodes, &nodeit);

	/* iterate over the nodes */
	while (!IGRAPH_VIT_END(nodeit)) {
		/* current node id */
		long int nodeid = (long int) IGRAPH_VIT_GET(nodeit);

		union dblPointer data;
		data.doublevalue = VAN(igraph,"stmt",nodeid);
		PLpgSQL_stmt* stmt = data.pointer;


		data.doublevalue = VAN(igraph,"write",nodeid);
		int* writeDnos = data.pointer;
		data.doublevalue  = VAN(igraph,"nwrite",nodeid);
		long nwriteDnos = data.longvalue;

		if(data.doublevalue != data.doublevalue)
			writeDnos = NULL;


		if(writeDnos != NULL){

			igraph_vector_t order;
			igraph_vector_init(&order, 0);

			igraph_dfs(igraph, /*root=*/nodeid, /*neimode=*/IGRAPH_OUT,
					/*unreachable=*/0, &order, 0, 0, 0, 0, 0,0);




			for(int i=0;i<igraph_vector_size(&order);i++){
				unsigned int vid = VECTOR(order)[i];

				if(vid && nodeid != vid){
					data.doublevalue = VAN(igraph,"read",vid);

					if(data.doublevalue == data.doublevalue){


						Bitmapset* bmsN = bms_copy(data.pointer);
						int dno;
						while (!bms_is_empty(bmsN) && (dno = bms_first_member(bmsN)) >= 0){
							PLpgSQL_datum *datum = function->datums[dno];

							if (datum->dtype == PLPGSQL_DTYPE_VAR){



								for(int j = 0; j < nwriteDnos; j++){

									if(dno == writeDnos[j]){

										/* read -> write dependency, add edge */

										/* Add edges to the graph */
										igraph_add_edge(igraph,nodeid,vid);

										igraph_integer_t eid;
										igraph_get_eid(igraph, &eid,nodeid,vid,1,0);

										SETEAS(igraph,"type",eid,"WR-DEPENDENCE");
									}
								}


							}
						}
					}



					data.doublevalue = VAN(igraph,"write",vid);
					int* writeDnos2 = data.pointer;

					data.doublevalue  = VAN(igraph,"nwrite",vid);
					long nwriteDnos2 = data.longvalue;

					if(data.doublevalue != data.doublevalue)
						writeDnos2 = NULL;


					if(writeDnos2 != NULL){
						for(int j = 0; j < nwriteDnos; j++){


							for(int k = 0; k < nwriteDnos2; k++){
								/* write -> write dependency, add edge */
								/* Add edges to the graph */

								if(writeDnos[j] == writeDnos2[k]){
									igraph_add_edge(igraph,nodeid,vid);

									igraph_integer_t eid;
									igraph_get_eid(igraph, &eid,nodeid,vid,1,0);

									SETEAS(igraph,"type",eid,"WW-DEPENDENCE");
								}

							}
						}



						data.doublevalue = VAN(igraph,"read",nodeid);
						Bitmapset* bms = bms_copy(data.pointer);
						int dno;
						while (!bms_is_empty(bms) && (dno = bms_first_member(bms)) >= 0){
							PLpgSQL_datum *datum = function->datums[dno];

							if (datum->dtype == PLPGSQL_DTYPE_VAR){


								for(int j = 0; j < nwriteDnos2; j++){


									if(writeDnos2[j] == dno){
										/* write -> read dependency, add edge */

										/* Add edges to the graph */
										igraph_add_edge(igraph,nodeid,vid);

										igraph_integer_t eid;
										igraph_get_eid(igraph, &eid,nodeid,vid,1,0);

										SETEAS(igraph,"type",eid,"RW-DEPENDENCE");
									}
								}

							}
						}
					}


				}


			}

			igraph_vector_destroy(&order);

		}


		/* next iteration over graph nodes */
		IGRAPH_VIT_NEXT(nodeit);
	}
	/* destroy the graph iterator */
	igraph_vit_destroy(&nodeit);
	igraph_vs_destroy(&allNodes);
}

int getNodeNumberToStmt(PLpgSQL_stmt* stmt1, igraph_t* graph){
	/* init and create a node iterator over all nodes */
	igraph_vit_t nodeit;
	igraph_vs_t allNodes;
	igraph_vs_all(&allNodes);
	igraph_vit_create(graph,allNodes, &nodeit);

	int id = -1;

	/* iterate over the nodes */
	while (!IGRAPH_VIT_END(nodeit)) {
		/* current node id */
		long int nodeid = (long int) IGRAPH_VIT_GET(nodeit);

		union dblPointer data;
		data.doublevalue = VAN(graph,"stmt",nodeid);
		PLpgSQL_stmt* stmt = data.pointer;

		if(stmt1 == stmt){
			id = nodeid;
			break;
		}

		IGRAPH_VIT_NEXT(nodeit);
	}



	/* destroy the graph iterator */
	igraph_vit_destroy(&nodeit);
	igraph_vit_create(graph,allNodes, &nodeit);
	return id;
}



int dependenceConflict(int node1, int node2, igraph_t* igraph){

	/* if there is no node for the statement we must assume a conflict */
	if(node1 == -1 || node2 == -1){
		return 1;
	}



	igraph_vector_t eids;
	igraph_vector_init(&eids,0);
	igraph_incident(igraph, &eids,node1, IGRAPH_OUT);

	int retval = 0;

	for(int i=0;i<igraph_vector_size(&eids);i++){
		int eid = VECTOR(eids)[i];
		igraph_integer_t from; igraph_integer_t to;
		igraph_edge(igraph, eid,&from,&to);

		/* found a edge from the first to the second node */
		if(to == node2){

			const char* type = EAS(igraph,"type",eid);
			/* check if type is set */
			if(type){
				/* RW-DEPENDENCY */
				if(strcmp(type,"RW-DEPENDENCE") == 0){
					printf("RW-DEPENDENCE found from %s to %s!\n",
								VAS(igraph,"label",from),
								VAS(igraph,"label",to));
					retval = 1;
					break;
				}
				/* WR-DEPENDENCY */
				if(strcmp(type,"WR-DEPENDENCE") == 0){
					printf("WR-DEPENDENCE found from %s to %s!\n",
								VAS(igraph,"label",from),
								VAS(igraph,"label",to));
					retval = 1;
					break;
				}
				/* WW-DEPENDENCY */
				if(strcmp(type,"WW-DEPENDENCE") == 0){
					printf("WW-DEPENDENCE found from %s to %s!\n",
								VAS(igraph,"label",from),
								VAS(igraph,"label",to));
					retval = 1;
					break;
				}

			}


		}

	}

	if(retval == 0){
		printf("No confict found from %s to %s!\n",
					VAS(igraph,"label",node1),
					VAS(igraph,"label",node2));
	}

	igraph_vector_destroy(&eids);
	return retval;
}


int conflict(PLpgSQL_stmt* stmt1, PLpgSQL_stmt* stmt2, igraph_t* igraph){
	int node1 = getNodeNumberToStmt(stmt1,igraph);
	int node2 = getNodeNumberToStmt(stmt2,igraph);
	return dependenceConflict(node1,node2,igraph);

}

