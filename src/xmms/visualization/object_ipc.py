#!/usr/bin/python
import sys

sys.path.append('../waftools')
from genipc_server import build

build('visualization', 'xmms_visualization_t *')
