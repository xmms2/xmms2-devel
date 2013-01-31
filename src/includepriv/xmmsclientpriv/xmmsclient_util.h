#ifndef __XUTILS_H__
#define __XUTILS_H__

#include <stdio.h>
#include "xmmsc/xmmsc_compiler.h"
#include "xmmsc/xmmsc_util.h"
#include "xmmscpriv/xmmsc_util.h"

#define x_check_conn(c, retval) do { x_api_error_if (!c, "with a NULL connection", retval); x_api_error_if (!c->ipc, "with a connection that isn't connected", retval);} while (0)

#endif /* __XUTILS_H__ */
