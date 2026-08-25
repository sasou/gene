#!/usr/bin/env python3
"""Scan Gene extension C sources for arginfo vs zend_parse_parameters mismatches."""

import re
import os
import sys

SRC = os.path.join(os.path.dirname(__file__), "..", "..", "src")

ARGINFO_RE = re.compile(
    r"ZEND_BEGIN_ARG_INFO_EX\((\w+),\s*0,\s*0,\s*(\d+)\)(.*?)ZEND_END_ARG_INFO\(\)",
    re.DOTALL,
)
ZPP_RE = re.compile(r'zend_parse_parameters\(ZEND_NUM_ARGS\(\),\s*"([^"]+)"')
ZPP_NONE_RE = re.compile(r"zend_parse_parameters_none\(\)")
PHP_METHOD_RE = re.compile(r"PHP_METHOD\((\w+),\s*(\w+)\)")
PHP_ME_RE = re.compile(r"PHP_ME\((\w+),\s*(\w+),\s*(\w+)")
GENE_REQUEST_METHOD_RE = re.compile(r"GENE_REQUEST_METHOD\((\w+),\s*(\w+),\s*\w+\)")

# GENE_REQUEST_METHOD macro always uses "|sz"
GENE_REQUEST_ZPP = "|sz"


def zpp_required(fmt: str) -> int:
    if "|" in fmt:
        return len(fmt.split("|", 1)[0])
    return len(fmt)


def arginfo_total(arginfo_body: str) -> int:
    return len(re.findall(r"ZEND_ARG_(TYPE_)?INFO", arginfo_body))


def scan_file(path: str) -> list:
    with open(path, encoding="utf-8", errors="replace") as f:
        content = f.read()

    arginfos = {}
    for m in ARGINFO_RE.finditer(content):
        name, required, body = m.group(1), int(m.group(2)), m.group(3)
        arginfos[name] = {"required": required, "total": arginfo_total(body), "body": body}

    # method -> arginfo from PHP_ME
    me_map = {}
    for m in PHP_ME_RE.finditer(content):
        ce, method, arginfo = m.group(1), m.group(2), m.group(3)
        me_map[(ce, method)] = arginfo

    issues = []

    # Direct PHP_METHOD blocks
    parts = re.split(r"(?=PHP_METHOD\()", content)
    for part in parts[1:]:
        mm = PHP_METHOD_RE.match(part)
        if not mm:
            continue
        ce, method = mm.group(1), mm.group(2)
        arginfo_name = me_map.get((ce, method))
        if not arginfo_name or arginfo_name not in arginfos:
            continue
        ai = arginfos[arginfo_name]

        zm = ZPP_RE.search(part)
        if zm:
            zpp = zm.group(1)
            req = zpp_required(zpp)
        elif ZPP_NONE_RE.search(part):
            zpp = "(none)"
            req = 0
        else:
            continue

        if req != ai["required"]:
            issues.append(
                {
                    "file": path,
                    "class": ce,
                    "method": method,
                    "arginfo": arginfo_name,
                    "arginfo_required": ai["required"],
                    "zpp": zpp,
                    "zpp_required": req,
                }
            )

    # GENE_REQUEST_METHOD expansions
    for m in GENE_REQUEST_METHOD_RE.finditer(content):
        ce, method = m.group(1), m.group(2)
        arginfo_name = me_map.get((ce, method))
        if not arginfo_name or arginfo_name not in arginfos:
            continue
        ai = arginfos[arginfo_name]
        req = zpp_required(GENE_REQUEST_ZPP)
        if req != ai["required"]:
            issues.append(
                {
                    "file": path,
                    "class": ce,
                    "method": method,
                    "arginfo": arginfo_name,
                    "arginfo_required": ai["required"],
                    "zpp": GENE_REQUEST_ZPP,
                    "zpp_required": req,
                    "via": "GENE_REQUEST_METHOD",
                }
            )

    return issues


def main():
    all_issues = []
    for root, _, files in os.walk(SRC):
        for fn in files:
            if fn.endswith(".c"):
                all_issues.extend(scan_file(os.path.join(root, fn)))

    if not all_issues:
        print("No arginfo/zpp required-count mismatches found.")
        return 0

    print(f"Found {len(all_issues)} mismatch(es):\n")
    for i in all_issues:
        via = f" [{i['via']}]" if "via" in i else ""
        print(
            f"{i['file']}: {i['class']}::{i['method']}(){via}\n"
            f"  arginfo {i['arginfo']}: required={i['arginfo_required']}\n"
            f"  zpp \"{i['zpp']}\": required={i['zpp_required']}\n"
        )
    return 1


if __name__ == "__main__":
    sys.exit(main())
