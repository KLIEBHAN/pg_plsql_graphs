# contrib/pg_plsql_graphs/Makefile


top_builddir = ../..

PGFILEDESC = "pg_plsql_graphs"
PGAPPICON = win32


MODULE_big = pg_plsql_graphs
OBJS	= pg_plsql_graphs.o pg_plsql_graph_ops.o pg_plsql_plstmts2graph.o pg_plsql_graph2igraph.o pg_plsql_igraph_export.o pg_plsql_igraphanalysis.o pg_plsql_list_ops.o pg_plsql_string_ops.o pg_plsql_expr_evaluation.o

EXTENSION = pg_plsql_graphs
DATA = pg_plsql_graphs--1.0.sql pg_plsql_graphs--unpackaged--1.0.sql

SHLIB_LINK = -ligraph
PG_CPPFLAGS  += -I$(srcdir) -I$(top_builddir)/src/pl/plpgsql/src/



ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/pg_plsql_graphs
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif

