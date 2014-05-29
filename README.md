pg_plsql_graphs
===============

_PostgreSQL_ _contrib_ extension for creating flow and deplence graphs for _plpgsql functions_ in _dot_ format

##Pre-Installation

- Download the _igraph_ library and install it to your local system. You can use the following link:
http://igraph.org/nightly/get/c/igraph-0.7.1.tar.gz

- Download the _PostgresSQL_ sources via git from git://git.postgresql.org/git/postgresql.git (More details are given here: https://wiki.postgresql.org/wiki/Working_with_Git)


##Installation

- Add the configurations given by the following path file to _configure.in_ of your _PostgresSQL_ sourcecode at the correct places. This will make the _igraph_ library available for _PostgresSQL_

```Diff
--- configure.in
+++ configure.in
@@ -727,6 +727,11 @@ fi
 AC_SUBST(with_uuid)
 AC_SUBST(UUID_EXTRA_OBJS)
 
+#
+# iGraph library
+#
+PGAC_ARG_BOOL(with, igraph, no, [build contrib/pg_plsql_graphs, requires IGraph library])
+AC_SUBST(with_igraph)
 
 #
 # XML
@@ -997,6 +1002,10 @@ elif test "$with_uuid" = ossp ; then
 fi
 AC_SUBST(UUID_LIBS)
 
+#for contrib/pg_plsql_graphs
+if test "$with_igraph" = yes; then
+	AC_CHECK_LIB([igraph], [igraph_adjlist_init], [], [echo "Library Igraph not found!"; exit -1])
+fi
 
 ##
 ## Header files
@@ -1142,6 +1151,13 @@ if test "$PORTNAME" = "win32" ; then
    AC_CHECK_HEADERS(crtdefs.h)
 fi
 
+
+# for contrib/pg_plsql_graphs
+if test "$with_igraph" = yes ; then
+    AC_CHECK_HEADERS(igraph/igraph.h, [],
+      [AC_MSG_ERROR([header file <igraph/igraph.h> is required for iGraph])])
+fi
+
 ##
 ## Types, structures, compiler characteristics
 ##

```

- Clone this git repository to the _contrib_ folder. E.g. using the following command (assuming you are on the root folder of the _PostgreSQL_ source)

```Shell
cd ./contrib; git clone https://github.com/BA-KLI/pg_plsql_graphs.git
```

- Edit the Makefile in the contrib folder and add _pg_plsql_graphs_ to the _SUBDIRS_

```Shell
...
SUBDIRS = \
		pg_plsql_graphs	\
		adminpack	\
		auth_delay	\
		auto_explain	\
		btree_gin	\
...
```

- Go back to the root folder of the _PostgreSQL_ source and type the following:

```Shell
autoconf; ./configure --with-igraph; make; sudo make install
```

- This should install _PostgreSQL_ for you. If you have problems or need more information take a look at http://www.postgresql.org/docs/9.3/static/installation.html


- Open your _postgres.conf_ configuration file.
You can find out the path of the file by starting up the _PostgreSQL_ server and then typing the following command in the _psql_ client: 

```Shell
SHOW config_file;
```

- In the _postgres.conf_ add _pg_plsql_graphs_ to _shared_preload_libraries_. It should look like this:

```Shell
...
shared_preload_libraries = 'pg_plsql_graphs'   # (change requires restart)
...
```

- Restart the _PostgreSQL_ server


##Usage

- Start up _psql_ and type 

```Sql
CREATE EXTENSION pg_plsql_graphs;
```

- Now for every _plpgsql_ function you call, a corresponding entry with the _flow_ and _depencence graphs_ in _dot_-format is created in the _pg_plsql_graphs_ view.

- You can query this view e.g. by typing: 

```Sql
SELECT * FROM pg_plsql_graphs;
```

- This will show you an indented version of the dot graphs of the previously called _plpgsql_ functions. There is also a not indented version that is optimized for exports to external files called _pg_plsql_graphs_trimmed_.

- In order to export a graph to a external file (e.g. to convert it to png) you can use the COPY function. The following commands will copy the flow graph and the depence graph of the last called _plpgsql_ function to external _dot_ files. To do that the views _pg_plsql_last_flowgraph_dot_ and _pg_plsql_last_pdgs_dot_ are used.

```Sql
\COPY (select * from pg_plsql_last_flowgraph_dot)  TO 'flow.dot';
\COPY (select * from pg_plsql_last_pdgs_dot)  TO 'pdg.dot';
```

- You can now convert these _dot_ files e.g. to the png format using the following _graphviz_ commands

```Shell
dot -Tpng 'flow.dot' > flow.png
dot -Tpng 'pdg.dot' > pdg.png
```
