/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2011 XMMS2 Team
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

#include <glib.h>
#include <stdlib.h>
#include <string.h>

#include "xmmspriv/xmms_log.h"
#include "xmmspriv/xmms_ipc.h"
#include "xmmspriv/xmms_config.h"
#include "xmmspriv/xmms_medialib.h"

#include "utils/jsonism.h"
#include "utils/value_utils.h"
#include "utils/coll_utils.h"

typedef void (*xmms_path_predicate)(const gchar *filename, xmmsv_t *list);

typedef void (*xmms_test_predicate)(xmms_medialib_t *medialib, const gchar *name,
                                    xmmsv_t *content, xmmsv_coll_t *coll,
                                    xmmsv_t *specification, xmmsv_t *expected,
                                    gint format, const gchar *datasetname);

typedef struct xmms_test_args_St {
	enum {
		PERFORMANCE,
		UNITTEST
	} variant;
	enum {
		FORMAT_PRETTY,
		FORMAT_CSV
	} format;
	const gchar *database_path;
	const gchar *testcase_path;
	gboolean debug;
} xmms_test_args_t;

static void
simple_log_handler (const gchar *log_domain, GLogLevelFlags log_level,
                    const gchar *message, gpointer user_data)
{
	xmms_test_args_t *args = (xmms_test_args_t *) user_data;

	if (log_level & G_LOG_FLAG_RECURSION) {
		exit (1);
	}

	if (args->debug && (log_level & G_LOG_LEVEL_DEBUG)) {
		g_print ("DEBUG: %s\n", message);
	} else if (log_level & ~G_LOG_LEVEL_DEBUG) {
		if (log_level & (G_LOG_LEVEL_DEBUG | G_LOG_LEVEL_INFO |
		                 G_LOG_LEVEL_MESSAGE)) {
			g_print ("%s\n", message);
		} else {
			g_printerr ("xmms2-launcher: %s\n", message);
		}
	}


	if (log_level & G_LOG_FLAG_FATAL) {
		exit (EXIT_FAILURE);
	}
}

static xmmsv_t *
scan_path (const gchar *path, xmms_path_predicate predicate)
{
	const gchar *filename;
	xmmsv_t *list;
	GDir *dir;

	g_debug ("Scanning path: %s", path);

	list = xmmsv_new_list ();

	if (g_file_test (path, G_FILE_TEST_IS_REGULAR)) {
		predicate (path, list);
		return list;
	}

	dir = g_dir_open (path, 0, NULL);
	if (dir == NULL) {
		g_print ("Could not open directory: %s\n", path);
		exit (EXIT_FAILURE);
	}

	while ((filename = g_dir_read_name (dir)) != NULL) {
		gchar *filepath;

		filepath = g_build_filename (path, filename, NULL);

		g_debug ("Found file: %s", filepath);
		predicate (filepath, list);

		g_free (filepath);
	}

	g_dir_close (dir);

	return list;
}

static void
filter_databases (const gchar *path, xmmsv_t *list)
{
	if (g_str_has_suffix (path, ".s4"))
		xmmsv_list_append_string (list, path);
}


static void
filter_testcase (const gchar *path, xmmsv_t *list)
{
	gchar *content, *filename;
	xmmsv_t *dict, *data, *holder;
	xmmsv_coll_t *coll;

	g_assert (g_file_get_contents (path, &content, NULL, NULL));
	dict = xmmsv_from_json (content);
	g_free (content);

	g_assert (xmmsv_dict_has_key (dict, "medialib"));
	g_assert (xmmsv_dict_has_key (dict, "collection"));
	g_assert (xmmsv_dict_has_key (dict, "specification"));
	g_assert (xmmsv_dict_has_key (dict, "expected"));

	g_assert (xmmsv_dict_get (dict, "collection", &data));
	g_assert (xmmsv_is_type (data, XMMSV_TYPE_DICT));

	coll = xmmsv_coll_from_dict (data);
	holder = xmmsv_new_coll (coll);
	xmmsv_coll_unref (coll);

	xmmsv_dict_set (dict, "collection", holder);
	xmmsv_unref (holder);

	filename = g_path_get_basename (path);
	xmmsv_dict_set_string (dict, "name", filename);
	g_free (filename);

	xmmsv_list_append (list, dict);
	xmmsv_unref (dict);
}


/**
 * TODO: Should check for '/' in the key, and set source if found.
 */
static void
populate_medialib (xmms_medialib_t *medialib, xmmsv_t *content)
{
	xmms_medialib_session_t *session;
	xmms_medialib_entry_t entry = 0;
	xmmsv_list_iter_t *lit;

	session = xmms_medialib_session_begin (medialib);

	xmmsv_get_list_iter (content, &lit);
	while (xmmsv_list_iter_valid (lit)) {
		xmmsv_dict_iter_t *dit;
		xmms_error_t err;
		xmmsv_t *dict;

		xmms_error_reset (&err);

		xmmsv_list_iter_entry (lit, &dict);

		if (xmmsv_dict_has_key (dict, "url")) {
			const gchar *url;
			xmmsv_dict_entry_get_string (dict, "url", &url);
			entry = xmms_medialib_entry_new (session, url, &err);
		} else {
			gchar *url;
			url = g_strdup_printf ("file://%d.mp3", entry + 1);
			entry = xmms_medialib_entry_new (session, url, &err);
			g_free (url);
		}

		xmmsv_get_dict_iter (dict, &dit);

		while (xmmsv_dict_iter_valid (dit)) {
			const gchar *key;
			xmmsv_t *container;

			xmmsv_dict_iter_pair (dit, &key, &container);
			if (xmmsv_is_type (container, XMMSV_TYPE_STRING)) {
				const gchar *value;
				xmmsv_get_string (container, &value);
				xmms_medialib_entry_property_set_str (session, entry, key, value);
			} else {
				gint32 value;
				xmmsv_get_int (container, &value);
				xmms_medialib_entry_property_set_int (session, entry, key, value);
			}

			xmmsv_dict_iter_next (dit);
		}

		xmmsv_list_iter_next (lit);
	}

	xmms_medialib_session_commit (session);
}


/**
 * Unit test predicate
 */
static void
run_unit_test (xmms_medialib_t *mlib, const gchar *name, xmmsv_t *content,
               xmmsv_coll_t *coll, xmmsv_t *specification, xmmsv_t *expected,
               gint format, const gchar *datasetname)
{
	gboolean matches, ordered = FALSE;
	xmmsv_t *ret, *value;
	xmms_error_t err;
	xmms_medialib_t *medialib;
	xmms_medialib_session_t *session;

	g_debug ("Running test: %s", name);

	xmms_ipc_init ();
	xmms_config_init ("memory://");
	xmms_config_property_register ("medialib.path", "memory://", NULL, NULL);

	medialib = xmms_medialib_init ();

	populate_medialib (medialib, content);

	session = xmms_medialib_session_begin (medialib);
	ret = xmms_medialib_query (session, coll, specification, &err);
	xmms_medialib_session_commit (session);

	xmmsv_dict_get (expected, "result", &value);
	xmmsv_dict_entry_get_int (expected, "ordered", &ordered);

	if (ordered) {
		matches = xmmsv_compare (ret, value);
	} else {
		matches = xmmsv_compare_unordered (ret, value);
	}

	if (matches) {
		if (format == FORMAT_CSV) {
			g_print ("\"%s\", 1\n", name);
		} else {
			g_print ("............................................................ Success!");
			g_print ("\r%s \n", name);
		}

	} else {
		if (format == FORMAT_CSV) {
			g_print ("\"%s\", 0\n", name);
		} else {
			g_print ("............................................................ Failure!");
			g_print ("\r%s \n", name);
		}

		g_printerr ("The result: ");
		xmmsv_dump (ret);
		g_printerr ("Does not equal: ");
		xmmsv_dump (value);
	}

	xmmsv_unref (ret);

	xmms_object_unref (medialib);
	xmms_config_shutdown ();
	xmms_ipc_shutdown ();
}


/**
 * Performance test predicate
 */
static void
run_performance_test (xmms_medialib_t *medialib, const gchar *name, xmmsv_t *content,
                      xmmsv_coll_t *coll, xmmsv_t *specification, xmmsv_t *expected,
                      gint format, const gchar *datasetname)
{
	xmms_medialib_session_t *session;
	xmms_error_t err;
	GTimeVal t0, t1;
	guint64 duration;
	xmmsv_t *ret;

	session = xmms_medialib_session_begin (medialib);

	g_get_current_time (&t0);
	ret = xmms_medialib_query (session, coll, specification, &err);
	g_get_current_time (&t1);

	xmms_medialib_session_commit (session);

	duration = (guint64)((t1.tv_sec - t0.tv_sec) * G_USEC_PER_SEC) + (t1.tv_usec - t0.tv_usec);

	if (format == FORMAT_PRETTY)
		g_print ("* Test %s\n", name);

	if (xmms_error_iserror (&err)) {
		if (format == FORMAT_CSV) {
			g_print ("\"%s\",\"%s\",0,0\n", datasetname, name);
		} else {
			g_print ("   - Query failed: %s\n", xmms_error_message_get (&err));
		}
	} else {
		if (format == FORMAT_CSV) {
			g_print ("\"%s\",\"%s\",1,%" G_GUINT64_FORMAT "\n", datasetname, name, duration);
		} else {
			g_print ("   - Time elapsed: %.3fms\n", duration / 1000.0);
		}
	}

	xmmsv_unref (ret);
}


static void
run_tests (xmms_medialib_t *medialib, xmmsv_t *testcases, xmms_test_predicate predicate,
           gint format, const gchar *datasetname)
{
	xmmsv_list_iter_t *it;

	xmmsv_get_list_iter (testcases, &it);
	while (xmmsv_list_iter_valid (it)) {
		xmmsv_t *dict, *content, *specification, *holder, *expected;
		xmmsv_coll_t *coll;
		const gchar *name;
		dict = NULL;

		g_assert (xmmsv_list_iter_entry (it, &dict));

		xmmsv_dict_entry_get_string (dict, "name", &name);

		xmmsv_dict_get (dict, "medialib", &content);
		xmmsv_dict_get (dict, "specification", &specification);
		xmmsv_dict_get (dict, "collection", &holder);
		xmmsv_dict_get (dict, "expected", &expected);

		xmmsv_get_coll (holder, &coll);

		predicate (medialib, name, content, coll, specification,
		           expected, format, datasetname);

		xmmsv_list_iter_next (it);
	}
}


static void
run_performance_tests (xmmsv_t *databases, xmmsv_t *testcases, gint format)
{
	xmmsv_list_iter_t *it;

	xmmsv_get_list_iter (databases, &it);
	while (xmmsv_list_iter_valid (it)) {
		xmms_medialib_t *medialib;
		const gchar *filename;

		xmmsv_list_iter_entry_string (it, &filename);

		if (format == FORMAT_PRETTY)
			g_print ("Running suite with: %s\n", filename);

		xmms_ipc_init ();
		xmms_config_init ("memory://");
		xmms_config_property_register ("medialib.path", filename, NULL, NULL);

		medialib = xmms_medialib_init ();
		if (medialib == NULL) {
			g_print ("Could not open database: %s (%d)\n", filename, s4_errno ());
			exit (EXIT_FAILURE);
		}

		run_tests (medialib, testcases, run_performance_test, format, filename);

		xmms_object_unref (medialib);
		xmms_config_shutdown ();
		xmms_ipc_shutdown ();

		xmmsv_list_iter_next (it);
	}
}


static void
parse_command_line (gint argc, gchar **argv, xmms_test_args_t *args)
{
	GOptionContext *context;
	const gchar *variant = "unittest";
	const gchar *format = "pretty";
	GError *error = NULL;

	args->database_path = "tests/server/databases";
	args->testcase_path = "tests/server/medialib";

	const GOptionEntry options[] = {
		{
			"variant", 'v', 0,
			G_OPTION_ARG_STRING, &variant,
			"'performance' or 'unittest' (default).", "<variant>"
		},
		{
			"format", 'f', 0,
			G_OPTION_ARG_STRING, &format,
			"'csv' or 'pretty' (default).", "<format>"
		},
		{
			"medialib-path", 'm', 0,
			G_OPTION_ARG_FILENAME, &args->database_path,
			"Scan <path> for 1..n media libraries.", "<path>"
		},
		{
			"testcase-path", 't', 0,
			G_OPTION_ARG_FILENAME, &args->testcase_path,
			"Scan <path> for 1..n test cases.", "<path>"
		},
		{
			"debug", 'd', 0,
			G_OPTION_ARG_NONE, &args->debug,
			"Enable debug logging.", NULL
		},
		{
			NULL
		}
	};

	context = g_option_context_new ("- Medialib Test Runner");
	g_option_context_add_main_entries (context, options, NULL);

	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		gchar *helptext = g_option_context_get_help (context, TRUE, NULL);
		g_print ("Option parsing failed: %s\n%s", error->message, helptext);
		g_free (helptext);
		exit (EXIT_FAILURE);
	}

	if (strcmp (variant, "performance") == 0) {
		args->variant = PERFORMANCE;
	} else {
		args->variant = UNITTEST;
	}

	if (strcmp (format, "pretty") == 0) {
		args->format = FORMAT_PRETTY;
	} else {
		args->format = FORMAT_CSV;
	}

	g_option_context_free (context);
}


/**
 * Media Library Test Runner
 * - load a number of tests from json files
 * - by default, run tests as unit tests
 * - optionally run tests as performance tests, but then require a db directory
 */
gint
main (gint argc, gchar **argv)
{
	xmmsv_t *testcases, *databases;
	xmms_test_args_t args = { 0 };

	g_thread_init (0);

	xmms_log_init (0);

	parse_command_line (argc, argv, &args);

	g_log_set_default_handler (simple_log_handler, (gpointer) &args);

	g_debug ("Test variant: %s", args.variant == UNITTEST ? "unit test" : "performance test");
	g_debug ("Output format: %s", args.format == FORMAT_PRETTY ? "pretty" : "csv");
	g_debug ("Database path: %s", args.database_path);
	g_debug ("Testcase path: %s", args.testcase_path);

	testcases = scan_path (args.testcase_path, filter_testcase);

	if (args.variant == PERFORMANCE) {
		if (args.format == FORMAT_CSV)
			g_print ("\"dataset\",\"test\",\"success\",\"duration\"\n");
		else
			g_print (" - Running Performance Test -\n");

		databases = scan_path (args.database_path, filter_databases);
		run_performance_tests (databases, testcases, args.format);
		xmmsv_unref (databases);
	} else {
		if (args.format == FORMAT_CSV)
			g_print ("\"test\",\"success\"\n");
		else
			g_print (" - Running Unit Test -\n");
		run_tests (NULL, testcases, run_unit_test, args.format, NULL);
	}

	xmmsv_unref (testcases);

	return EXIT_SUCCESS;
}
