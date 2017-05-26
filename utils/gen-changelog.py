#!/usr/bin/python
from subprocess import check_output
import os

authorchanges = {}
prevrelease = "HEAD"
prevdate = "Not released"
prevtreehash = "HEAD"

for line in check_output("git log --pretty=format:'%t\t%an\t%ai\t%s'", shell=True).decode('utf-8', "replace").split("\n"):
    line = line.strip()
    if not line:
        continue

    parts = line.strip().split("\t", 3)
    if len(parts) == 4:
        treehash, author, date, subject = parts
    else:
        treehash, author, date, subject = parts + ["No Subject"]

    if subject.lower().startswith("merge"):
        continue

    if subject.lower().startswith("release"):
        subject = subject[9:]
        if len(authorchanges) == 0:
            prevrelease = subject
            prevtreehash = treehash
            prevdate = date.split()[0]
            continue
        print("Changes between %s and %s" % (subject, prevrelease))
        print
        print(" Release date: %s" % prevdate)
        print(" Authors contributing to this release: %d" % len(authorchanges))
        print(" Number of changesets: %d" % sum(map(len, authorchanges.values())))
        print(" Number of files in this release: %s" % check_output("git ls-tree -r %s | wc -l" % prevtreehash, shell=True).decode().strip())
        print
        authors = list(authorchanges.keys())
        authors.sort()
        for a in authors:
            print(" %s:" % a.encode("utf-8"))
            changes = authorchanges[a]
            changes.sort()
            for c in changes:
                print("  * %s" % c.encode("utf-8"))
            print
        print
        print
        authorchanges={}
        prevrelease = subject
        prevtreehash = treehash
        prevdate = date.split()[0]
        continue

    if author not in authorchanges:
        authorchanges[author] = []

    authorchanges[author].append(subject)
