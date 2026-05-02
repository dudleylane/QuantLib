#!/usr/bin/python

import sys

# MSVC drift-check entries removed -- the fork supports RHEL/CentOS 10+
# only (memory: project_supported_platforms.md), so QuantLib.vcxproj
# / testsuite.vcxproj are no longer maintained-in-lockstep upstream
# artifacts.  The .vcxproj files remain in the tree for any future
# upstream merge but are not gated on.
inputs = [
    ("ql.dist.diff", "Some Makefile.am"),
    ("test-suite.dist.diff", "test-suite/Makefile.am"),
    ("ql.cmake.diff", "ql/CMakeLists.txt"),
    ("test-suite.cmake.diff", "test-suite/CMakeLists.txt"),
]

result = 0


def format(line):
    filename = line[2:].strip()
    if filename.endswith(".hpp"):
        return "header file %s" % filename
    elif filename.endswith(".cpp"):
        return "source file %s" % filename
    else:
        return "file %s" % filename


CYAN = "\033[96m"
RED = "\033[91m"
GREEN = "\033[92m"
BOLD = "\033[1m"
RESET = "\033[0m"

print(BOLD + CYAN + "\n=============================== RESULTS ================================\n" + RESET)

for diffs, target in inputs:
    with open(diffs) as f:
        for line in f:
            if line.startswith("< "):
                print(RED + "%s contains extra %s" % (target, format(line)) + RESET)
                result = 1
            if line.startswith("> "):
                print(RED + "%s doesn't contain %s" % (target, format(line)) + RESET)
                result = 1

if result == 0:
    print(GREEN + "All clear." + RESET)

sys.exit(result)
