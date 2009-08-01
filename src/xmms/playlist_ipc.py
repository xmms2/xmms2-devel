#!/usr/bin/python
import sys

sys.path.append('../waftools')
from genipc_server import build

build('playlist', 'xmms_playlist_t *')
