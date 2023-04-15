![CI](https://github.com/loonycyborg/scons-plusplus/actions/workflows/ci.yml/badge.svg)
![GitHub](https://img.shields.io/github/license/loonycyborg/scons-plusplus)

SCons++ - Python-scriptable build tool
======================================

PREREQUISITES
-------------
* Python >= 3.0
* pybind11
* boost >= 1.44
  * boost::system
  * boost::filesystem
  * boost::program_options
  * Boost Graph Library
* SQLite 3
* scons >= 3.0.0

BUILD
-----
Run scons
If required, location of boost installation can be overridden:
    scons boostdir=/usr/local/include/boost-1_39 boostlibdir=/usr/local/lib boost_suffix=-gcc43-mt-1_39

RUNNING
-------
running resulting `scons++` program will try to find SConstruct++ or SContruct file
in current directory and build it as a SCons script. `scons++ -F make` will work in Make
mode instead, parsing Makefiles. Running `./scons++ --help` will show all options.

`scons check` will run scons++ ad-hoc regression test 
suite and compile "Hello,world" program located in 
examples/hello with scons++

Whenever run, scons++ will leave graph.dot file in current
directory which contains the dependency graph in Graphviz DOT format.
If Graphviz is installed, you can use dot utilty to massage the .dot
file into more appealing form:
    dot -Tpng graph.dot > graph.png
