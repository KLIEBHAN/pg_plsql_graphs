/* contrib/postgres_fdw/postgres_plsql_graphs--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_plsql_graphs" to load this file. \quit



-- Register the C function.
CREATE FUNCTION pg_plsql_graphs(OUT text, OUT text, OUT text)
RETURNS SETOF record
AS 'MODULE_PATHNAME', 'pg_plsql_graphs'
LANGUAGE C STRICT;



-- Register a view on the function for ease of use.
CREATE VIEW pg_plsql_graphs(function_name, flow_graph_dot, program_dependence_graph_dot) AS
  SELECT * FROM pg_plsql_graphs();

  
-- Register a view on the function for ease of use.
CREATE VIEW pg_plsql_graphs_trimmed(function_name, flow_graph_dot, program_dependence_graph_dot) AS
  SELECT 	function_name, 
  flow_graph_dot,
  program_dependence_graph_dot
  FROM pg_plsql_graphs;
  
-- Register a view on the function for ease of use.
CREATE VIEW pg_plsql_last_flowgraph_dot(flow_graph_dot) AS
  SELECT flow_graph_dot FROM pg_plsql_graphs_trimmed FETCH FIRST ROW ONLY;
  
  
-- Register a view on the function for ease of use.
CREATE VIEW pg_plsql_last_pdgs_dot(program_dependence_graph_dot) AS
  SELECT program_dependence_graph_dot FROM pg_plsql_graphs_trimmed FETCH FIRST ROW ONLY;
  