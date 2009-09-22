# vi: syntax=python:et:ts=4
import __builtin__
old_import = __builtin__.__import__
import SCons, sys

def sconspp_import(name, globals=None, locals=None, fromlist=None, blah=None, **kw):
    try:
        return old_import(name, globals, locals, fromlist, **kw)
    except ImportError:
        module = name.split(".")
        if module[0] == "SCons":
            old_import(".".join(module[1:]), globals, locals, fromlist, **kw)
            setattr(SCons, module[1], old_import(module[1]))
            sys.modules["SCons." + module[1]] = old_import(module[1])
            return SCons
        else:
            raise

__builtin__.__import__ = sconspp_import
