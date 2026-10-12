"""Compile a focused probe against the current CMake production objects.

Run from Data: python src/world/tests/run_native_probe.py <probe.cpp>.
Use --replace-ypabact for gameplay_parameters_test.cpp, which includes that
production unit to exercise its private Smart weapon selector.
"""

import argparse
from pathlib import Path
import os
import re
import shlex
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("--replace-ypabact", action="store_true")
    parser.add_argument("probe_args", nargs="*")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    build = root / "Build"
    compiler = Path("C:/msys64/mingw64/bin/c++.exe")
    ninja = compiler.parent / "ninja.exe"
    env = dict(os.environ)
    env["PATH"] = str(compiler.parent) + os.pathsep + env.get("PATH", "")
    commands = subprocess.check_output([str(ninja), "-C", str(build), "-t", "commands", "OpenNeoUA"], text=True, env=env).splitlines()
    compile_line = next(line for line in commands if "speech.cpp.obj" in line and " -c " in line)
    compile_args = shlex.split(compile_line.replace("\\", "/"))[1:]
    flags = compile_args[:compile_args.index("-MD")]
    output = build / "probes"
    output.mkdir(exist_ok=True)
    name = args.source.stem
    obj, executable = output / f"{name}.obj", output / f"{name}.exe"
    subprocess.run([str(compiler), *flags, "-O0", "-c", str(args.source.resolve()), "-o", str(obj)], cwd=build, env=env, check=True)
    link_line = commands[-1].replace("\\", "/")
    tail = re.search(r"c\+\+\.exe\s+(.*?)(?: && cd \.|$)", link_line).group(1)
    link_args = shlex.split(tail)
    objects = [arg for arg in link_args if arg.endswith(".obj") and not (args.replace_ypabact and arg.endswith("/ypabact.cpp.obj"))]
    libraries = [arg for arg in link_args if arg.startswith("-l") or arg.endswith(".a")]
    response = output / f"{name}.rsp"
    response.write_text("\n".join('"' + arg.replace("\\", "/") + '"' for arg in [str(obj), *objects, *libraries, "-o", str(executable)]), encoding="utf-8")
    subprocess.run([str(compiler), "@" + str(response)], cwd=build, env=env, check=True)
    return subprocess.run([str(executable), *args.probe_args], cwd=root.parent, env=env).returncode


if __name__ == "__main__":
    raise SystemExit(main())
