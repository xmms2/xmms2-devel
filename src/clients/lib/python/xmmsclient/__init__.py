##

# Do some magic in order to make xmmsapi to see xmmsvalue
import sys, os
# XXX Do not prepend the directory. The threading import symbols from a 'collections'
# module that is not ours !!!
sys.path.append(os.path.dirname(__file__))

import xmmsapi, xmmsvalue
from xmmsapi import XMMS, XMMSResult, userconfdir_get
from xmmsvalue import XMMSValue
from xmmsvalue import coll_parse
from sync import XMMSSync, XMMSError
from propdict import PropDict
from consts import *

# Now, it is safe to remove the directory from the path.
sys.path.remove(os.path.dirname(__file__))
del sys, os
