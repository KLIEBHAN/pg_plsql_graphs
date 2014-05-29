pg_plsql_graphs
===============

PostgreSQL contrib extension for creating flow and deplence graphs for plpgsql functions in dot format

Pre-Installation:

- Download the igraph library and install it to your local system. You can use the following link:
http://igraph.org/nightly/get/c/igraph-0.7.1.tar.gz

- Download the PostgresSQL sources via git from git://git.postgresql.org/git/postgresql.git (More details are given here: https://wiki.postgresql.org/wiki/Working_with_Git)


Installation:

- Add the configurations given by the following path file to configure.in of your PostgresSQL sourcecode at the correct places. This will make the igraph library available for PostgresSQL

```
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

- Clone this git repository to the contrib folder. E.g. using the following command (assuming you are on the root folder of the PostgreSQL source)

```
cd ./contrib; git clone https://github.com/BA-KLI/pg_plsql_graphs.git
```

- Edit the Makefile in the contrib folder and add pg_plsql_graphs	to the SUBDIRS

```
...
SUBDIRS = \
		pg_plsql_graphs	\
		adminpack	\
		auth_delay	\
		auto_explain	\
		btree_gin	\
...
```

- Go back to the root folder of the PostgreSQL source and type the following:

```
autoconf; ./configure --with-igraph; make; sudo make install
```

- This should install Postgres for you. If you have problems or need more information take a look at http://www.postgresql.org/docs/9.3/static/installation.html


- Open your postgres.conf configuration file (you can find out the path of the file by starting up the postgres server and then typing the following command in the psql client: SHOW config_file;)

- In the postgres.conf ddd pg_plsql_graphs to shared_preload_libraries. It should look like this:

```
...
shared_preload_libraries = 'pg_plsql_graphs'   # (change requires restart)
...
```

- Restart the Postgres server


Usage:

- Start up psql and type 

```
CREATE EXTENSION pg_plsql_graphs;
```

- Now for every plpgsql function you call a corresponding entry with the flow graph and depencence graph in dot format is created in the pg_plsql_graphs view.

- You can query this view e.g. by typing: 

```
SELECT * FROM pg_plsql_graphs;
```

- This will show you an indented version of the dot graphs of the previously called plpgsql functions. There is also a not indented version that is optimized for exports to external files called pg_plsql_graphs_trimmed.

- In order to export a graph to a external file (e.g. to convert it to png) you can use the COPY function. The following commands will copy the flow graph and the depence graph of the last called plpgsql function to external dot files. To do that the views pg_plsql_last_flowgraph_dot and  pg_plsql_last_pdgs_dot are used.

```
\COPY (select * from pg_plsql_last_flowgraph_dot)  TO 'flow.dot';
\COPY (select * from pg_plsql_last_pdgs_dot)  TO 'pdg.dot';
```

- You can now convert these dot files e.g. to the png format using the following graphviz commands

```
dot -Tpng 'flow.dot' > flow.png
dot -Tpng 'pdg.dot' > pdg.png
```
