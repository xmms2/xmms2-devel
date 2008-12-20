#ifndef XMMSC_STDBOOL_H
#define XMMSC_STDBOOL_H

#ifdef _MSC_VER
#define __bool_true_false_are_defined 1
typedef int bool;
#define true 1
#define false 0

#else
#ifndef __cplusplus
#include <stdbool.h>
#endif

#endif

#endif
