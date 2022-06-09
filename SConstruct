SetOption("implicit_cache", True)

def OptionalPath(key, val, env):
    if val:
        PathVariable.PathIsDir(key, val, env)

opts = Variables('options.cache')

opts.AddVariables(
    PathVariable('boostdir', 'Directory of boost installation.', "", OptionalPath),
    PathVariable('boostlibdir', 'Directory where boost libraries are installed.', "", OptionalPath),
    ('boost_suffix', 'Suffix of boost libraries.')
    )

env = Environment(tools = ["default"], toolpath = "scons_tools", variables = opts)

opts.Save('options.cache', env)

import os
env["ENV"]["PATH"] = os.environ.get("PATH")

import sys
sys.path.insert(0, "scons_tools")
import metasconf

custom_tests = metasconf.init_metasconf(env, ["boost", "python_devel"])
conf = env.Configure(custom_tests = custom_tests, config_h = "src/config.hpp")
conf.CheckBoost("system", require_version = "1.44") and \
conf.CheckBoost("filesystem") and \
conf.CheckBoost("thread") and \
conf.CheckBoost("program_options") and \
conf.CheckLibWithHeader("sqlite3", "sqlite3.h", "C") or Exit(1)
conf.Define("PYTHON_MODULES_PATH", "\"" + Dir("python_modules").abspath + "\"")
conf.Finish()

env.ParseConfig("python3-config --embed --includes --ldflags")

env.Append(CPPDEFINES = ["BOOST_FILESYSTEM_VERSION=3"])

env.Append(CCFLAGS = Split("-O0 -ggdb -Werror -fvisibility=hidden"), CXXFLAGS = Split("-Wall -ansi -Wno-deprecated -Wno-parentheses"))
env.Append(CPPPATH = ["#thirdparty/pybind11/include"])
if "gcc" in env["TOOLS"]:
    env.Append(CXXFLAGS = ["-std=c++17"])
env.Append(CPPPATH = ["#/src"])

env.Decider("MD5-timestamp")

def test(name, dir):
    test = AlwaysBuild(env.Alias("test_" + name, [File("scons++"), Dir(dir)], "-cd ${SOURCES[1]}; ${SOURCE.abspath}"))
    Alias("check", test)

test("general", "test")
test("hello", "examples/hello")
test("builders", "test/builders")
test_env = env.Clone()
conf = test_env.Configure(custom_tests = custom_tests)
test_env["have_boost_test"] = conf.CheckBoost("unit_test_framework") and \
conf.CheckBoost("graph")

conf.Finish()
test_env.Append(CPPDEFINES = ["BOOST_TEST_DYN_LINK"])

Export("env", "test_env")
env.SConscript("src/SConscript", variant_dir = "build", duplicate = False)
