#!/usr/bin/env python
# -*- coding: utf-8 -*-

import subprocess
import sys
import re
from collections import defaultdict

if len(sys.argv) != 2 and len(sys.argv) != 3:
    print "usage: %s <from> [<to>]" % sys.argv[0]
    raise SystemExit(1)

rev_start = sys.argv[1]

if len(sys.argv) > 2:
    rev_end = sys.argv[2]
else:
    rev_end = "HEAD"

command = ["git", "log", "--pretty=%H☃%an☃%s", rev_start + "..." + rev_end]

commits = defaultdict(lambda : defaultdict(list))
for line in subprocess.check_output(command).split("\n"):
    if not line:
        continue
    commit, author, comment = line.split("☃")
    if not (comment.startswith("BUG") or comment.startswith("FEATURE")):
        continue
    match = re.findall("(FEATURE|BUG)\(([0-9]+)\).+", comment)
    if len(match) != 1 and len(match[0]) != 2:
        print "skipping", match

    _, bug_id = match[0]

    commits[author][int(bug_id)].append(commit)

for author in sorted(commits.keys()):
    print "'''%s'''" % author
    for bugid in sorted(commits[author].keys()):
        print "* <mantis>%s</mantis>" % bugid
        for commit in commits[author][bugid]:
            print "** <git norepo=1 repo='xmms2-devel.git'>%s</git>" % commit
