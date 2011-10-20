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

        fd = file(path, "w+")
        fd.write(content)
        fd.close()

        tinfo = tfile.gettarinfo(arcname=os.path.join(prefix, name), name=path)
        for key, value in template.items():
            setattr(tinfo, key, value)

        fd = file(path)
        tfile.addfile(tinfo, fileobj=fd)
        fd.close()

    tfile.close()

VERSION = check_output(["git", "describe"]).strip()

TUTORIAL_DIR="doc/tutorial"

PREFIX="xmms2-%s" % VERSION
PREFIX_TUTORIAL="%s/%s" % (PREFIX, TUTORIAL_DIR)

DIST_DIR="dist"
DIST_XMMS2="%s/xmms2-%s.tar" % (DIST_DIR, VERSION)
DIST_XMMS2_BZ2="%s/xmms2-%s.tar.bz2" % (DIST_DIR, VERSION)
DIST_TUTORIAL="%s/xmms2-tutorial-%s.tar" % (DIST_DIR, VERSION)

if not os.path.exists(DIST_DIR):
    os.mkdir(DIST_DIR)

if os.path.exists(DIST_XMMS2):
    os.unlink(DIST_XMMS2)

if os.path.exists(DIST_XMMS2_BZ2):
    os.unlink(DIST_XMMS2_BZ2)

if os.path.exists(DIST_TUTORIAL):
    os.unlink(DIST_TUTORIAL)

# Tar up XMMS2
call("git archive --format=tar --prefix=%s/ HEAD > %s" % (PREFIX, DIST_XMMS2), shell=True)

# Checkout and tar up the tutorials
call("git submodule init", shell=True)
call("git submodule update", shell=True)
call("git --git-dir=%s/.git archive --format=tar --prefix=%s/ HEAD > %s" % (TUTORIAL_DIR, PREFIX_TUTORIAL, DIST_TUTORIAL), shell=True)

# Append the tutorials to the XMMS2 archive
call("tar -Af %s %s" % (DIST_XMMS2, DIST_TUTORIAL), shell=True)

# Append ChangeLog and a summary of all file hashes."
add_files(DIST_XMMS2, PREFIX, get_template(DIST_XMMS2, os.path.join(PREFIX, "wscript")), [
        ("xmms2-%s.ChangeLog" % VERSION, check_output("utils/gen-changelog.py")),
        ("checksums", check_output("utils/gen-tree-hashes.py"))
])

call("bzip2 %s" % DIST_XMMS2, shell=True)
