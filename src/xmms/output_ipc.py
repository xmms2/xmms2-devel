#!/usr/bin/python
import sys

sys.path.append('../waftools')
from genipc_server import build

build('playback', 'xmms_output_t *')
