"""Compile integration probes and reuse a completed Ninja build's real game objects.

Usage: python run_runtime_probe.py BUILD_DIR probe.cpp [probe.cpp ...]
Requires CMake's CMAKE_EXPORT_COMPILE_COMMANDS=ON and Ninja on PATH.
"""
import ctypes
import json
from pathlib import Path
import subprocess
import sys


def windows_args(command):
    count = ctypes.c_int()
    split = ctypes.windll.shell32.CommandLineToArgvW
    split.argtypes = [ctypes.c_wchar_p, ctypes.POINTER(ctypes.c_int)]
    split.restype = ctypes.POINTER(ctypes.c_wchar_p)
    pointer = split(command, ctypes.byref(count))
    try:
        return [pointer[i] for i in range(count.value)]
    finally:
        free = ctypes.windll.kernel32.LocalFree
        free.argtypes = [ctypes.c_void_p]
        free(pointer)


def main():
    build = Path(sys.argv[1]).resolve()
    database = json.loads((build / "compile_commands.json").read_text())
    main_entry = next(entry for entry in database if Path(entry["file"]).name == "main.cpp")
    compile_command = windows_args(main_entry["command"])
    game_directory = Path(main_entry["file"]).resolve().parents[2]
    commands = json.loads(subprocess.check_output(
        ["ninja", "-C", str(build), "-t", "compdb", "-x"], text=True))
    link_command = next(entry["command"] for entry in commands
                        if entry["output"] == "OpenNeoUA.exe")
    link = windows_args(link_command.split(" && ")[1])
    globals_source = build / "runtime_probe_globals.cpp"
    globals_obj = build / "runtime_probe_globals.obj"
    globals_source.write_text('#define GLOBAL\n#include "common/includes.h"\n', encoding="utf-8")
    globals_compile = compile_command.copy()
    globals_compile[globals_compile.index("-o") + 1] = str(globals_obj)
    globals_compile[globals_compile.index("-c") + 1] = str(globals_source)
    subprocess.run(globals_compile, cwd=build, check=True)
    for name in sys.argv[2:]:
        source = Path(name).resolve()
        obj = build / (source.stem + ".obj")
        exe = build / (source.stem + ".exe")
        compile_args = compile_command.copy()
        compile_args[compile_args.index("-o") + 1] = str(obj)
        compile_args[compile_args.index("-c") + 1] = str(source)
        subprocess.run(compile_args, cwd=build, check=True)
        args = [str(obj) if item.replace("\\", "/").endswith("/main.cpp.obj") else item
                for item in link if item != "-mwindows"]
        args[args.index("-o") + 1] = str(exe)
        args.append(str(globals_obj))
        args = [f"-Wl,--out-implib,{source.stem}.dll.a" if item.startswith("-Wl,--out-implib,")
                else item for item in args]
        response = build / (source.stem + ".rsp")
        response.write_text("\n".join('"' + item.replace("\\", "/").replace('"', '\\"') + '"'
                                      for item in args[1:]), encoding="utf-8")
        subprocess.run([args[0], "@" + str(response)], cwd=build, check=True)
        subprocess.run([str(exe)], cwd=game_directory, check=True)


if __name__ == "__main__":
    main()
