# PlatformIO pre-build script: enable the Cortex-M4 FPU (hard-float ABI).
# PlatformIO's stm32cube framework only passes -mcpu/-mthumb, which builds
# soft-float code. The FreeRTOS ARM_CM4F port needs hardware floating point,
# so the same flags are added to the compiler, assembler and linker. Being a
# "pre:" script, the flags are inherited by the HAL framework and all libraries.
Import("env")

fpu_flags = ["-mfpu=fpv4-sp-d16", "-mfloat-abi=hard"]

env.Append(
    CCFLAGS=fpu_flags,
    ASFLAGS=fpu_flags,
    LINKFLAGS=fpu_flags,
)
