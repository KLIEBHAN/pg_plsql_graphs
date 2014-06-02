#include "postgres.h"
#include "plpgsql.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include "access/tuptoaster.h"
#include "access/htup_details.h"
#include "access/hash.h"
#include "catalog/pg_language.h"
#include "catalog/pg_proc.h"
#include "executor/functions.h"
#include "executor/instrument.h"
#include "executor/spi.h"
#include "lib/stringinfo.h"
#include "fmgr.h"
#include "funcapi.h"
#include "lib/stringinfo.h"
#include "miscadmin.h"
#include "nodes/nodeFuncs.h"
#include "utils/builtins.h"
#include "utils/xml.h"
#include "fmgr.h"
#include "parser/analyze.h"
#include "parser/gramparse.h"
#include "parser/parser.h"
#include "parser/parsetree.h"
#include "parser/scanner.h"
#include "parser/parse_node.h"
#include "storage/fd.h"
#include "storage/ipc.h"
#include "storage/spin.h"
#include "pgstat.h"
#include "tcop/utility.h"
#include "utils/builtins.h"
#include "utils/fmgrtab.h"
#include "utils/guc.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "pg_plsql_graphs.h"
#define MAXDOTFILESIZE 4096
PG_MODULE_MAGIC;



/*
 * Shared state
 */
typedef struct pgpgSharedState
{
    LWLock*            lock;            /* protects hashtable search/modification */
    PLpgSQL_plugin* plugin;            /* PlpgSQL_pluging struct */
    int         counter;            /* counter for ids */

} pgpgSharedState;


/*
 * Hashtable key that defines the identity of a hashtable entry.  We separate
 * entries by user, database and a function id. Also every entey has a unique id.
 */
typedef struct pgpgHashKey
{
    Oid            userid;            /* user OID */
    Oid            dbid;            /* database OID */
    Oid            functionid;        /* function OID */
    long        uniqueid;        /* unique ID */

} pgpgHashKey;

/*
 * Dot dependency graphs to plpgsql functions
 */
typedef struct DotStruct
{
    int64        id;
    char        functionName[255];
    char        flowGraphDot[MAXDOTFILESIZE];
    char        programDepencenceGraphDot[MAXDOTFILESIZE];
} DotStruct;



/*
 * Statistics per statement
 */
typedef struct pgpgEntry
{
    pgpgHashKey key;            /* hash key of entry - MUST BE FIRST */
    DotStruct    dotStruct;        /* the dotfiles for this function */
    slock_t        mutex;            /* protects the dotStruct only */
} pgpgEntry;

Datum        pg_plsql_graphs(PG_FUNCTION_ARGS);
void        _PG_init(void);

static void pgpg_shmem_startup(void);
static void pgpg_func_beg(PLpgSQL_execstate*    estate,
                          PLpgSQL_function*     func);
static void pgpg_func_end(PLpgSQL_execstate*    estate,
                          PLpgSQL_function*     func);
static uint32 pgpg_hash_fn(const void*     key,
                           Size            keysize);
static int    pgpg_match_fn(const void*    key1,
                            const void*    key2,
                            Size keysize);

static pgpgEntry * entry_alloc( pgpgHashKey*    key,
                                bool            sticky,
                                int             id,
                                char*           functionName,
                                char*           flowGraphDot,
                                char*           programDepencenceGraphDot);
static void entry_dealloc(void);

/* Saved hook values in case of unload */
static shmem_startup_hook_type prev_shmem_startup_hook = NULL;

static int    pgpg_max = 5000;            /* max # statements to track */


/* Links to shared memory state */
static pgpgSharedState* pgpg = NULL;
static HTAB* pgpg_hash = NULL;

/*
 * Module load callback
 */
void
_PG_init(void)
{

    if (!process_shared_preload_libraries_in_progress)
        return;

    /*
     * Define (or redefine) custom GUC variable(s).
     */
    DefineCustomIntVariable("pg_plsql_graphs.max",
      "Sets the maximum number of functions tracked by pg_plsql_graphs.",
                            NULL,
                            &pgpg_max,
                            5000,
                            100,
                            INT_MAX,
                            PGC_POSTMASTER,
                            0,
                            NULL,
                            NULL,
                            NULL);



    RequestAddinShmemSpace( sizeof(PLpgSQL_plugin)+
                            sizeof(pgpgSharedState)+
                            hash_estimate_size(pgpg_max,
                                               sizeof(pgpgEntry)));
    RequestAddinLWLocks(1);



    /*
     * Install hooks.
     */
    prev_shmem_startup_hook = shmem_startup_hook;
    shmem_startup_hook = pgpg_shmem_startup;

}

/*
 * Module unload callback
 */
void
_PG_fini(void)
{
    shmem_startup_hook = prev_shmem_startup_hook;
}

/**
 * On startup of shared memory
 */
static void pgpg_shmem_startup(){
    bool        found;
    HASHCTL        info;



    if (prev_shmem_startup_hook)
        prev_shmem_startup_hook();

    /*
     * Acquire lock because we are accessing shared memory
     */
    LWLockAcquire(AddinShmemInitLock, LW_EXCLUSIVE);

    /**
     * Allocate memory for pgpgSharedState structure
     */
    pgpg = ShmemInitStruct("pgpgSharedState",
               sizeof(pgpgSharedState),
               &found);

    if(!found){

        /**
         * Allocate memory for PLpgSQL_plugin
         */
        pgpg->plugin = ShmemInitStruct("PLpgSQL_plugin",
                   sizeof(PLpgSQL_plugin),
                   &found);

        pgpg->lock = LWLockAssign();

        /**
         * Set a function hook before the execution of PL/SQL function
         */
        pgpg->plugin->func_beg = pgpg_func_beg;

        /**
         * Set a function hook after the execution of PL/SQL function
         */
        pgpg->plugin->func_end = pgpg_func_end;



        /* Set up a rendezvous point */
        PLpgSQL_plugin ** plugin_ptr = (PLpgSQL_plugin **)
                                        find_rendezvous_variable("PLpgSQL_plugin");
        *plugin_ptr = pgpg->plugin;

    }



    /* add Hash table */
    memset(&info, 0, sizeof(info));
    info.keysize = sizeof(pgpgHashKey);
    info.entrysize = sizeof(pgpgEntry);
    info.hash = pgpg_hash_fn;
    info.match = pgpg_match_fn;
    pgpg_hash = ShmemInitHash("pg_plsql_graph hash",
                              pgpg_max,
                              pgpg_max,
                              &info,
                              HASH_ELEM | HASH_FUNCTION | HASH_COMPARE);
    /* Release the lock */
    LWLockRelease(AddinShmemInitLock);


}

/**
 * Function hook before the execution of function
 */
static void pgpg_func_beg(PLpgSQL_execstate *estate, PLpgSQL_function *func){
    /* creates a graph for the current function */
    createGraph(func,estate);
}


/**
 * Function hook after the execution of function
 */
static void pgpg_func_end(PLpgSQL_execstate *estate, PLpgSQL_function *func){
}

/**
 * Creates the graph and stores it in the HashTable
 */
void createGraph(PLpgSQL_function* function,PLpgSQL_execstate *estate){

    int newnodeid = 0;
    struct graph_status* status  = initStatus(newnodeid);
    /* create the flow graph to the given PL/SQL function */
    createProgramGraph(&newnodeid,status,function->action->body,function);


    /* convert the graph to an igraph */
    igraph_t* igraph = buildIGraph(status->nodes,function,estate);

    /* perform operations depenence analysis on igraph */
    iterateIGraphNodes(igraph,&createProgramDependenceGraph,NULL,NULL,0);
    /* convert the igraph to a dot format */
    createDotGraphs();



    /* Create the flow graph */
    char* flowGraphDot = convertGraphToDotFormat(
                                igraph,
                                /* Flow graph has only the FLOW edges with the color black */
                                list_make1(
                                list_make2("FLOW", "black")),
                                1,/* edge labels */
                                0,/* not on same level */
                                NULL,/* no additional general atrribs */
                                "[shape=box]",/* box shape */
                                MAXDOTFILESIZE);


    char* programDepenceGraphDot = convertGraphToDotFormat(
                                igraph,
                                /* Dependence Graph edges with colors, Flow edges are dashed */
                                list_make4(
                                        list_make3("FLOW",         "black", "[style=dashed]"),
                                        list_make2("RW-DEPENDENCE","blue"),
                                        list_make2("WR-DEPENDENCE","green"),
                                        list_make2("WW-DEPENDENCE","red")),
                                0,/* no edge labels */
                                1,/* on same level */
                                "splines=ortho;\n",/* ortho */
                                NULL,/* no additional node attribs */
                                MAXDOTFILESIZE);




    /* destroy the igraph */
    igraph_destroy(igraph);
    pfree(igraph);

    pgpgHashKey key;

    /* Set lock for accessing pgpg variables and the hash table */
    LWLockAcquire(pgpg->lock, LW_EXCLUSIVE);

    /* Set up key for hashtable search */
    key.userid = GetUserId();
    key.dbid = MyDatabaseId;
    key.functionid = function->fn_oid;
    key.uniqueid = pgpg->counter++;


    /* Allocates an entry in the hash table */
    entry_alloc(    &key,
                    true,
                    pgpg->counter,
                    function->fn_signature,
                    flowGraphDot,
                    programDepenceGraphDot);

    /* Release the lock */
    LWLockRelease(pgpg->lock);

    /* Dot sources were copied to shared memory so we can free them */
    pfree(programDepenceGraphDot);
    pfree(flowGraphDot);

}






PG_FUNCTION_INFO_V1(pg_plsql_graphs);

/**
 * Custom pg_plsql_graphs function that can be called by the user
 * and will return a table containing graphs of preivous called
 * plpgsql functions.
 */
Datum pg_plsql_graphs(PG_FUNCTION_ARGS){

    pgpgHashKey key;
    pgpgEntry * entry;

    /* Info about the return set */
    ReturnSetInfo*   rsinfo = (ReturnSetInfo *) fcinfo->resultinfo;
    TupleDesc        tupdesc;
    Tuplestorestate* tupstore;
    MemoryContext    per_query_ctx;
    MemoryContext    oldcontext;
    Oid              userid = GetUserId();
    HASH_SEQ_STATUS  hash_seq;


    /* hash table must exist already */
    if (!pgpg || !pgpg_hash)
        ereport(ERROR,
                (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
                 errmsg("pg_stat_statements must be loaded via shared_preload_libraries")));

    /* check to see if caller supports us returning a tuplestore */
    if (rsinfo == NULL || !IsA(rsinfo, ReturnSetInfo))
        ereport(ERROR,
                (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                 errmsg("set-valued function called in context that cannot accept a set")));
    if (!(rsinfo->allowedModes & SFRM_Materialize))
        ereport(ERROR,
                (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                 errmsg("materialize mode required, but it is not " \
                        "allowed in this context")));

    /* Build a tuple descriptor for our result type */
    if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
        elog(ERROR, "return type must be a row type");


    /* Switch into long-lived context to construct returned data structures */
    per_query_ctx = rsinfo->econtext->ecxt_per_query_memory;
    oldcontext = MemoryContextSwitchTo(per_query_ctx);


    /* Init tuplestore to store the return table */
    tupstore = tuplestore_begin_heap(true, false, work_mem);
    rsinfo->returnMode = SFRM_Materialize;
    rsinfo->setResult = tupstore;
    rsinfo->setDesc = tupdesc;


    /*
     * Get shared lock, load or reload the query text file if we must, and
     * iterate over the hashtable entries.
     */
    LWLockAcquire(pgpg->lock, LW_SHARED);

    /* interate the pgpg_hash */
    hash_seq_init(&hash_seq, pgpg_hash);
    while ((entry = hash_seq_search(&hash_seq)) != NULL)
    {
        /* values will store the current row  */
        Datum        values[tupdesc->natts];
        bool        nulls[tupdesc->natts];
        int            i = 0;
        int            j = 0;
        DotStruct    tmp;

        memset(values, 0, sizeof(values));
        memset(nulls, 0, sizeof(nulls));

        /* Set columns of the current row */
        values[i++] = CStringGetTextDatum(entry->dotStruct.functionName);
        values[i++] = CStringGetTextDatum(entry->dotStruct.flowGraphDot);
        values[i++] = CStringGetTextDatum(entry->dotStruct.programDepencenceGraphDot);

        /* No null values */
        nulls[j++] = false;
        nulls[j++] = false;
        nulls[j++] = false;

        /* put current row in the tuplestore */
        tuplestore_putvalues(tupstore, tupdesc, values, nulls);
    }

    /* Release the lock */
    LWLockRelease(pgpg->lock);

    /* clean up and return the tuplestore */
    tuplestore_donestoring(tupstore);


    return (Datum) 0;;
}






/*
 * Calculate hash value for a key
 */
static uint32
pgpg_hash_fn(const void *key, Size keysize)
{
    const pgpgHashKey *k = (const pgpgHashKey *) key;

    return hash_uint32((uint32) k->userid) ^
        hash_uint32((uint32) k->dbid) ^
        hash_uint32((uint32) k->functionid) ^
        hash_uint32((uint32) k->uniqueid) ;
}

/*
 * Compare two keys - zero means match
 */
static int
pgpg_match_fn(const void *key1, const void *key2, Size keysize)
{
    const pgpgHashKey *k1 = (const pgpgHashKey *) key1;
    const pgpgHashKey *k2 = (const pgpgHashKey *) key2;

    if (k1->userid == k2->userid &&
        k1->dbid == k2->dbid &&
        k1->functionid == k2->functionid &&
        k1->uniqueid == k2->uniqueid)
        return 0;
    else
        return 1;
}





/*
 * Allocate a new hashtable entry.
 * caller must hold an exclusive lock on pgss->lock
 */
static pgpgEntry *
entry_alloc(pgpgHashKey*    key,
            bool            sticky,
            int             id,
            char*           functionName,
            char*           flowGraphDot,
            char*           programDepencenceGraphDot)
{
    pgpgEntry  *entry;
    bool        found;

    /* Make space if needed */
    while (hash_get_num_entries(pgpg_hash) >= pgpg_max){
        entry_dealloc();
    }

    /* Find or create an entry with desired hash code */
    entry = (pgpgEntry *) hash_search(pgpg_hash, key, HASH_ENTER, &found);

    if (!found)
    {
        /* New entry, initialize it */

        /* reset the statistics */
        memset(&entry->dotStruct, 0, sizeof(DotStruct));

        entry->dotStruct.id = id;
        strcpy(entry->dotStruct.functionName,functionName);
        strcpy(entry->dotStruct.flowGraphDot,flowGraphDot);
        strcpy(entry->dotStruct.programDepencenceGraphDot,programDepencenceGraphDot);
    }

    return entry;
}


/*
 * Deallocate least used entries.
 * Caller must hold an exclusive lock on pgss->lock.
 */
static void
entry_dealloc(void)
{
    HASH_SEQ_STATUS hash_seq;
    pgpgEntry*      entry;
    int             nvictims = 5;

    int i = 0;
    /* iterate hash and free first nvictims items */
    hash_seq_init(&hash_seq, pgpg_hash);
    while ((entry = hash_seq_search(&hash_seq)) != NULL && i < nvictims)
    {
        hash_search(pgpg_hash, &entry->key, HASH_REMOVE, NULL);
    }
}

