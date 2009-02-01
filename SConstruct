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

import sys
sys.path.insert(0, "scons_tools")
import metasconf

conf = env.Configure(custom_tests = metasconf.init_metasconf(env, ["boost", "python_devel"]), config_h = "src/config.hpp")
conf.CheckPython() and \
conf.CheckBoost("python", require_version = "1.36") and \
conf.CheckBoost("system") and \
conf.CheckBoost("filesystem") or Exit(1)
conf.Define("PYTHON_MODULES_PATH", "\"" + Dir("python_modules").abspath + "\"")
conf.Finish()

env.Append(CXXFLAGS = Split("-O0 -ggdb -Wall -ansi"))
env.Append(CPPPATH = ["#/src"])

env.SConscript("src/SConscript", exports = ["env"], variant_dir = "build", duplicate = False)

test       = AlwaysBuild(Alias("test_general", [], "-cd test; ../scons++"))
test_hello = AlwaysBuild(Alias("test_hello",   [], "cd examples/hello; ../../scons++"))
Alias("check", [test, test_hello])
