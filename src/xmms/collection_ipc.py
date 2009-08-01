#!/usr/bin/python
import sys

sys.path.append('../waftools')
from genipc_server import build

build('collection', 'xmms_coll_dag_t *')
