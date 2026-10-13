# PlatformIO post-build script: support the conventional `pio run -t build` command.
Import("env")

env.AddCustomTarget(
    "build",
    env["PIOMAINPROG"],
    None,
    "Building firmware",
)
