#ifndef XMMSC_SNPRINTF_H
#define XMMSC_SNPRINTF_H

#ifdef _MSC_VER
#define snprintf _snprintf
#define vsnprintf _vsnprintf
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#else
#include <strings.h>
#endif


#endif
