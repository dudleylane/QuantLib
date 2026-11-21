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


def main(argv):
    args = argv[1:]
    config_header = None
    if len(args) == 3 and args[1] == "--config-header":
        config_header = args[2]
    elif len(args) != 1:
        print("usage: check_no_skipped_tests.py <JUnit file> [--config-header <ql/config.hpp>]",
              file=sys.stderr)
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
    gaps, allowed = [], []
    for case in cases:
        if case.find("skipped") is None:
            continue
        name = case.get("classname", "").replace(".", "/") + "/" + case.get("name", "")
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


if __name__ == "__main__":
    sys.exit(main(sys.argv))
