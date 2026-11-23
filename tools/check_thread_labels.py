#!/usr/bin/env python3

# Copyright (C) 2026 dudleylane
#
# This file is new in the dudleylane fork of QuantLib and is licensed
# under the GNU Affero General Public License v3.0 or later (AGPL-3.0+).
# See the LICENSE file in the repository root for the full license text.
#
# Unlike upstream QuantLib files (which remain under BSD-3-Clause — see
# LICENSE.TXT), this file and any derivative works of it are subject to
# the copyleft and network-use clauses of AGPL-3.0.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the license for more details.

"""Fail when code that CI's tsan job doesn't run could start a thread.

Usage: check_thread_labels.py [--write <file>]        (from the repository root)
       check_thread_labels.py --configured [<directory>]

The tsan job (.github/workflows/sanitizers.yml) runs only the test cases labelled "threads", written
*boost::unit_test::label("threads") and selected with --run_test=@threads, and no examples.
ThreadSanitizer reports a data race only when two threads touch the same memory, and the other
cases and the examples run on one thread.  That holds while three things do, and the first form
checks them in the source tree:

- every test case that starts a thread carries the label, and no test-suite file starts one
  outside a case's body;
- no example starts a thread;
- the library starts threads only through OpenMP, which the tsan job's build leaves off.

A thread start is std::thread or std::jthread (not their static members, such as
hardware_concurrency()), std::async, boost::thread or boost::thread_group, pthread_create, a
parallel execution policy, or an OpenMP pragma, outside comments and string literals.  In a file
with a using-directive for std or boost, or a using-declaration of one of those names, the bare
names thread, jthread, thread_group and async count too, outside preprocessor lines; a
using-declaration itself is not a start.  Two test-suite files are left out:
paralleltestrunner.hpp starts threads only in a build with the parallel test runner, and
quantlibbenchmark.cpp is quantlib-benchmark's own program, which no sanitizer job runs.  With
--write, the labelled cases' full names (QuantLibTests/<Suite>/<case>) go to the file, one per
line, for check_no_skipped_tests.py --filtered.

The second form checks a configured autotools tree instead (default: the current directory): no
OpenMP flag in its CXXFLAGS and no parallel test runner in its ql/config.hpp, the two ways the
library or the test suite would start threads in every case.

Exits 1 on a violation, 2 when it can't read what it needs, and 0 otherwise.
"""

import os
import re
import sys

LABEL = "threads"

THREAD_START = re.compile(
    r"\bstd::j?thread\b(?!\s*::)"
    r"|\bstd::async\b"
    r"|\bboost::thread(?:_group)?\b(?!\s*::)"
    r"|\bpthread_create\b"
    r"|\bstd::execution::par(?:_unseq)?\b"
    r"|#\s*pragma\s+omp\b"
)
OPENMP_PRAGMA = re.compile(r"#\s*pragma\s+omp\b")
USING_NAMES = re.compile(
    r"\busing\s+namespace\s+(?:std|boost)\s*;"
    r"|\busing\s+(?:std|boost)::(?:j?thread|thread_group|async)\s*;"
)
BARE_NAME = re.compile(r"(?<![\w:.])(?<!->)(?:j?thread|thread_group|async)\b(?!\s*::)")
DIRECTIVE = re.compile(r"^[ \t]*#.*$", re.M)

TEST_CASE = re.compile(r"\bBOOST_(?:AUTO|FIXTURE|DATA)_TEST_CASE(?:_TEMPLATE)?\s*\(")
SUITE_OPEN = re.compile(r"\bBOOST_(?:AUTO|FIXTURE)_TEST_SUITE\s*\(")
SUITE_END = re.compile(r"\bBOOST_AUTO_TEST_SUITE_END\s*\(")
HAS_LABEL = re.compile(r'\blabel\s*\(\s*"' + LABEL + r'"\s*\)')

# Test-suite files that start threads outside what the tsan job runs, and why that's safe.
EXCLUDED = {
    "test-suite/paralleltestrunner.hpp": "only in a build with the parallel test runner, which --configured rules out",
    "test-suite/quantlibbenchmark.cpp": "quantlib-benchmark's own program, which no sanitizer job runs",
}


LEXEME = re.compile(
    r"//[^\n]*"  # a line comment
    r"|/\*.*?(?:\*/|\Z)"  # a block comment
    r'|(?<!\w)(?:u8|[uUL])?R"(?P<d>[^()\\\s]{0,16})\(.*?(?:\)(?P=d)"|\Z)'  # a raw string
    r'|"(?:\\.|[^"\\\n])*(?:"|$)'  # a string
    r"|(?<![A-Za-z0-9])'(?:\\.|[^'\\\n])*(?:'|$)",  # a character; after a digit, ' separates digits
    re.S | re.M,
)


def blanked(text):
    """Spaces in place of every character of text but its line breaks."""
    return "\n".join(" " * len(part) for part in text.split("\n"))


def strip(src):
    """The source with its comments and its string and character literals blanked.

    Offsets and line breaks are kept, so positions in the result are positions in the source."""
    return LEXEME.sub(lambda m: blanked(m.group(0)), src)


def matching(text, start, opening, closing):
    """The offset of the bracket closing the one at text[start], or -1."""
    depth = 0
    for k in range(start, len(text)):
        if text[k] == opening:
            depth += 1
        elif text[k] == closing:
            depth -= 1
            if depth == 0:
                return k
    return -1


def first_argument(text, open_paren, close_paren):
    depth = 0
    for k in range(open_paren + 1, close_paren):
        if text[k] in "(<":
            depth += 1
        elif text[k] in ")>":
            depth -= 1
        elif text[k] == "," and depth == 0:
            return text[open_paren + 1:k].strip()
    return text[open_paren + 1:close_paren].strip()


def line_of(text, offset):
    return text.count("\n", 0, offset) + 1


def thread_starts(code):
    """The offset and text of each thread start in stripped code, in order."""
    sites = []
    if not any(word in code for word in ("thread", "async", "execution", "pragma")):
        return sites
    for m in THREAD_START.finditer(code):
        line_start = code.rfind("\n", 0, m.start()) + 1
        if not re.search(r"\busing\s+$", code[line_start:m.start()]):
            sites.append((m.start(), m.group(0)))
    if USING_NAMES.search(code):
        # Blank the preprocessor lines (#include <thread>, #error ... thread-safe ...), keeping offsets.
        plain = DIRECTIVE.sub(lambda d: " " * len(d.group(0)), code)
        sites += [(m.start(), m.group(0)) for m in BARE_NAME.finditer(plain)]
        sites.sort()
    return sites


def test_cases(path, src, code, errors):
    """Each test case in a test-suite file: (full name, labelled, body start, body end, line)."""
    events = []
    for kind, pattern in (("open", SUITE_OPEN), ("end", SUITE_END), ("case", TEST_CASE)):
        events += [(m.start(), kind, m) for m in pattern.finditer(code)]
    events.sort(key=lambda e: e[0])
    suites, cases = [], []
    for _, kind, m in events:
        if kind == "end":
            if suites:
                suites.pop()
            continue
        open_paren = m.end() - 1
        close_paren = matching(code, open_paren, "(", ")")
        if close_paren < 0:
            errors.append((path, line_of(code, m.start()), "can't find the end of this macro's arguments"))
            continue
        name = first_argument(code, open_paren, close_paren)
        if kind == "open":
            suites.append(name)
            continue
        body = close_paren + 1
        while body < len(code) and code[body].isspace():
            body += 1
        if body >= len(code) or code[body] != "{":
            errors.append((path, line_of(code, m.start()), f"can't find the body of test case {name}"))
            continue
        end = matching(code, body, "{", "}")
        if end < 0:
            errors.append((path, line_of(code, m.start()), f"can't find the end of test case {name}'s body"))
            continue
        labelled = HAS_LABEL.search(src, m.start(), close_paren + 1) is not None
        cases.append(("/".join(suites + [name]), labelled, body, end, line_of(code, m.start())))
    return cases


def source_files(top):
    for directory, _, files in os.walk(top):
        for f in sorted(files):
            if f.endswith((".cpp", ".hpp")):
                yield os.path.join(directory, f).replace(os.sep, "/")


def check_sources(write_to):
    errors, labelled, starting = [], [], set()
    for top in ("test-suite", "Examples", "ql"):
        if not os.path.isdir(top):
            print(f"::error::no {top}/ directory here; run this from the repository root")
            return 2
    for path in sorted(source_files("test-suite")):
        try:
            with open(path, encoding="utf-8", errors="replace") as f:
                src = f.read()
        except OSError as e:
            print(f"::error::cannot read {path}: {e}")
            return 2
        code = strip(src)
        sites = thread_starts(code)
        cases = test_cases(path, src, code, errors)
        labelled += [c[0] for c in cases if c[1]]
        if path in EXCLUDED or not sites:
            continue
        for offset, _ in sites:
            where = [c for c in cases if c[2] < offset < c[3]]
            line = line_of(code, offset)
            if not where:
                errors.append((path, line, "starts a thread outside a test case's body, so the tsan job can't "
                               "tell which cases reach it: move it into the cases, or label every case that "
                               "calls it and extend tools/check_thread_labels.py"))
                continue
            name, has_label, _, _, case_line = where[0]
            starting.add(name)
            if not has_label:
                errors.append((path, case_line, f"test case {name} starts a thread (line {line}) but isn't "
                               f'labelled "{LABEL}", so the tsan job would never run it: add '
                               f'*boost::unit_test::label("{LABEL}") to its declaration'))
    for top, allow_openmp, why in (
            ("Examples", False, "an example starts a thread, but the tsan job doesn't run the examples"),
            ("ql", True, "the library starts a thread outside OpenMP, but the tsan job runs only the labelled "
                         "cases, on the assumption that it starts none")):
        for path in source_files(top):
            try:
                with open(path, encoding="utf-8", errors="replace") as f:
                    code = strip(f.read())
            except OSError as e:
                print(f"::error::cannot read {path}: {e}")
                return 2
            for offset, text in thread_starts(code):
                if allow_openmp and OPENMP_PRAGMA.match(text):
                    continue
                errors.append((path, line_of(code, offset), why))
    for name in sorted(set(labelled) - starting):
        print(f'::notice::{name} is labelled "{LABEL}" but starts no thread itself; the tsan job runs it anyway')
    if write_to is not None:
        try:
            with open(write_to, "w", encoding="utf-8") as f:
                f.writelines(name + "\n" for name in sorted(labelled))
        except OSError as e:
            print(f"::error::cannot write {write_to}: {e}")
            return 2
    if errors:
        for path, line, message in errors:
            print(f"::error file={path},line={line}::{message}")
        return 1
    print(f'{len(labelled)} test cases are labelled "{LABEL}"; no code outside them starts a thread:')
    for name in sorted(labelled):
        print(f"  {name}")
    return 0


def check_configured(directory):
    errors = []
    try:
        with open(os.path.join(directory, "Makefile"), encoding="utf-8") as f:
            cxxflags = [line for line in f if line.startswith("CXXFLAGS =")]
        with open(os.path.join(directory, "ql", "config.hpp"), encoding="utf-8") as f:
            config = f.read()
    except OSError as e:
        print(f"::error::cannot read the configured tree in {directory}: {e}")
        return 2
    if not cxxflags:
        print(f"::error::{directory}/Makefile has no CXXFLAGS line")
        return 2
    if re.search(r"(^|\s)-[fq]?openmp\S*", cxxflags[0][len("CXXFLAGS ="):]):
        errors.append("this build compiles with OpenMP, so the library starts threads in cases the tsan job "
                      "doesn't run")
    if re.search(r"^\s*#\s*define\s+QL_ENABLE_PARALLEL_UNIT_TEST_RUNNER\b", config, re.M):
        errors.append("this build uses the parallel test runner, which runs every case on its own threads")
    for message in errors:
        print(f"::error::{message}")
    if errors:
        return 1
    print("the configured build has no OpenMP and no parallel test runner")
    return 0


def main(argv):
    args = argv[1:]
    if args[:1] == ["--configured"] and len(args) <= 2:
        return check_configured(args[1] if len(args) == 2 else ".")
    if not args:
        return check_sources(None)
    if len(args) == 2 and args[0] == "--write":
        return check_sources(args[1])
    print("usage: check_thread_labels.py [--write <file>] | --configured [<directory>]", file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
