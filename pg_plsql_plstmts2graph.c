#include "plpgsql.h"
#include "nodes/pg_list.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <igraph/igraph.h>
#include "pg_plsql_graphs.h"

/**
 * Converts the variable with the given id to its refname
 */
char* varnumberToVarname(int varno,PLpgSQL_datum** datums, int ndatums){

	/* iterate over the datums */
	for(int i=0; i<ndatums;i++){
		PLpgSQL_datum* datum = datums[i];
		/* check if datum type is a variable */
		if (datum->dtype == PLPGSQL_DTYPE_VAR) {
			PLpgSQL_variable* myvar = ((PLpgSQL_variable*)datum);
			/* check if it has the given variable number */
			if(myvar->dno == varno){
				/* return the refname of the variable */
				return myvar->refname;
			}
		}

	}
	return (char*)NULL;
}




struct graph_status* initStatus(int initnodeid){
	/* create entry node for the flow graph */
	struct node* root = palloc(sizeof(struct node));
	/* Fist nodeid is 0 */
	root->key = initnodeid;
	/* set edges of root to empty list */
	root->edges = NIL;

	/* init nodes with the entry node */
	List* nodes = list_make1(root);
	/* init parents with the entry node */
	List* parents = list_make1(root);
	/* status structure will carry the nodes and current parents */
	struct graph_status* status = palloc(sizeof(struct graph_status));
	status->nodes = nodes;
	status->parents = parents;

	return status;

}


void appendNewNodeAndConnectParents(int* newnodeid,
									struct graph_status* status,
									PLpgSQL_stmt* stmt){

	/* create new node id */
	(*newnodeid)++;


	/* create new node */
	struct node* newnode = palloc(sizeof(struct node));
	/* add the new id */
	newnode->key = *newnodeid;
	/* add the statement */
	newnode->stmt = stmt;

	newnode->edges = NIL;

	/* append the new node to nodes */
	lappend(status->nodes,newnode);

	/* connect parents with the new node */
	connectNodeToParents(newnode->key,status->parents);
	/* set current node as new parent */
	status->parents = list_make1(newnode);
}

/**
 * creates a program graph to a PLpgSQL_function
 */
void createProgramGraph(int* newnodeid,
					struct graph_status* status,
					List* statements,
					PLpgSQL_function* function){



	ListCell* l;
	/* iterate over the statements */
	foreach(l, statements){

		/* Get the statement */
		PLpgSQL_stmt* stmt = lfirst(l);

		switch (stmt->cmd_type) {
			case PLPGSQL_STMT_ASSIGN:
				/* Append the assign statement an connect it to the parents */
				appendNewNodeAndConnectParents(newnodeid,status,stmt);
				break;
			case PLPGSQL_STMT_RAISE:
				/* Append the raise statement an connect it to the parents */
				appendNewNodeAndConnectParents(newnodeid,status,stmt);
				break;
			case PLPGSQL_STMT_WHILE:

				/* Append the while statement an connect it to the parents */
				appendNewNodeAndConnectParents(newnodeid,status,stmt);

				/* Parents after the while node */
				List* parentsAfterWhile = copy_list(status->parents);

				/* remember the while node id */
				int whileNodeId = *newnodeid;

				/* Progress the statements in the loop */
				createProgramGraph(	newnodeid,
									status,
									((PLpgSQL_stmt_while*)stmt)->body,
									function);

				/* connect the while node id to the parents after the last while node */
				connectNodeToParents(whileNodeId,status->parents);

				/* parents for upcoming statements are the parents after the while node */
				status->parents = parentsAfterWhile;
				break;
			case PLPGSQL_STMT_IF:

				/* Append the if node and connect it to the parents */
				appendNewNodeAndConnectParents(newnodeid,status,stmt);
				/* Parents after the if node */
				List* parentsAfterIf = copy_list(status->parents);

				/* Progress the then statements */
				createProgramGraph(	newnodeid,
									status,
									((PLpgSQL_stmt_if*)stmt)->then_body,
									function);

				/* Parents after the last then statement */
				List* parentsAfterThen = copy_list(status->parents);

				/* Set parents to those after the if node for progressing the else body */
				status->parents = copy_list(parentsAfterIf);

				/* Progress the else statements */
				createProgramGraph(	newnodeid,
									status,
									((PLpgSQL_stmt_if*)stmt)->else_body,
									function);

				/* Parents after the last else statement */
				List* parentsAfterElse = copy_list(status->parents);

				/*  set the parents for the upcoming statements to those */
				/*  after the last then statement concatinated */
				/*  with those after the the last else statement */
				/*  or if not present to those after the if statement. */
				if(((PLpgSQL_stmt_if*)stmt)->else_body != NULL){
					status->parents = list_concat(parentsAfterThen,parentsAfterElse);
				}
				else{
					status->parents = list_concat(parentsAfterThen,parentsAfterIf);
				}
				break;
			case PLPGSQL_STMT_RETURN:
				/* Append the return statement an connect it to the parents */
				appendNewNodeAndConnectParents(newnodeid,status,stmt);
				/* A return statement is never a parent */
				status->parents = NIL;
				break;
			case PLPGSQL_STMT_EXECSQL:
				/* Append the exec sql statement an connect it to the parents */
				appendNewNodeAndConnectParents(newnodeid,status,stmt);
				break;
			default:
				printf("Unsupported Statement %i\n",stmt->cmd_type);
				break;
		}

	}
}
