#!/usr/bin/env python3
"""Check a gtest XML report from an adapter test run (HARDENING.md check 6/7).

Usage: check_adapter_tests.py <gtest.xml> <HARDENING.md>

Passes only if at least one test case ran to completion and every skipped case is a
documented waiver: its skip message contains "WAIVED:" and its full name
(e.g. AllAdapters/DatabaseAdapterTest.UpdateRecord/MySQL) is listed in backticks in
the "Waived adapter tests" section of HARDENING.md. Test failures are reported by
the test executable's exit status, not by this script.
"""

import re
import sys
import xml.etree.ElementTree as ET


def waived_tests(runbook_path):
    text = open(runbook_path, encoding="utf-8").read()
    section = text.split("### Waived adapter tests", 1)
    if len(section) < 2:
        sys.exit(f"FAIL: no '### Waived adapter tests' section in {runbook_path}")
    body = re.split(r"\n#{2,3} ", section[1], maxsplit=1)[0]
    return set(re.findall(r"`([\w/]+\.[\w/]+)`", body))


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    xml_path, runbook_path = sys.argv[1], sys.argv[2]
    waived = waived_tests(runbook_path)

    completed = 0
    problems = []
    for case in ET.parse(xml_path).getroot().iter("testcase"):
        name = f"{case.get('classname')}.{case.get('name')}"
        result = case.get("result")
        if result == "completed":
            completed += 1
        elif result == "skipped":
            skipped = case.find("skipped")
            message = skipped.get("message", "") if skipped is not None else ""
            if "WAIVED:" not in message:
                problems.append(f"unwaived skip: {name}")
            elif name not in waived:
                problems.append(f"waiver not listed in HARDENING.md: {name}")
        else:
            problems.append(f"unexpected result '{result}': {name}")

    if completed == 0:
        problems.append("no test case ran to completion")
    for problem in problems:
        print(f"FAIL: {problem}")
    if problems:
        sys.exit(1)
    print(f"OK: {completed} test case(s) completed, no unwaived skips")


if __name__ == "__main__":
    main()
