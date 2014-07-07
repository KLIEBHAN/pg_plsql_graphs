# contrib/pg_plsql_graphs/Makefile


top_builddir = ../..

PGFILEDESC = "pg_plsql_graphs"
PGAPPICON = win32


MODULE_big = pg_plsql_graphs



OBJS	=  pg_plsql_graphs.o\
 pl_graphs/pl_stmt_ops.o\
 pl_graphs/pl_graph_ops.o\
 pl_graphs/pl_plstmts2igraph.o\
 pl_graphs/pl_igraph_ops.o\
 pl_graphs/pl_igraph_export.o\
 pl_graphs/pl_igraphanalysis.o\
 pl_graphs/pl_list_ops.o\
 pl_graphs/pl_string_ops.o

EXTENSION = pg_plsql_graphs
DATA = pg_plsql_graphs--1.0.sql pg_plsql_graphs--unpackaged--1.0.sql

LIBS += -L$(top_builddir)/lib 
SHLIB_LINK =-ligraph
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

