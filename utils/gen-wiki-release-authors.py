#!/usr/bin/env python
# -*- coding: utf-8 -*-

import subprocess
import sys

if len(sys.argv) != 2 and len(sys.argv) != 3:
    print("usage: %s <from> [<to>]" % sys.argv[0])
    raise SystemExit(1)

rev_start = sys.argv[1]

if len(sys.argv) > 2:
    rev_end = sys.argv[2]
else:
    rev_end = "HEAD"

command = ["git", "log", "--pretty=%an", rev_start + "..." + rev_end]

authors = {}
for line in subprocess.check_output(command).decode('utf-8').split("\n"):
    if not line.strip():
        continue
    authors[line.strip()] = True

print(", ".join(sorted(authors.keys())))
