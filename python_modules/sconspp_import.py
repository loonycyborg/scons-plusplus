# vi: syntax=python:et:ts=4
import __builtin__
old_import = __builtin__.__import__
import SCons, sys
import warnings

# With python 2.6.2 calling __builtin__.__import__ manually causes DeprecationWarnings about md5 and popen2
warnings.filterwarnings('ignore', category = DeprecationWarning, module = 'sconspp_import')

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
