# vi: syntax=python:et:ts=4
import sys, os
from glob import glob

def CheckPython(context):
    env = context.env
    backup = env.Clone().Dictionary()
    context.Message("Checking for Python... ")
    if not env.get("python_dir"):
        import distutils.sysconfig
        env.AppendUnique(CPPPATH = distutils.sysconfig.get_python_inc())
        version = distutils.sysconfig.get_config_var("VERSION")
        if not version:
            version = sys.version[:3]
        if env["PLATFORM"] == "win32":
            version = version.replace('.', '')
        env.AppendUnique(LIBPATH = distutils.sysconfig.get_config_var("LIBDIR") or \
                                   os.path.join(distutils.sysconfig.get_config_var("prefix"), "libs") )
        env.AppendUnique(LIBS = "python" + version)
    else:
        env.AppendUnique(CPPPATH = [os.path.join(env["python_dir"], "include")], LIBPATH = [os.path.join(env["python_dir"], "libs")])
        lib = glob(os.path.join(env["python_dir"], "libs", "libpython2?.a"))[0]
        lib = os.path.basename(lib)[3:-2]
        env.AppendUnique(LIBS = [lib])

    test_program = """
    #include <Python.h>
    int main()
    {
        Py_Initialize();
    }
    \n"""
    if context.TryLink(test_program, ".c"):
        from copy import copy
        pyext_builder = copy(env["BUILDERS"]["SharedLibrary"])
        pyext_builder.prefix = ''
        if not env.get("python_dir"):
            pyext_builder.suffix = distutils.sysconfig.get_config_var("SO")
        else:
            pyext_builder.suffix = ".pyd"
        env["BUILDERS"]["PythonExtension"] = pyext_builder
        context.Result("yes")
        return True
    else:
        context.Result("no")
        env.Replace(**backup)
        return False

config_checks = { "CheckPython" : CheckPython }
