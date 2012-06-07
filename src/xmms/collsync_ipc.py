#!/usr/bin/python
import sys

sys.path.append('../waftools')
from genipc_server import build

build('coll_sync', 'xmms_coll_sync_t *')
