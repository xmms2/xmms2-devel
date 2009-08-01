#!/usr/bin/python
import sys

sys.path.append('../waftools')
from genipc_server import build

build('mediainfo_reader', 'xmms_mediainfo_t *')
