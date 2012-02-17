#!/usr/bin/python
import sys

sys.path.append('../waftools')
from genipc_server import build

build('courier', 'xmms_courier_t *')
