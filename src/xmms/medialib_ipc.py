#!/usr/bin/python
import sys

sys.path.append('../waftools')
from genipc_server import build

build('medialib', 'xmms_medialib_t *')
