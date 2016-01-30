/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2016 XMMS2 Team
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */

#ifndef __XCU_H
#define __XCU_H

#include "CUnit/Basic.h"


#define ST_NE(x) #x
#define ST(x) ST_NE(x)

int xcu_pre_case (const char *name);
void xcu_post_case (const char *name);


#define CASE(name)							\
	static void __testcase_##name (void);				\
	void __testcase_wrapper_##name (void);			\
	void __testcase_wrapper_##name (void) {			\
		if (xcu_pre_case (ST (name))) {				\
			__testsuite_setup ();					\
			__testcase_##name ();					\
			__testsuite_cleanup ();					\
			xcu_post_case (ST (name));				\
		}							\
	}								\
	static void __testcase_##name (void)

#define SETUP(name)							\
	static int __testsuite_setup (void);				\
	static int __testsuite_cleanup (void);				\
	static int __testsuite_setup (void)

#define CLEANUP(name) \
	static int __testsuite_cleanup (void)

#endif
