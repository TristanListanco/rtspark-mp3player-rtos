# PlatformIO pre-build script for the native test environment: the
# sanitizer flags in build_flags only reach the compiler, so pass them to the
# linker as well.
Import("env")

flags = env.GetProjectOption("build_flags", [])
if isinstance(flags, str):
    flags = [flags]
flags = " ".join(flags).split()

env.Append(LINKFLAGS=[f for f in flags
                      if f.startswith("-fsanitize") or f.startswith("-fno-sanitize")])
