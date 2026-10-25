"""Compile/run the portable Core, without PlatformIO-native or UI dependencies."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument("--compiler", default=shutil.which("g++") or r"C:\msys64\ucrt64\bin\g++.exe")
p.add_argument("--sanitize", action="store_true", help="enable host undefined-behavior checks")
stress = p.add_mutually_exclusive_group()
stress.add_argument("--soak", action="store_true", help="run tenfold finite Phase C stress patterns")
stress.add_argument("--measure", action="store_true", help="run the Phase C allocation benchmark after existing suites")
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
compiler = Path(a.compiler)
env = dict(os.environ)
env["PATH"] = str(compiler.parent) + os.pathsep + env.get("PATH", "")
with tempfile.TemporaryDirectory(prefix="remote01-core-") as work:
    binary = Path(work) / ("core_tests.exe" if os.name == "nt" else "core_tests")
    cmd = [str(compiler), "-std=c++23", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
           "-fno-exceptions", "-fno-rtti",
           "-I", str(root / "components/control/include"),
           str(root / "components/control/core.cpp"),
           str(root / "components/control/ingress.cpp"),
           str(root / "components/control/mock_adapter.cpp"),
           str(root / "tests/control/core_tests.cpp"), "-o", str(binary)]
    if a.sanitize:
        cmd[1:1] = ["-fsanitize=undefined", "-fno-sanitize-recover=all"]
    print("COMPILE:", " ".join(cmd), flush=True)
    subprocess.run(cmd, cwd=root, env=env, check=True)
    result = subprocess.run([str(binary)], cwd=root, env=env).returncode
    if result:
        raise SystemExit(result)
    profile_binary = Path(work) / ("profile_tests.exe" if os.name == "nt" else "profile_tests")
    profile_cmd = cmd[:-2] + ["-I", str(root / "components/control_profile/include"),
                             str(root / "components/control_profile/profile.cpp"),
                             str(root / "tests/control/profile_tests.cpp"),
                             "-o", str(profile_binary)]
    # Replace the foundation harness, not Core/Mock sources.
    profile_cmd.remove(str(root / "tests/control/core_tests.cpp"))
    print("PROFILE COMPILE:", " ".join(profile_cmd), flush=True)
    subprocess.run(profile_cmd, cwd=root, env=env, check=True)
    subprocess.run([str(profile_binary)], cwd=root, env=env, check=True)
    home_binary = Path(work) / ("home_tests.exe" if os.name == "nt" else "home_tests")
    home_cmd = cmd[:-2] + ["-I", str(root / "components/control_home/include"),
        "-I", str(root / "components/rtcp_client/include"),
        str(root / "components/control_home/facade.cpp"),
        str(root / "components/control_home/development.cpp"),
        str(root / "tests/control/rtcp_host_stubs.cpp"),
        str(root / "tests/control/home_tests.cpp"), "-o", str(home_binary)]
    home_cmd.remove(str(root / "tests/control/core_tests.cpp"))
    print("HOME COMPILE:", " ".join(home_cmd), flush=True)
    subprocess.run(home_cmd, cwd=root, env=env, check=True)
    subprocess.run([str(home_binary)], cwd=root, env=env, check=True)
    ownership_binary = Path(work) / ("rtcp_ownership_tests.exe" if os.name == "nt" else "rtcp_ownership_tests")
    ownership_cmd = cmd[:-2] + ["-I", str(root / "components/control_home/include"),
        "-I", str(root / "components/rtcp_client/include"),
        str(root / "components/control_home/facade.cpp"),
        str(root / "components/control_home/rtcp_device.cpp"),
        str(root / "tests/control/rtcp_ownership_tests.cpp"), "-o", str(ownership_binary)]
    ownership_cmd.remove(str(root / "tests/control/core_tests.cpp"))
    print("RTCP OWNERSHIP COMPILE:", " ".join(ownership_cmd), flush=True)
    subprocess.run(ownership_cmd, cwd=root, env=env, check=True)
    subprocess.run([str(ownership_binary)], cwd=root, env=env, check=True)
    hardening_binary = Path(work) / ("hardening_tests.exe" if os.name == "nt" else "hardening_tests")
    hardening_cmd = list(home_cmd)
    hardening_cmd[hardening_cmd.index(str(root / "tests/control/home_tests.cpp"))] = str(root / "tests/control/hardening_tests.cpp")
    hardening_cmd[-1] = str(hardening_binary)
    print("HARDENING COMPILE:", " ".join(hardening_cmd), flush=True)
    subprocess.run(hardening_cmd, cwd=root, env=env, check=True)
    hardening_args = ["--soak"] if a.soak else ["--measure"] if a.measure else []
    subprocess.run([str(hardening_binary)] + hardening_args, cwd=root, env=env, check=True)
    if a.measure:
        raise SystemExit(0)
    ingress_binary = Path(work) / ("ingress_tests.exe" if os.name == "nt" else "ingress_tests")
    ingress_cmd = list(home_cmd)
    ingress_cmd[ingress_cmd.index(str(root / "tests/control/home_tests.cpp"))] = str(root / "tests/control/ingress_tests.cpp")
    ingress_cmd[-1] = str(ingress_binary)
    ingress_cmd.insert(1, "-pthread")
    print("INGRESS COMPILE:", " ".join(ingress_cmd), flush=True)
    subprocess.run(ingress_cmd, cwd=root, env=env, check=True)
    subprocess.run([str(ingress_binary)] + (["--soak"] if a.soak else []), cwd=root, env=env, check=True)
    power_binary = Path(work) / ("power_tests.exe" if os.name == "nt" else "power_tests")
    power_cmd = list(home_cmd)
    power_cmd[power_cmd.index(str(root / "tests/control/home_tests.cpp"))] = str(root / "tests/control/power_tests.cpp")
    power_cmd[-1] = str(power_binary)
    power_cmd[1:1] = ["-I", str(root / "components/power_management/include"), "-I", str(root / "components/SensorLib/src"), str(root / "components/power_management/model.cpp")]
    print("POWER COMPILE:", " ".join(power_cmd), flush=True)
    subprocess.run(power_cmd, cwd=root, env=env, check=True)
    subprocess.run([str(power_binary)] + (["--soak"] if a.soak else []), cwd=root, env=env, check=True)
    network_binary = Path(work) / ("network_tests.exe" if os.name == "nt" else "network_tests")
    network_cmd = [str(compiler), "-std=c++23", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
        "-fno-exceptions", "-fno-rtti", "-I", str(root / "components/network_manager/include"),
        str(root / "components/network_manager/model.cpp"), str(root / "components/network_manager/presentation.cpp"), str(root / "tests/control/network_tests.cpp"), "-o", str(network_binary)]
    if a.sanitize:
        network_cmd[1:1] = ["-fsanitize=undefined", "-fno-sanitize-recover=all"]
    subprocess.run(network_cmd, cwd=root, env=env, check=True)
    subprocess.run([str(network_binary)] + (["--soak"] if a.soak else []), cwd=root, env=env, check=True)
    sleep_binary = Path(work) / ("network_sleep_tests.exe" if os.name == "nt" else "network_sleep_tests")
    sleep_cmd = [str(compiler), "-std=c++23", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
        "-fno-exceptions", "-fno-rtti", "-I", str(root / "components/network_manager/include"),
        "-I", str(root / "components/power_management/include"),
        str(root / "tests/control/network_sleep_tests.cpp"), "-o", str(sleep_binary)]
    subprocess.run(sleep_cmd, cwd=root, env=env, check=True)
    subprocess.run([str(sleep_binary)] + (["--soak"] if a.soak else []), cwd=root, env=env, check=True)
    external_binary = Path(work) / ("external_power_tests.exe" if os.name == "nt" else "external_power_tests")
    external_cmd = [str(compiler), "-std=c++23", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
        "-fno-exceptions", "-fno-rtti", "-I", str(root / "components/power_management/include"),
        "-I", str(root / "components/diagnostics/include"), str(root / "components/power_management/model.cpp"),
        str(root / "components/diagnostics/model.cpp"), str(root / "tests/control/external_power_tests.cpp"), "-o", str(external_binary)]
    subprocess.run(external_cmd, cwd=root, env=env, check=True)
    subprocess.run([str(external_binary)] + (["--soak"] if a.soak else []), cwd=root, env=env, check=True)
    portal_binary = Path(work) / ("portal_tests.exe" if os.name == "nt" else "portal_tests")
    portal_cmd = list(network_cmd)
    portal_cmd[portal_cmd.index(str(root / "components/network_manager/model.cpp"))] = str(root / "components/network_manager/portal.cpp")
    portal_cmd[portal_cmd.index(str(root / "tests/control/network_tests.cpp"))] = str(root / "tests/control/portal_tests.cpp")
    portal_cmd[-1] = str(portal_binary)
    subprocess.run(portal_cmd, cwd=root, env=env, check=True)
    subprocess.run([str(portal_binary)] + (["--soak"] if a.soak else []), cwd=root, env=env, check=True)

    diagnostic_binary = Path(work) / ("diagnostics_tests.exe" if os.name == "nt" else "diagnostics_tests")
    diagnostic_cmd = [str(compiler), "-std=c++23", "-O1", "-g", "-Wall", "-Wextra", "-Werror", "-fno-exceptions", "-fno-rtti",
        "-I", str(root / "components/diagnostics/include"), "-I", str(root / "components/power_management/include"),
        "-I", str(root / "components/network_manager/include"), str(root / "components/diagnostics/model.cpp"),
        str(root / "tests/control/diagnostics_tests.cpp"), "-o", str(diagnostic_binary)]
    if a.sanitize:
        diagnostic_cmd[1:1] = ["-fsanitize=undefined", "-fno-sanitize-recover=all"]
    subprocess.run(diagnostic_cmd, cwd=root, env=env, check=True)
    subprocess.run([str(diagnostic_binary)] + (["--soak"] if a.soak else []), cwd=root, env=env, check=True)

    physical_binary = Path(work) / ("physical_input_tests.exe" if os.name == "nt" else "physical_input_tests")
    physical_cmd = [str(compiler), "-std=c++23", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
        "-fno-exceptions", "-fno-rtti", "-I", str(root / "include"),
        str(root / "tests/control/physical_input_tests.cpp"), "-o", str(physical_binary)]
    if a.sanitize:
        physical_cmd[1:1] = ["-fsanitize=undefined", "-fno-sanitize-recover=all"]
    subprocess.run(physical_cmd, cwd=root, env=env, check=True)
    subprocess.run([str(physical_binary)] + (["--soak"] if a.soak else []), cwd=root, env=env, check=True)

    # Real WoM code against the frozen pre-instrumentation transaction/delay oracle.
    for enabled in (1, 0):
        wom_binary = Path(work) / ("wom_tests.exe" if os.name == "nt" else "wom_tests")
        wom_cmd = [str(compiler), "-std=c++23", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
            "-fno-exceptions", "-fno-rtti", "-DPOWER_WOM_DIAGNOSTIC=" + str(enabled),
            "-I", str(root / "components/power_management/include"),
            "-I", str(root / "components/SensorLib/src"),
            str(root / "tests/control/wom_diagnostic_tests.cpp"), "-o", str(wom_binary)]
        subprocess.run(wom_cmd, cwd=root, env=env, check=True)
        subprocess.run([str(wom_binary)] + (["--soak"] if a.soak else []), cwd=root, env=env, check=True)

    # Same bounded measurement model in development-enabled and disabled builds.
    stack_binary = Path(work) / ("stack_tests.exe" if os.name == "nt" else "stack_tests")
    for enabled in (1, 0):
        stack_cmd = [str(compiler), "-std=c++23", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
            "-fno-exceptions", "-fno-rtti", "-DPOWER_STACK_MEASUREMENT=" + str(enabled),
            "-I", str(root / "components/power_management/include"),
            str(root / "tests/control/stack_measurement_tests.cpp"), "-o", str(stack_binary)]
        if a.sanitize:
            stack_cmd[1:1] = ["-fsanitize=undefined", "-fno-sanitize-recover=all"]
        subprocess.run(stack_cmd, cwd=root, env=env, check=True)
        subprocess.run([str(stack_binary)] + (["--soak"] if a.soak else []), cwd=root, env=env, check=True)

    # Guard the real serial namespaces, not a duplicated test-only key table.
    import re
    mock_source = (root / "components/control_home/include/control_home/development.hpp").read_text()
    power_source = (root / "components/power_management/runtime.cpp").read_text()
    network_source = (root / "components/network_manager/runtime.cpp").read_text()
    mock_keys = set(re.findall(r"case '([^'])':", mock_source))
    power_keys = set(re.findall(r"case '([^'])':", power_source))
    network_keys = set(re.findall(r"c == '([^'])'", network_source))
    assert "R" not in mock_keys
    assert "R" not in power_keys
    assert network_keys == {"W", "G", "Y", "R"}
    stack_source = (root / "components/power_management/include/power_management/stack_measurement.hpp").read_text()
    stack_keys = set(re.findall(r"c == '([^'])'", stack_source))
    assert stack_keys == {"S", "T"}
    assert not stack_keys & (mock_keys | power_keys | network_keys)
    print("DIAGNOSTICS_NAMESPACE: PASS=1 FAIL=0 assertions=5", flush=True)
    # Real boundary audit: local requests enqueue; HTTP does not acquire mutation APIs.
    request_body = network_source.split("bool settings_request(",1)[1].split("void start()",1)[0]
    assert "Kind::SETTINGS" in request_body and "return post(m)" in request_body
    assert "SettingsAction::FACTORY_RESET" in request_body and "return false" in request_body
    status_body = network_source.split("bool read_status(",1)[1].split("bool read_settings(",1)[0]
    assert "xSemaphoreTake(shared_mutex, 0)" in status_body and "published.status" in status_body
    for relative in ("components/network_manager/web.cpp", "components/diagnostics/service.cpp"):
        assert "settings_request(" not in (root / relative).read_text(encoding="utf-8")
    print("NETWORK_SETTINGS_BOUNDARY: PASS=1 FAIL=0 assertions=5", flush=True)


# Offline bounded artifact checker; never touches a serial port or flash.
import sys
subprocess.run([sys.executable, str(root / "tests/control/ota_preflight_tests.py")],
               cwd=root, check=True)

# Actual synchronous POWER publication helper against bounded host seams.
subprocess.run([sys.executable, str(root / "tests/control/power_publication_tests.py")]
               + (["--soak"] if a.soak else []), cwd=root, check=True)

subprocess.run([sys.executable, str(root / "tests/control/network_sleep_boundary_tests.py")], cwd=root, check=True)

subprocess.run([sys.executable, str(root / "tests/control/external_adc_ui_tests.py")], cwd=root, check=True)
