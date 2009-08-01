#!/usr/bin/python
import sys

sys.path.append('../waftools')
from genipc_server import build

build('bindata', 'xmms_bindata_t *')
