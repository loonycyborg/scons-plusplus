# vi: syntax=python:et:ts=4
import os, sys
from SCons.Environment import Environment
env = Environment(_no_platform = True)

Entry = lambda x : env.Entry(x)

class FS:
    def __init__(self):
        pass
    
    def Entry(self, name):
        return env.Entry(name)
    def File(self, name):
        return env.File(name)
    def Dir(self, name):
        return env.Dir(name)

_is_cygwin = sys.platform == "cygwin"
if os.path.normcase("TeSt") == os.path.normpath("TeSt") and not _is_cygwin:
    def _my_normcase(x):
        return x
else:
    def _my_normcase(x):
        return string.upper(x)

from SCons.Script import FindFile as find_file
