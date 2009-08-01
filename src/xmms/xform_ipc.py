#!/usr/bin/python
import sys

sys.path.append('../waftools')
from genipc_server import build

build('xform', 'xmms_xform_object_t *')
