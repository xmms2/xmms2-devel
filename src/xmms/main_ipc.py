#!/usr/bin/python
import sys

sys.path.append('../waftools')
from genipc_server import build

build('main', 'xmms_object_t *')
