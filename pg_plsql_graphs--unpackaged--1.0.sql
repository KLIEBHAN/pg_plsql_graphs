
-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_plsql_graphs" to load this file. \quit


ALTER EXTENSION pg_plsql_graphs ADD function pg_plsql_graphs(text);
ALTER EXTENSION pg_plsql_graphs ADD view pg_plsql_graphs;
ALTER EXTENSION pg_plsql_graphs ADD view pg_plsql_graphs_trimmed;
ALTER EXTENSION pg_plsql_graphs ADD view pg_plsql_last_flowgraph_dot;
ALTER EXTENSION pg_plsql_graphs ADD view pg_plsql_last_pdgs_dot;