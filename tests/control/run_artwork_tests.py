"""Compile and run the portable artwork state model, then UI policy checks."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
compiler = Path(shutil.which("g++") or r"C:\msys64\ucrt64\bin\g++.exe")
env = dict(os.environ)
env["PATH"] = str(compiler.parent) + os.pathsep + env.get("PATH", "")

with tempfile.TemporaryDirectory(prefix="remote01-artwork-") as work:
    binary = Path(work) / ("artwork_tests.exe" if os.name == "nt" else "artwork_tests")
    command = [str(compiler), "-std=c++23", "-O1", "-Wall", "-Wextra", "-Werror",
               "-I", str(root / "components/artwork/include"),
               str(root / "tests/control/artwork_tests.cpp"), "-o", str(binary)]
    subprocess.run(command, cwd=root, env=env, check=True)
    subprocess.run([str(binary)], cwd=root, env=env, check=True)
subprocess.run([shutil.which("python") or "python",
                str(root / "tests/control/artwork_ui_policy_tests.py")],
               cwd=root, check=True)
