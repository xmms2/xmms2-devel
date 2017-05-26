#!/usr/bin/env python
from subprocess import check_output
from operator import itemgetter
import os

def collect_hashes(*paths):
    hashes = []
    for path in paths:
        data = check_output("git --git-dir=%s/.git ls-tree -r HEAD" % path, shell=True)
        for line in data.split("\n"):
            line = line.strip()
            if line:
                perms, typ, chash, fpath = line.split()
                hashes.append((perms, typ, chash, os.path.normpath(os.path.join(path, fpath))))

    result = []
    for (perms, typ, chash, path) in sorted(hashes, key=itemgetter(3)):
        result.append("%s %s %s\t %s" % (perms, typ, chash, path))

    return "\n".join(result)

print check_output("git describe", shell=True)
print collect_hashes(".", "doc/tutorial")
print collect_hashes(".", "src/lib/s4")
