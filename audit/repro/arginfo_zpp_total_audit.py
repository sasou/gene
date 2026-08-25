#!/usr/bin/env python3
"""Scan Gene extension C sources for arginfo total/required vs zend_parse_parameters mismatches."""

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
ZPP_START_RE = re.compile(r"ZEND_PARSE_PARAMETERS_START\((\d+),\s*(\d+)\)")
PHP_METHOD_RE = re.compile(r"PHP_METHOD\((\w+),\s*(\w+)\)")
PHP_ME_RE = re.compile(r"PHP_ME\((\w+),\s*(\w+),\s*(\w+)")
GENE_REQUEST_METHOD_RE = re.compile(r"GENE_REQUEST_METHOD\((\w+),\s*(\w+),\s*\w+\)")


def zpp_total(fmt: str) -> int:
    return len(fmt.replace("|", ""))


def zpp_required(fmt: str) -> int:
    if "|" in fmt:
        return len(fmt.split("|", 1)[0])
    return len(fmt)


def arginfo_total(body: str) -> int:
    return len(re.findall(r"ZEND_ARG_(TYPE_)?INFO", body))


def scan_file(path: str) -> list:
    with open(path, encoding="utf-8", errors="replace") as f:
        content = f.read()

    arginfos = {}
    for m in ARGINFO_RE.finditer(content):
        name, required, body = m.group(1), int(m.group(2)), m.group(3)
        arginfos[name] = {"required": required, "total": arginfo_total(body)}

    me_map = {}
    for m in PHP_ME_RE.finditer(content):
        ce, method, arginfo = m.group(1), m.group(2), m.group(3)
        me_map[(ce, method)] = arginfo

    issues = []

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
            total = zpp_total(zpp)
        elif ZPP_NONE_RE.search(part):
            zpp = "(none)"
            req = 0
            total = 0
        elif (zm2 := ZPP_START_RE.search(part)):
            req = int(zm2.group(1))
            total = int(zm2.group(2))
            zpp = f"START({req},{total})"
        else:
            continue

        if req != ai["required"] or total != ai["total"]:
            issues.append(
                {
                    "file": path,
                    "class": ce,
                    "method": method,
                    "arginfo": arginfo_name,
                    "arginfo_required": ai["required"],
                    "arginfo_total": ai["total"],
                    "zpp": zpp,
                    "zpp_required": req,
                    "zpp_total": total,
                }
            )

    for m in GENE_REQUEST_METHOD_RE.finditer(content):
        ce, method = m.group(1), m.group(2)
        arginfo_name = me_map.get((ce, method))
        if not arginfo_name or arginfo_name not in arginfos:
            continue
        ai = arginfos[arginfo_name]
        zpp = "|sz"
        req = zpp_required(zpp)
        total = zpp_total(zpp)
        if req != ai["required"] or total != ai["total"]:
            issues.append(
                {
                    "file": path,
                    "class": ce,
                    "method": method,
                    "arginfo": arginfo_name,
                    "arginfo_required": ai["required"],
                    "arginfo_total": ai["total"],
                    "zpp": zpp,
                    "zpp_required": req,
                    "zpp_total": total,
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
        print("No arginfo/zpp total/required mismatches found.")
        return 0

    print(f"Found {len(all_issues)} mismatch(es):\n")
    for i in all_issues:
        via = f" [{i['via']}]" if "via" in i else ""
        print(
            f"{i['file']}: {i['class']}::{i['method']}(){via}\n"
            f"  arginfo {i['arginfo']}: required={i['arginfo_required']} total={i['arginfo_total']}\n"
            f"  zpp \"{i['zpp']}\": required={i['zpp_required']} total={i['zpp_total']}\n"
        )
    return 1


if __name__ == "__main__":
    sys.exit(main())
