#!/usr/bin/python3.4
import os
import sys
import re
import hashlib
import gzip
import pprint
import math

dir_pattern = re.compile("^(?!(\.conf_check|c4che))")
file_pattern = re.compile("^(?!(\.(wafpickle|lock-waf).*|.+\.(log|a|o)$))")

def open_any(path):
    if path.endswith(".gz"):
        return gzip.open(path, "rb")
    return open(path, "rb")

def analyze(payload):
    return (hashlib.sha1(payload).hexdigest(), len(payload))

def checksum_root(root):
    root = root.rstrip("/")
    checksums = {}
    for path, directories, files in os.walk(root, topdown=True):
        if path == root:
            directories[:] = filter(dir_pattern.match, directories)
        for name in filter(file_pattern.match, files):
            realpath = os.path.join(path, name)
            if os.path.islink(realpath):
                continue
            with open_any(realpath) as fd:
                checksums[realpath[len(root) + 1:]] = analyze(fd.read())
    return checksums

def compare(root_a, root_b, verbose=False):
    a = checksum_root(root_a)
    b = checksum_root(root_b)

    max_length = len(max(a.keys(), key=len))
    name_fmt = "%%(entry)-%ds" % max_length
    union = set(a.keys()) | set(b.keys())
    diffs = len(union)
    for name in sorted(union):
        checksum_a, size_a = a.get(name, (None, 0))
        checksum_b, size_b = b.get(name, (None, 0))

        args = {
            "entry": name, "root_a": root_a, "root_b": root_b,
            "sign": "=><"[(size_a > size_b) - (size_a < size_b)],
            "delta": math.fabs(size_a - size_b)
        }

        if checksum_a == checksum_b:
            if verbose:
                print((name_fmt + " success") % args)
            diffs -= 1
        elif not checksum_a:
            print((name_fmt + " missing in %(root_a)s") % args)
        elif not checksum_b:
            print((name_fmt + " missing in %(root_b)s") % args)
        else:
            print((name_fmt + " size(%(root_a)s %(sign)s %(root_b)s) %(delta)8d bytes") % args)

    print("Total: %d of %d failed" % (diffs, len(union)))

if __name__ == "__main__":
    compare(sys.argv[-2], sys.argv[-1], "-v" in sys.argv)
