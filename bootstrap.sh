#!/bin/bash -xe

PYTHON_INCLUDE=`python3-config --includes`
PYTHON_LDFLAGS=`python3-config --ldflags --embed`

GXX=g++

BOOTSTRAP_DIR=bootstrap
LIBS="-lboost_system -lboost_filesystem -lboost_thread -lboost_program_options -lsqlite3"

OBJECTS=()

cat > src/config.hpp <<- EOF
#ifndef SRC_CONFIG_HPP
#define SRC_CONFIG_HPP

#define PYTHON_MODULES_PATH "`pwd`/python_modules"

#endif
EOF

for SOURCE in src/*.cpp src/*.c src/python_interface/*.cpp src/make_interface/*.cpp
do
	mkdir -p $BOOTSTRAP_DIR/`dirname $SOURCE`
	OBJECT=$BOOTSTRAP_DIR/${SOURCE%.cpp}.o
	$GXX -c -o $OBJECT -Isrc $PYTHON_INCLUDE -Ithirdparty/pybind11/include -pthread -std=c++17 -Wno-deprecated -Wno-parentheses -fvisibility=hidden -O3 $SOURCE
	OBJECTS+=("$OBJECT")
done

$GXX -o $BOOTSTRAP_DIR/scons++0 -pthread $LIBS $PYTHON_LDFLAGS ${OBJECTS[*]}

$BOOTSTRAP_DIR/scons++0
