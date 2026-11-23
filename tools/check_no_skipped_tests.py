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

"""Fail when a test-suite run skipped a test case.

Usage: check_no_skipped_tests.py <JUnit file> [--config-header <ql/config.hpp of the build>]
                                 [--filtered <file of the cases the filter selects>]

CI runs quantlib-test-suite with --logger=JUNIT,error,<file>:HRF,message.  Boost.Test writes a
case that a precondition or a decorator turned off as <testcase ...><skipped/>, with the reason in
its system-out, and counts it neither as a failure nor as an error: "No errors detected" says
nothing about a case that never ran.  In CI a skip is a gap, not a pass.  This lists every skipped
case with its reason and exits 1 when there is one, and 2 when the file can't be read or lists no
case, so a missing file never reads as a clean run.

CI passes no speed flag (-- --fast or -- --faster), so no case skips by tier there.  The one kind
of skip that isn't a gap is a case that can't hold under indexed coupons: the cases below carry
*precondition(usingAtParCoupons()), and a build that defines QL_USE_INDEXED_COUPON turns them off
by design.  Such a skip passes only when all three hold: its name is listed below, its reason is a
failed precondition, and the build's config header (--config-header) defines
QL_USE_INDEXED_COUPON.  It is still listed, as a notice.  Any other skip fails, and a new
configuration-dependent precondition joins the list by name; nothing passes by its reason alone.

--filtered is for a run that selects some cases with --run_test, as the tsan job selects the cases
labelled "threads" (--run_test=@threads).  Boost.Test reports every case the filter leaves out as
skipped, with the reason 'disabled'.  The file names the cases the filter is meant to select, one
full name (QuantLibTests/<Suite>/<case>) per line, as tools/check_thread_labels.py --write writes
them.  The run then passes only when every listed case is in the log and ran (or is an
indexed-coupon skip, as above), no other case ran, and every other case was left out by the
filter.  A listed case missing from the log fails the run too: the build doesn't compile it, as
when the tsan job's build loses the thread-safe observer that two of the cases need.  It exits 2
when no case ran.
"""

import re
import sys
import xml.etree.ElementTree as ET

# Cases whose *precondition(usingAtParCoupons()) turns them off under QL_USE_INDEXED_COUPON, as
# Boost.Test names them.  Taken from a whole-suite run of an indexed-coupon build.
INDEXED_COUPON_SKIPS = {
    "QuantLibTests/CashFlowTests/testNullFixingDays",
    # Upstream's guard; why the case fails under indexed coupons was never established (#48).
    "QuantLibTests/PiecewiseYieldCurveTests/testBadPreviousCurve",
    "QuantLibTests/PiecewiseYieldCurveTests/testGlobalBootstrap",
    "QuantLibTests/PiecewiseYieldCurveTests/testGlobalBootstrapPenalty",
}

PRECONDITION_TOKEN = "reason: 'precondition failed"
FILTERED_TOKEN = "reason: 'disabled'"


def uses_indexed_coupons(path):
    """Whether the config header defines QL_USE_INDEXED_COUPON (autotools' and CMake's forms)."""
    with open(path, encoding="utf-8") as f:
        return any(re.match(r"\s*#\s*define\s+QL_USE_INDEXED_COUPON\b", line) for line in f)


def reason_of(output):
    """The reason Boost.Test gave for turning the case off, from its system-out."""
    for line in output.splitlines():
        if "reason:" in line:
            return line.strip().lstrip("- ")[:300]
    return output.strip().splitlines()[0][:300] if output.strip() else "no reason given"


def case_name(case):
    """A JUnit test case's full name as Boost.Test writes it on its command line."""
    return case.get("classname", "").replace(".", "/") + "/" + case.get("name", "")


def main(argv):
    args = argv[1:]
    options = {"--config-header": None, "--filtered": None}
    usage = ("usage: check_no_skipped_tests.py <JUnit file> [--config-header <ql/config.hpp>] "
             "[--filtered <selected cases>]")
    if not args or args[0].startswith("--"):
        print(usage, file=sys.stderr)
        return 2
    rest = args[1:]
    while rest:
        if len(rest) < 2 or rest[0] not in options or options[rest[0]] is not None:
            print(usage, file=sys.stderr)
            return 2
        options[rest[0]] = rest[1]
        rest = rest[2:]
    config_header = options["--config-header"]
    selected = None
    if options["--filtered"] is not None:
        try:
            with open(options["--filtered"], encoding="utf-8") as f:
                selected = {line.strip() for line in f if line.strip() and not line.startswith("#")}
        except OSError as e:
            print(f"::error::cannot read the list of selected cases {options['--filtered']}: {e}")
            return 2
        if not selected:
            print(f"::error::the list of selected cases {options['--filtered']} is empty")
            return 2
    try:
        root = ET.parse(args[0]).getroot()
    except (OSError, ET.ParseError) as e:
        print(f"::error::cannot read the test suite's JUnit file {args[0]}: {e}")
        return 2
    indexed = False
    if config_header is not None:
        try:
            indexed = uses_indexed_coupons(config_header)
        except OSError as e:
            print(f"::error::cannot read the config header {config_header}: {e}")
            return 2
    cases = list(root.iter("testcase"))
    if not cases:
        print(f"::error::{args[0]} lists no test case at all")
        return 2
    if selected is not None:
        return check_filtered(args[0], cases, selected, indexed)
    gaps, allowed = [], []
    for case in cases:
        if case.find("skipped") is None:
            continue
        name = case_name(case)
        output = case.findtext("system-out") or ""
        if indexed and name in INDEXED_COUPON_SKIPS and PRECONDITION_TOKEN in output:
            allowed.append(name)
        else:
            gaps.append((name, reason_of(output)))
    for name in allowed:
        print(f"::notice::{name} was skipped: it can't hold under indexed coupons, which this build uses")
    if gaps:
        print(f"::error::{len(gaps)} of {len(cases)} test cases were skipped; in CI a skip is a gap, not a pass")
        for name, why in gaps:
            print(f"  {name}  ({why})")
        return 1
    ran = len(cases) - len(allowed)
    if allowed:
        print(f"no gap: {ran} of {len(cases)} test cases ran, {len(allowed)} skipped under indexed coupons (listed above)")
    else:
        print(f"no test case was skipped ({len(cases)} ran)")
    return 0


def check_filtered(path, cases, selected, indexed):
    """--filtered: the listed cases ran, nothing else did, and everything else was filtered out."""
    gaps, unexpected, allowed, ran = [], [], [], []
    filtered_out = 0
    present = set()
    for case in cases:
        name = case_name(case)
        output = case.findtext("system-out") or ""
        skipped = case.find("skipped") is not None
        if name in selected:
            present.add(name)
            if not skipped:
                ran.append(name)
            elif indexed and name in INDEXED_COUPON_SKIPS and PRECONDITION_TOKEN in output:
                allowed.append(name)
            else:
                gaps.append((name, reason_of(output)))
        elif not skipped:
            unexpected.append(name)
        elif FILTERED_TOKEN in output:
            filtered_out += 1
        else:
            gaps.append((name, reason_of(output)))
    absent = sorted(selected - present)
    for name in allowed:
        print(f"::notice::{name} was skipped: it can't hold under indexed coupons, which this build uses")
    if not ran:
        print(f"::error::{path}: no test case ran, so the filter selected nothing")
        return 2
    if gaps or unexpected or absent:
        if absent:
            print(f"::error::{len(absent)} listed test cases are not in the log, so this build doesn't compile "
                  f"them")
            for name in absent:
                print(f"  {name}")
        if gaps:
            print(f"::error::{len(gaps)} test cases were skipped other than by the filter; in CI a skip is a gap")
            for name, why in gaps:
                print(f"  {name}  ({why})")
        if unexpected:
            print(f"::error::{len(unexpected)} test cases ran that the list doesn't name; the filter and the "
                  f"list disagree")
            for name in unexpected:
                print(f"  {name}")
        return 1
    print(f"no gap: {len(ran)} of the {len(selected)} listed test cases ran, and the filter left out the "
          f"other {filtered_out}:")
    for name in sorted(ran):
        print(f"  {name}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
