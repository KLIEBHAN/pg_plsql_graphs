
#include "plpgsql.h"
#include "nodes/pg_list.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <igraph/igraph.h>
#include "pg_plsql_graphs_lib.h"

#include "access/htup_details.h"
#include "catalog/namespace.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_proc_fn.h"
#include "catalog/pg_type.h"
#include "funcapi.h"
#include "nodes/makefuncs.h"
#include "parser/parse_type.h"
#include "utils/builtins.h"
#include "utils/guc.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/syscache.h"



/**
 * Workaround to get the parameters of a query as Bitmapset
 */
Bitmapset* getParametersOfQueryExpr(PLpgSQL_expr*         expr,
                                    PLpgSQL_function*     surroundingFunction,
                                    PLpgSQL_execstate*     estate){

    expr->func = surroundingFunction;
    SPI_prepare_params(expr->query,
                              (ParserSetupHook) pg_plsql_parser_setup,
                              (void *) expr,
                              0);


    return expr->paramnos;
}


/**
 * Woraround for resolving parameters
 *
 * Copys of plpgsql compiler functions!!!
 */
Node *pg_plsql_pre_column_ref(ParseState *pstate, ColumnRef *cref);
Node *pg_plsql_post_column_ref(ParseState *pstate, ColumnRef *cref, Node *var);
Node *pg_plsql_param_ref(ParseState *pstate, ParamRef *pref);

Node *
resolve_column_ref(ParseState *pstate, PLpgSQL_expr *expr,
                   ColumnRef *cref, bool error_if_no_field);

Node *
make_datum_param(PLpgSQL_expr *expr, int dno, int location);
PLpgSQL_nsitem *
plpgsql_ns_lookup(PLpgSQL_nsitem *ns_cur, bool localmode,
                  const char *name1, const char *name2, const char *name3,
                  int *names_used);
void
exec_get_datum_type_info(PLpgSQL_execstate *estate,
                         PLpgSQL_datum *datum,
                         Oid *typeid, int32 *typmod, Oid *collation);
/*
 * pg_plsql_parser_setup        set up parser hooks for dynamic parameters
 *
 * Note: this routine, and the hook functions it prepares for, are logically
 * part of plpgsql parsing.  But they actually run during function execution,
 * when we are ready to evaluate a SQL query or expression that has not
 * previously been parsed and planned.
 */
void
pg_plsql_parser_setup(struct ParseState *pstate, PLpgSQL_expr *expr)
{
    pstate->p_pre_columnref_hook = pg_plsql_pre_column_ref;
    pstate->p_post_columnref_hook = pg_plsql_post_column_ref;
    pstate->p_paramref_hook = pg_plsql_param_ref;
    /* no need to use p_coerce_param_hook */
    pstate->p_ref_hook_state = (void *) expr;
}

/*
 * pg_plsql_pre_column_ref        parser callback before parsing a ColumnRef
 */
Node *
pg_plsql_pre_column_ref(ParseState *pstate, ColumnRef *cref)
{
    PLpgSQL_expr *expr = (PLpgSQL_expr *) pstate->p_ref_hook_state;

    if (expr->func->resolve_option == PLPGSQL_RESOLVE_VARIABLE)
        return resolve_column_ref(pstate, expr, cref, false);
    else
        return NULL;
}

/*
 * pg_plsql_post_column_ref        parser callback after parsing a ColumnRef
 */
Node *
pg_plsql_post_column_ref(ParseState *pstate, ColumnRef *cref, Node *var)
{
    PLpgSQL_expr *expr = (PLpgSQL_expr *) pstate->p_ref_hook_state;
        Node       *myvar;

        if (expr->func->resolve_option == PLPGSQL_RESOLVE_VARIABLE)
            return NULL;            /* we already found there's no match */

        if (expr->func->resolve_option == PLPGSQL_RESOLVE_COLUMN && var != NULL)
            return NULL;            /* there's a table column, prefer that */

        /*
         * If we find a record/row variable but can't match a field name, throw
         * error if there was no core resolution for the ColumnRef either.    In
         * that situation, the reference is inevitably going to fail, and
         * complaining about the record/row variable is likely to be more on-point
         * than the core parser's error message.  (It's too bad we don't have
         * access to transformColumnRef's internal crerr state here, as in case of
         * a conflict with a table name this could still be less than the most
         * helpful error message possible.)
         */
        myvar = resolve_column_ref(pstate, expr, cref, (var == NULL));

        if (myvar != NULL && var != NULL)
        {
            /*
             * We could leave it to the core parser to throw this error, but we
             * can add a more useful detail message than the core could.
             */
            ereport(ERROR,
                    (errcode(ERRCODE_AMBIGUOUS_COLUMN),
                     errmsg("column reference \"%s\" is ambiguous",
                            NameListToString(cref->fields)),
                     errdetail("It could refer to either a PL/pgSQL variable or a table column."),
                     parser_errposition(pstate, cref->location)));
        }

        return myvar;
}

/*
 * pg_plsql_param_ref        parser callback for ParamRefs ($n symbols)
 */
Node *
pg_plsql_param_ref(ParseState *pstate, ParamRef *pref)
{
    PLpgSQL_expr *expr = (PLpgSQL_expr *) pstate->p_ref_hook_state;
    char        pname[32];
    PLpgSQL_nsitem *nse;

    snprintf(pname, sizeof(pname), "$%d", pref->number);

    nse = plpgsql_ns_lookup(expr->ns, false,
                            pname, NULL, NULL,
                            NULL);

    if (nse == NULL)
        return NULL;            /* name not known to plpgsql */

    return make_datum_param(expr, nse->itemno, pref->location);
}



/*
 * resolve_column_ref        attempt to resolve a ColumnRef as a plpgsql var
 *
 * Returns the translated node structure, or NULL if name not found
 *
 * error_if_no_field tells whether to throw error or quietly return NULL if
 * we are able to match a record/row name but don't find a field name match.
 */
Node *
resolve_column_ref(ParseState *pstate, PLpgSQL_expr *expr,
                   ColumnRef *cref, bool error_if_no_field)
{
    PLpgSQL_execstate *estate;
    PLpgSQL_nsitem *nse;
    const char *name1;
    const char *name2 = NULL;
    const char *name3 = NULL;
    const char *colname = NULL;
    int            nnames;
    int            nnames_scalar = 0;
    int            nnames_wholerow = 0;
    int            nnames_field = 0;

    /*
     * We use the function's current estate to resolve parameter data types.
     * This is really pretty bogus because there is no provision for updating
     * plans when those types change ...
     */
    estate = expr->func->cur_estate;

    /*----------
     * The allowed syntaxes are:
     *
     * A        Scalar variable reference, or whole-row record reference.
     * A.B        Qualified scalar or whole-row reference, or field reference.
     * A.B.C    Qualified record field reference.
     * A.*        Whole-row record reference.
     * A.B.*    Qualified whole-row record reference.
     *----------
     */
    switch (list_length(cref->fields))
    {
        case 1:
            {
                Node       *field1 = (Node *) linitial(cref->fields);

                Assert(IsA(field1, String));
                name1 = strVal(field1);
                nnames_scalar = 1;
                nnames_wholerow = 1;
                break;
            }
        case 2:
            {
                Node       *field1 = (Node *) linitial(cref->fields);
                Node       *field2 = (Node *) lsecond(cref->fields);

                Assert(IsA(field1, String));
                name1 = strVal(field1);

                /* Whole-row reference? */
                if (IsA(field2, A_Star))
                {
                    /* Set name2 to prevent matches to scalar variables */
                    name2 = "*";
                    nnames_wholerow = 1;
                    break;
                }

                Assert(IsA(field2, String));
                name2 = strVal(field2);
                colname = name2;
                nnames_scalar = 2;
                nnames_wholerow = 2;
                nnames_field = 1;
                break;
            }
        case 3:
            {
                Node       *field1 = (Node *) linitial(cref->fields);
                Node       *field2 = (Node *) lsecond(cref->fields);
                Node       *field3 = (Node *) lthird(cref->fields);

                Assert(IsA(field1, String));
                name1 = strVal(field1);
                Assert(IsA(field2, String));
                name2 = strVal(field2);

                /* Whole-row reference? */
                if (IsA(field3, A_Star))
                {
                    /* Set name3 to prevent matches to scalar variables */
                    name3 = "*";
                    nnames_wholerow = 2;
                    break;
                }

                Assert(IsA(field3, String));
                name3 = strVal(field3);
                colname = name3;
                nnames_field = 2;
                break;
            }
        default:
            /* too many names, ignore */
            return NULL;
    }

    nse = plpgsql_ns_lookup(expr->ns, false,
                            name1, name2, name3,
                            &nnames);

    if (nse == NULL)
        return NULL;            /* name not known to plpgsql */

    switch (nse->itemtype)
    {
        case PLPGSQL_NSTYPE_VAR:
            if (nnames == nnames_scalar)
                return make_datum_param(expr, nse->itemno, cref->location);
            break;
        case PLPGSQL_NSTYPE_REC:
            if (nnames == nnames_wholerow)
                return make_datum_param(expr, nse->itemno, cref->location);
            if (nnames == nnames_field)
            {
                /* colname could be a field in this record */
                int            i;

                /* search for a datum referencing this field */
                for (i = 0; i < estate->ndatums; i++)
                {
                    PLpgSQL_recfield *fld = (PLpgSQL_recfield *) estate->datums[i];

                    if (fld->dtype == PLPGSQL_DTYPE_RECFIELD &&
                        fld->recparentno == nse->itemno &&
                        strcmp(fld->fieldname, colname) == 0)
                    {
                        return make_datum_param(expr, i, cref->location);
                    }
                }

                /*
                 * We should not get here, because a RECFIELD datum should
                 * have been built at parse time for every possible qualified
                 * reference to fields of this record.    But if we do, handle
                 * it like field-not-found: throw error or return NULL.
                 */
                if (error_if_no_field)
                    ereport(ERROR,
                            (errcode(ERRCODE_UNDEFINED_COLUMN),
                             errmsg("record \"%s\" has no field \"%s\"",
                                    (nnames_field == 1) ? name1 : name2,
                                    colname),
                             parser_errposition(pstate, cref->location)));
            }
            break;
        case PLPGSQL_NSTYPE_ROW:
            if (nnames == nnames_wholerow)
                return make_datum_param(expr, nse->itemno, cref->location);
            if (nnames == nnames_field)
            {
                /* colname could be a field in this row */
                PLpgSQL_row *row = (PLpgSQL_row *) estate->datums[nse->itemno];
                int            i;

                for (i = 0; i < row->nfields; i++)
                {
                    if (row->fieldnames[i] &&
                        strcmp(row->fieldnames[i], colname) == 0)
                    {
                        return make_datum_param(expr, row->varnos[i],
                                                cref->location);
                    }
                }
                /* Not found, so throw error or return NULL */
                if (error_if_no_field)
                    ereport(ERROR,
                            (errcode(ERRCODE_UNDEFINED_COLUMN),
                             errmsg("record \"%s\" has no field \"%s\"",
                                    (nnames_field == 1) ? name1 : name2,
                                    colname),
                             parser_errposition(pstate, cref->location)));
            }
            break;
        default:
            elog(ERROR, "unrecognized plpgsql itemtype: %d", nse->itemtype);
    }

    /* Name format doesn't match the plpgsql variable type */
    return NULL;
}



/*
 * Helper for columnref parsing: build a Param referencing a plpgsql datum,
 * and make sure that that datum is listed in the expression's paramnos.
 */
Node *
make_datum_param(PLpgSQL_expr *expr, int dno, int location)
{
    PLpgSQL_execstate *estate;
    PLpgSQL_datum *datum;
    Param       *param;
    MemoryContext oldcontext;

    /* see comment in resolve_column_ref */
    estate = expr->func->cur_estate;
    Assert(dno >= 0 && dno < estate->ndatums);
    datum = estate->datums[dno];

    /*
     * Bitmapset must be allocated in function's permanent memory context
     */
    oldcontext = MemoryContextSwitchTo(expr->func->fn_cxt);
    expr->paramnos = bms_add_member(expr->paramnos, dno);
    MemoryContextSwitchTo(oldcontext);

    param = makeNode(Param);
    param->paramkind = PARAM_EXTERN;
    param->paramid = dno + 1;
    exec_get_datum_type_info(estate,
                             datum,
                             &param->paramtype,
                             &param->paramtypmod,
                             &param->paramcollid);
    param->location = location;

    return (Node *) param;
}

/* ----------
 * plpgsql_ns_lookup        Lookup an identifier in the given namespace chain
 *
 * Note that this only searches for variables, not labels.
 *
 * If localmode is TRUE, only the topmost block level is searched.
 *
 * name1 must be non-NULL.    Pass NULL for name2 and/or name3 if parsing a name
 * with fewer than three components.
 *
 * If names_used isn't NULL, *names_used receives the number of names
 * matched: 0 if no match, 1 if name1 matched an unqualified variable name,
 * 2 if name1 and name2 matched a block label + variable name.
 *
 * Note that name3 is never directly matched to anything.  However, if it
 * isn't NULL, we will disregard qualified matches to scalar variables.
 * Similarly, if name2 isn't NULL, we disregard unqualified matches to
 * scalar variables.
 * ----------
 */
PLpgSQL_nsitem *
plpgsql_ns_lookup(PLpgSQL_nsitem *ns_cur, bool localmode,
                  const char *name1, const char *name2, const char *name3,
                  int *names_used)
{
    /* Outer loop iterates once per block level in the namespace chain */
    while (ns_cur != NULL)
    {
        PLpgSQL_nsitem *nsitem;

        /* Check this level for unqualified match to variable name */
        for (nsitem = ns_cur;
             nsitem->itemtype != PLPGSQL_NSTYPE_LABEL;
             nsitem = nsitem->prev)
        {
            if (strcmp(nsitem->name, name1) == 0)
            {
                if (name2 == NULL ||
                    nsitem->itemtype != PLPGSQL_NSTYPE_VAR)
                {
                    if (names_used)
                        *names_used = 1;
                    return nsitem;
                }
            }
        }

        /* Check this level for qualified match to variable name */
        if (name2 != NULL &&
            strcmp(nsitem->name, name1) == 0)
        {
            for (nsitem = ns_cur;
                 nsitem->itemtype != PLPGSQL_NSTYPE_LABEL;
                 nsitem = nsitem->prev)
            {
                if (strcmp(nsitem->name, name2) == 0)
                {
                    if (name3 == NULL ||
                        nsitem->itemtype != PLPGSQL_NSTYPE_VAR)
                    {
                        if (names_used)
                            *names_used = 2;
                        return nsitem;
                    }
                }
            }
        }

        if (localmode)
            break;                /* do not look into upper levels */

        ns_cur = nsitem->prev;
    }

    /* This is just to suppress possibly-uninitialized-variable warnings */
    if (names_used)
        *names_used = 0;
    return NULL;                /* No match found */
}


/*
 * exec_get_datum_type_info            Get datatype etc of a PLpgSQL_datum
 *
 * An extended version of exec_get_datum_type, which also retrieves the
 * typmod and collation of the datum.
 */
void
exec_get_datum_type_info(PLpgSQL_execstate *estate,
                         PLpgSQL_datum *datum,
                         Oid *typeid, int32 *typmod, Oid *collation)
{
    switch (datum->dtype)
    {
        case PLPGSQL_DTYPE_VAR:
            {
                PLpgSQL_var *var = (PLpgSQL_var *) datum;

                *typeid = var->datatype->typoid;
                *typmod = var->datatype->atttypmod;
                *collation = var->datatype->collation;
                break;
            }

        case PLPGSQL_DTYPE_ROW:
            {
                PLpgSQL_row *row = (PLpgSQL_row *) datum;

                if (!row->rowtupdesc)    /* should not happen */
                    elog(ERROR, "row variable has no tupdesc");
                /* Make sure we have a valid type/typmod setting */
                BlessTupleDesc(row->rowtupdesc);
                *typeid = row->rowtupdesc->tdtypeid;
                /* do NOT return the mutable typmod of a RECORD variable */
                *typmod = -1;
                /* composite types are never collatable */
                *collation = InvalidOid;
                break;
            }

        case PLPGSQL_DTYPE_REC:
            {
                PLpgSQL_rec *rec = (PLpgSQL_rec *) datum;

                if (rec->tupdesc == NULL)
                    ereport(ERROR,
                          (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
                           errmsg("record \"%s\" is not assigned yet",
                                  rec->refname),
                           errdetail("The tuple structure of a not-yet-assigned record is indeterminate.")));
                /* Make sure we have a valid type/typmod setting */
                BlessTupleDesc(rec->tupdesc);
                *typeid = rec->tupdesc->tdtypeid;
                /* do NOT return the mutable typmod of a RECORD variable */
                *typmod = -1;
                /* composite types are never collatable */
                *collation = InvalidOid;
                break;
            }

        case PLPGSQL_DTYPE_RECFIELD:
            {
                PLpgSQL_recfield *recfield = (PLpgSQL_recfield *) datum;
                PLpgSQL_rec *rec;
                int            fno;

                rec = (PLpgSQL_rec *) (estate->datums[recfield->recparentno]);
                if (rec->tupdesc == NULL)
                    ereport(ERROR,
                          (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
                           errmsg("record \"%s\" is not assigned yet",
                                  rec->refname),
                           errdetail("The tuple structure of a not-yet-assigned record is indeterminate.")));
                fno = SPI_fnumber(rec->tupdesc, recfield->fieldname);
                if (fno == SPI_ERROR_NOATTRIBUTE)
                    ereport(ERROR,
                            (errcode(ERRCODE_UNDEFINED_COLUMN),
                             errmsg("record \"%s\" has no field \"%s\"",
                                    rec->refname, recfield->fieldname)));
                *typeid = SPI_gettypeid(rec->tupdesc, fno);
                if (fno > 0)
                    *typmod = rec->tupdesc->attrs[fno - 1]->atttypmod;
                else
                    *typmod = -1;
                if (fno > 0)
                    *collation = rec->tupdesc->attrs[fno - 1]->attcollation;
                else    /* no system column types have collation */
                    *collation = InvalidOid;
                break;
            }

        default:
            elog(ERROR, "unrecognized dtype: %d", datum->dtype);
            *typeid = InvalidOid;        /* keep compiler quiet */
            *typmod = -1;
            *collation = InvalidOid;
            break;
    }
}

