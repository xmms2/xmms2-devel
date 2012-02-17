#!/usr/bin/python
import sys

sys.path.append('../waftools')
from genipc_server import build

build('ipc_manager', 'xmms_ipc_manager_t *')
