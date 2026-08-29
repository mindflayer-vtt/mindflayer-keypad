"""Additive PlatformIO integration for an app-only rBoot ROM image."""

import os
import shutil
import subprocess

Import("env")

build_dir = env.subst("$BUILD_DIR")
framework_dir = env.PioPlatform().get_package_dir("framework-arduinoespressif8266")
toolchain_dir = env.PioPlatform().get_package_dir("toolchain-xtensa")
sdk_lib = os.path.join(
    framework_dir, "tools", "sdk", "lib", "NONOSDK22x_190703", "libmain.a"
)
patched_lib = os.path.join(build_dir, "rboot-sdk", "libmain.a")


def patch_sdk_main(target, source, env):
    del target, source, env
    os.makedirs(os.path.dirname(patched_lib), exist_ok=True)
    shutil.copy2(sdk_lib, patched_lib)
    objcopy = os.path.join(toolchain_dir, "bin", "xtensa-lx106-elf-objcopy")
    subprocess.check_call([objcopy, "-W", "Cache_Read_Enable_New", patched_lib])


env.AddPreAction("$BUILD_DIR/${PROGNAME}.elf", patch_sdk_main)
env.Prepend(LIBPATH=[os.path.dirname(patched_lib)])
env.Append(LINKFLAGS=["-u", "Cache_Read_Enable_New"])
link_map = os.path.join(build_dir, "firmware.map")
env.Append(LINKFLAGS=["-Wl,-Map," + link_map])


def verify_rboot_elf(target, source, env):
    del source
    subprocess.check_call([
        env.subst("$PYTHONEXE"),
        os.path.join(env.subst("$PROJECT_DIR"), "scripts", "verify-rboot-elf.py"),
        "--elf", str(target[0]), "--map", link_map, "--toolchain-bin",
        os.path.join(toolchain_dir, "bin"),
    ])


env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", verify_rboot_elf)


def make_app_only_image(target, source, env):
    del target
    elf = str(source[0])
    output = os.path.join(build_dir, "rboot-app.bin")
    script = os.path.join(env.subst("$PROJECT_DIR"), "scripts", "elf2rboot.py")
    subprocess.check_call(
        [
            env.subst("$PYTHONEXE"),
            script,
            "--elf",
            elf,
            "--output",
            output,
            "--toolchain-bin",
            os.path.join(toolchain_dir, "bin"),
        ]
    )


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", make_app_only_image)

