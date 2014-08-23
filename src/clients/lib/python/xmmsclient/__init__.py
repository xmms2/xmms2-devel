##

# Do some magic in order to make xmmsapi to see xmmsvalue
import sys, os
# XXX Do not prepend the directory. The threading import symbols from a 'collections'
# module that is not ours !!!
sys.path.append(os.path.dirname(__file__))

import xmmsapi, xmmsvalue
from xmmsapi import Xmms, XmmsLoop, XmmsResult, userconfdir_get
from xmmsvalue import XmmsValue
from xmmsvalue import coll_parse
from sync import XmmsSync, XmmsError
from propdict import PropDict
from consts import *

# Compatibility (deprecated)
XMMS = XmmsLoop
XMMSResult = XmmsResult
XMMSValue = XmmsValue
XMMSSync = XmmsSync
XMMSError = XmmsError

# Now, it is safe to remove the directory from the path.
sys.path.remove(os.path.dirname(__file__))
del sys, os
