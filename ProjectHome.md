SCons++ is a build tool written in C++/Python(uses Boost.Python to load build scripts and reuse Platform and Tool modules from SCons). It aims to be compatible with [SCons](http://www.scons.org/) scripts. SCons++ was started to solve severe scalability issues plaguing SCons without brutalizing the scripting interface like Waf does.

# What currently works #
  * Building "Hello, world" program in examples/hello
  * Building SCons++ itself, albeit with simplified build script
  * Incremental rebuilds using SCons's md5/timestamp method to check for file changes
  * Implicit dependency scanning(e.g. for #included headers) using SCons's scanners.

# What's currently missing #
  * Most of SCons api