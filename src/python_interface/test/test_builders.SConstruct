from SCons.Builder import Builder

env = Environment()

def e(target, source, env):
    return (target, source)

builder = Builder(
    action = { ".blah" : "echo blah_builder targets: $TARGETS sources: $SOURCES" },
    target_factory = env.File,
    prefix = "blah_",
    suffix = "blah",
    src_suffix = "blah",
    emitter = e,
    ensure_suffix = True)

env.Append(BUILDERS = { "Blah" : builder })

env.Command("qux.blah", "SConstruct", "echo targets: $TARGETS  sources: $SOURCES")
blah = env.Blah("foo.bar", "qux")
env.Command("SConstruct.blah", "SConstruct", "echo targets: $TARGETS sources: $SOURCES")
env.Blah("SConstruct")
baz = env.Command("baz", [blah, "blah_SConstruct.blah"], "echo targets: $TARGETS sources: $SOURCES")
