#!/usr/bin/env python
from subprocess import check_output, call
import tarfile
import os

def get_template(ball, path):
    template = {}

    tfile = tarfile.open(ball, "r")

    reference = tfile.getmember(path)
    for attr in ('uid', 'gid', 'uname', 'gname', 'mtime'):
        template[attr] = getattr(reference, attr)

    tfile.close()

    return template

def add_files(ball, prefix, template, files):
    tfile = tarfile.open(ball, "a")

    for name, content in files:
        path = os.path.join(os.path.dirname(ball), name)
        if os.path.exists(path):
            os.unlink(path)

        fd = open(path, "wb+")
        fd.write(content)
        fd.close()

        tinfo = tfile.gettarinfo(arcname=os.path.join(prefix, name), name=path)
        for key, value in template.items():
            setattr(tinfo, key, value)

        fd = open(path, "rb")
        tfile.addfile(tinfo, fileobj=fd)
        fd.close()

    tfile.close()

VERSION = check_output(["git", "describe"]).decode().strip()
print("Found %s version" % VERSION)

# TODO: derive paths from submodule configuration
TUTORIAL_DIR="doc/tutorial"
S4_DIR="src/lib/s4"

PREFIX="xmms2-%s" % VERSION
PREFIX_TUTORIAL="%s/%s" % (PREFIX, TUTORIAL_DIR)
PREFIX_S4="%s/%s" % (PREFIX, S4_DIR)

DIST_DIR="dist"
DIST_XMMS2="%s/xmms2-%s.tar" % (DIST_DIR, VERSION)
DIST_XMMS2_BZ2="%s/xmms2-%s.tar.bz2" % (DIST_DIR, VERSION)
DIST_TUTORIAL="%s/xmms2-tutorial-%s.tar" % (DIST_DIR, VERSION)
DIST_S4="%s/xmms2-s4-%s.tar" % (DIST_DIR, VERSION)

if not os.path.exists(DIST_DIR):
    os.mkdir(DIST_DIR)

if os.path.exists(DIST_XMMS2):
    os.unlink(DIST_XMMS2)

if os.path.exists(DIST_XMMS2_BZ2):
    os.unlink(DIST_XMMS2_BZ2)

if os.path.exists(DIST_TUTORIAL):
    os.unlink(DIST_TUTORIAL)

if os.path.exists(DIST_S4):
    os.unlink(DIST_S4)

print("Create %s" % DIST_XMMS2)
call("git archive --format=tar --prefix=%s/ HEAD > %s" % (PREFIX, DIST_XMMS2), shell=True)

call("git submodule init", shell=True)
call("git submodule update", shell=True)
print("Create %s" % DIST_TUTORIAL)
call("git --git-dir=%s/.git archive --format=tar --prefix=%s/ HEAD > %s" % (TUTORIAL_DIR, PREFIX_TUTORIAL, DIST_TUTORIAL), shell=True)
print("Create %s" % DIST_S4)
call("git --git-dir=%s/.git archive --format=tar --prefix=%s/ HEAD > %s" % (S4_DIR, PREFIX_S4, DIST_S4), shell=True)

print("Append %s to %s" % (DIST_TUTORIAL, DIST_XMMS2))
call("tar -Af %s %s" % (DIST_XMMS2, DIST_TUTORIAL), shell=True)
print("Append %s to %s" % (DIST_S4, DIST_XMMS2))
call("tar -Af %s %s" % (DIST_XMMS2, DIST_S4), shell=True)

print("Append ChangeLong and hashed to %s" % DIST_XMMS2)
add_files(DIST_XMMS2, PREFIX, get_template(DIST_XMMS2, os.path.join(PREFIX, "wscript")), [
        ("xmms2-%s.ChangeLog" % VERSION, check_output("utils/gen-changelog.py")),
        ("checksums", check_output("utils/gen-tree-hashes.py"))
])

print("Compress %s" % DIST_XMMS2)
call("xz %s" % DIST_XMMS2, shell=True)
