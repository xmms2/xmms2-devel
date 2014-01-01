/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2014 XMMS2 Team
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 */

#ifndef __XMMS_CALL_H__
#define __XMMS_CALL_H__

#include <xmmsclient/xmmsclient.h>

#include <glib.h>
#include <glib/gprintf.h>

/* Perform a simple IPC call synchronously, report error and unref */
#define XMMS_CALL(fun, ...) do { \
		xmmsc_result_t *__result; \
		const gchar *__message; \
		xmmsv_t *__value; \
		__result = fun (__VA_ARGS__); \
		xmmsc_result_wait (__result); \
		__value = xmmsc_result_get_value (__result); \
		if (xmmsv_get_error (__value, &__message)) { \
			g_printf (_("Server error: %s\n"), __message); \
		} \
		xmmsc_result_unref (__result); \
	} while (0)

/* XMMS_CALL_CHAIN predicate */
#define XMMS_CALL_P(func, ...) do { \
		xmmsc_result_t *__result; \
		const gchar *__message; \
		xmmsv_t *__value; \
		__result = func (__VA_ARGS__); \
		xmmsc_result_wait (__result); \
		if (XMMS_PREV_VALUE != NULL) \
			xmmsv_unref (XMMS_PREV_VALUE); \
		XMMS_PREV_VALUE = NULL; \
		__value = xmmsc_result_get_value (__result); \
		if (xmmsv_get_error (__value, &__message)) { \
			g_printf (_("Server error: %s\n"), __message); \
		} else { \
			XMMS_PREV_VALUE = xmmsv_ref (__value); \
		} \
		xmmsc_result_unref (__result); \
	} while(0)

/* XMMS_CALL_CHAIN predicate */
#define FUNC_CALL_P(func, ...) do { \
		if (XMMS_PREV_VALUE != NULL) { func (__VA_ARGS__); } \
	} while (0)

#define XMMS_N_ARGS(...) XMMS_N_ARGS_IMPL(__VA_ARGS__, 5,4,3,2,1)
#define XMMS_N_ARGS_IMPL(_1,_2,_3,_4,_5,N,...) N

#define XMMS_MACRO_CHOOSER(predicate, ...) XMMS_MACRO_CHOOSER_(predicate, XMMS_N_ARGS(__VA_ARGS__))
#define XMMS_MACRO_CHOOSER_(predicate, nargs) XMMS_MACRO_CHOOSER__(predicate, nargs)
#define XMMS_MACRO_CHOOSER__(predicate, nargs) predicate ## nargs

#define XMMS_UNREF_VALUES(first, ...) do { \
		xmmsv_t *__values[] = { first, ##__VA_ARGS__, NULL }; \
		gint __i = 0; \
		for (__i = 0; __i < G_N_ELEMENTS (__values); __i++) { \
			if (__values[__i] != NULL) \
			    xmmsv_unref (__values[__i]); \
		} \
} while (0)

#define XMMS_CALL_CHAIN(predicate, ...) XMMS_MACRO_CHOOSER(XMMS_CALL_CHAIN, predicate, __VA_ARGS__)(predicate, __VA_ARGS__)

#define XMMS_CALL_CHAIN2(p1,p2) do { \
		xmmsv_t *XMMS_PREV_VALUE, *XMMS_FIRST_VALUE; \
		XMMS_PREV_VALUE = XMMS_FIRST_VALUE = NULL; \
		p1; \
		if (XMMS_PREV_VALUE != NULL) { XMMS_FIRST_VALUE = xmmsv_ref (XMMS_PREV_VALUE); p2; } \
		XMMS_UNREF_VALUES (XMMS_PREV_VALUE, XMMS_FIRST_VALUE); \
	} while (0)

#define XMMS_CALL_CHAIN3(p1,p2,p3) do { \
		xmmsv_t *XMMS_PREV_VALUE, *XMMS_FIRST_VALUE, *XMMS_SECOND_VALUE; \
		XMMS_PREV_VALUE = XMMS_FIRST_VALUE = XMMS_SECOND_VALUE = NULL; \
		p1; \
		if (XMMS_PREV_VALUE != NULL) { XMMS_FIRST_VALUE = xmmsv_ref (XMMS_PREV_VALUE); p2; } \
		if (XMMS_PREV_VALUE != NULL) { XMMS_SECOND_VALUE = xmmsv_ref (XMMS_PREV_VALUE); p3; } \
		XMMS_UNREF_VALUES (XMMS_PREV_VALUE, XMMS_FIRST_VALUE, XMMS_SECOND_VALUE); \
	} while (0)

#define XMMS_CALL_CHAIN4(p1,p2,p3,p4) do { \
		xmmsv_t *XMMS_PREV_VALUE, *XMMS_FIRST_VALUE, *XMMS_SECOND_VALUE, *XMMS_THIRD_VALUE; \
		XMMS_PREV_VALUE = XMMS_FIRST_VALUE = XMMS_SECOND_VALUE = XMMS_THIRD_VALUE = NULL; \
		p1; \
		if (XMMS_PREV_VALUE != NULL) { XMMS_FIRST_VALUE = xmmsv_ref (XMMS_PREV_VALUE); p2; } \
		if (XMMS_PREV_VALUE != NULL) { XMMS_SECOND_VALUE = xmmsv_ref (XMMS_PREV_VALUE); p3; } \
		if (XMMS_PREV_VALUE != NULL) { XMMS_THIRD_VALUE = xmmsv_ref (XMMS_PREV_VALUE); p4; } \
		XMMS_UNREF_VALUES (XMMS_PREV_VALUE, XMMS_FIRST_VALUE, XMMS_SECOND_VALUE, XMMS_THIRD_VALUE); \
	} while (0)

#define XMMS_CALL_CHAIN5(p1,p2,p3,p4,p5) do { \
		xmmsv_t *XMMS_PREV_VALUE, *XMMS_FIRST_VALUE, *XMMS_SECOND_VALUE, *XMMS_THIRD_VALUE, *XMMS_FOURTH_VALUE; \
		XMMS_PREV_VALUE = XMMS_FIRST_VALUE = XMMS_SECOND_VALUE = XMMS_THIRD_VALUE = XMMS_FOURTH_VALUE = NULL; \
		p1; \
		if (XMMS_PREV_VALUE != NULL) { XMMS_FIRST_VALUE = xmmsv_ref (XMMS_PREV_VALUE); p2; } \
		if (XMMS_PREV_VALUE != NULL) { XMMS_SECOND_VALUE = xmmsv_ref (XMMS_PREV_VALUE); p3; } \
		if (XMMS_PREV_VALUE != NULL) { XMMS_THIRD_VALUE = xmmsv_ref (XMMS_PREV_VALUE); p4; } \
		if (XMMS_PREV_VALUE != NULL) { XMMS_FOURTH_VALUE = xmmsv_ref (XMMS_PREV_VALUE); p5; } \
		XMMS_UNREF_VALUES (XMMS_PREV_VALUE, XMMS_FIRST_VALUE, XMMS_SECOND_VALUE, XMMS_THIRD_VALUE, XMMS_FOURTH_VALUE); \
	} while (0)

#endif
