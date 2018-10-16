SCons++ - Python-scriptable build tool
======================================

PREREQUISITES
-------------
* Python >= 2.5, < 3.0
* boost >= 1.44
   boost::system
   boost::filesystem
   boost::program_options
   Boost Graph Library
* SQLite 3
* scons >= 0.98.5

BUILD
-----
Run scons(scons++ can't build itself yet).
If required, location of boost installation can be overridden:
    scons boostdir=/usr/local/include/boost-1_39 boostlibdir=/usr/local/lib boost_suffix=-gcc43-mt-1_39

`scons check` will run scons++ ad-hoc regression test 
suite and compile "Hello,world" program located in 
examples/hello with scons++

Whenever run, scons++ will leave graph.dot file in current
directory which contains the dependency graph in Graphviz DOT format.
If Graphviz is installed, you can use dot utilty to massage the .dot
file into more appealing form:
    dot -Tpng graph.dot > graph.png