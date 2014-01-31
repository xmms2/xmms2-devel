#include "xcu.h"

#include <xmmspriv/xmms_log.h>
#include <xmmspriv/xmms_ipc.h>
#include <xmmspriv/xmms_config.h>
#include <xmmspriv/xmms_medialib.h>

#include "utils/jsonism.h"
#include "utils/value_utils.h"
#include "utils/coll_utils.h"
#include "server-utils/ipc_call.h"
#include "server-utils/mlib_utils.h"

#define CU_ASSERT_LIST_INT_EQUAL(list, pos, expected) do { \
		xmmsv_t *item; \
		gint val; \
		CU_ASSERT_EQUAL (XMMSV_TYPE_LIST, xmmsv_get_type (list)) \
		xmmsv_list_get (list, pos, &item); \
		xmmsv_get_int (item, &val); \
		CU_ASSERT_EQUAL (expected, val); \
	} while (0);

#define CU_ASSERT_LIST_DICT_INT_EQUAL(list, pos, key, expected) do { \
		xmmsv_t *item; \
		gint val; \
		CU_ASSERT_EQUAL (XMMSV_TYPE_LIST, xmmsv_get_type (list)) \
		xmmsv_list_get (list, pos, &item); \
		CU_ASSERT_EQUAL (XMMSV_TYPE_DICT, xmmsv_get_type (item)) \
		xmmsv_dict_entry_get_int (item, key, &val); \
		CU_ASSERT_EQUAL (expected, val); \
	} while (0);

static xmms_medialib_t *medialib;

SETUP (mlib) {
	xmms_ipc_init ();

	xmms_log_init (0);

	xmms_config_init ("memory://");
	xmms_config_property_register ("medialib.path", "memory://", NULL, NULL);

	medialib = xmms_medialib_init ();

	return 0;
}

CLEANUP () {
	xmms_object_unref (medialib); medialib = NULL;
	xmms_config_shutdown ();
	xmms_ipc_shutdown ();

	return 0;
}

static xmmsv_t *
medialib_query (xmmsv_t *coll, xmmsv_t *spec, xmms_error_t *err)
{
	xmms_medialib_session_t *session;
	xmmsv_t *ret;

	session = xmms_medialib_session_begin (medialib);
	ret = xmms_medialib_query (session, coll, spec, err);
	xmms_medialib_session_commit (session);

	return ret;
}

CASE (test_entry_property_get)
{
	xmms_medialib_session_t *session;
	xmms_medialib_entry_t first, second;
	xmmsv_t *result;
	gint tracknr;
	const gchar *title;
	gchar *string;

	first = xmms_mock_entry (medialib, 1, "Red Fang", "Red Fang", "Prehistoric Dog");
	second = xmms_mock_entry (medialib, 4, "Red Fang", "Red Fang", "Humans Remain Human Remains");

	session = xmms_medialib_session_begin (medialib);

	result = xmms_medialib_entry_property_get_value (session, second, "tracknr");
	xmmsv_get_int (result, &tracknr);
	CU_ASSERT_EQUAL (4, tracknr);
	xmmsv_unref (result);

	result = xmms_medialib_entry_property_get_value (session, first, "title");
	xmmsv_get_string (result, &title);
	CU_ASSERT_STRING_EQUAL ("Prehistoric Dog", title);
	xmmsv_unref (result);

	result = xmms_medialib_entry_property_get_value (session, 1337, "tracknr");
	CU_ASSERT_EQUAL (NULL, result);

	string = xmms_medialib_entry_property_get_str (session, first, "title");
	CU_ASSERT_STRING_EQUAL ("Prehistoric Dog", string);
	g_free (string);

	string = xmms_medialib_entry_property_get_str (session, second, "tracknr");
	CU_ASSERT_PTR_NULL (string);
	g_free (string);

	string = xmms_medialib_entry_property_get_str (session, 1337, "monkey");
	CU_ASSERT_EQUAL (NULL, string);

	tracknr = xmms_medialib_entry_property_get_int (session, second, "tracknr");
	CU_ASSERT_EQUAL (4, tracknr);

	tracknr = xmms_medialib_entry_property_get_int (session, 1337, "tracknr");
	CU_ASSERT_EQUAL (-1, tracknr);

	/*
	session = xmms_medialib_session_begin (medialib);
	result = xmms_medialib_entry_property_get_value (1337, "id");
	xmms_medialib_session_commit (session);
	CU_ASSERT_PTR_NULL (result);
	*/

	xmms_medialib_session_abort (session);
}

CASE (test_entry_remove)
{
	xmms_medialib_session_t *session;
	xmms_medialib_entry_t entry;
	xmmsv_t *result;

	entry = xmms_mock_entry (medialib, 1, "Red Fang", "Red Fang", "Prehistoric Dog");

	session = xmms_medialib_session_begin (medialib);
	result = xmms_medialib_entry_property_get_value (session, entry, "tracknr");
	xmms_medialib_session_commit (session);
	CU_ASSERT_PTR_NOT_NULL (result);
	xmmsv_unref (result);

	session = xmms_medialib_session_begin (medialib);
	xmms_medialib_entry_remove (session, entry);
	xmms_medialib_session_commit (session);

	session = xmms_medialib_session_begin (medialib);
	result = xmms_medialib_entry_property_get_value (session, entry, "tracknr");
	xmms_medialib_session_commit (session);
	CU_ASSERT_PTR_NULL (result);
}

CASE (test_entry_cleanup)
{
	xmms_medialib_session_t *session;
	xmms_medialib_entry_t entry;
	xmmsv_t *universe, *spec, *result;
	xmms_error_t err;
	gint count;

	xmms_error_reset (&err);

	entry = xmms_mock_entry (medialib, 1, "Red Fang", "Red Fang", "Prehistoric Dog");

	session = xmms_medialib_session_begin (medialib);
	xmms_medialib_entry_cleanup (session, entry);
	xmms_medialib_session_commit (session);

	session = xmms_medialib_session_begin (medialib);
	result = xmms_medialib_entry_property_get_value (session, entry, "tracknr");
	xmms_medialib_session_commit (session);
	CU_ASSERT_PTR_NULL (result);

	/* Should be cleaned up to check with _get_value once
	 * it actually checks the db when getting value by id.
	 */
	universe = xmmsv_new_coll (XMMS_COLLECTION_TYPE_UNIVERSE);

	spec = xmmsv_from_xson ("{ 'type': 'count' }");
	result = medialib_query (universe, spec, &err);

	xmmsv_get_int (result, &count);

	CU_ASSERT_EQUAL (1, count);

	xmmsv_unref (spec);
	xmmsv_unref (result);
	xmmsv_unref (universe);
}

CASE (test_not_resolved)
{
	xmms_medialib_session_t *session;
	xmms_medialib_entry_t entry, first, second;
	guint count;

	first = xmms_mock_entry (medialib, 1, "Red Fang", "Red Fang", "Prehistoric Dog");
	second = xmms_mock_entry (medialib, 2, "Red Fang", "Red Fang", "Reverse Thunder");

	session = xmms_medialib_session_begin (medialib);
	xmms_medialib_entry_property_set_int (session, first, "status",
	                                      XMMS_MEDIALIB_ENTRY_STATUS_NEW);
	xmms_medialib_entry_property_set_int (session, second, "status",
	                                      XMMS_MEDIALIB_ENTRY_STATUS_NEW);
	xmms_medialib_session_commit(session);

	session = xmms_medialib_session_begin (medialib);
	count = xmms_medialib_num_not_resolved (session);
	xmms_medialib_session_commit (session);

	CU_ASSERT_EQUAL (2, count);

	session = xmms_medialib_session_begin (medialib);
	entry = xmms_medialib_entry_not_resolved_get (session);
	xmms_medialib_session_commit (session);
	CU_ASSERT (entry == first || entry == second);
}

CASE (test_query_random_id)
{
	xmms_medialib_session_t *session;
	xmms_medialib_entry_t entry, first, second;
	xmmsv_t *universe;

	first = xmms_mock_entry (medialib, 1, "Red Fang", "Red Fang", "Prehistoric Dog");
	second = xmms_mock_entry (medialib, 2, "Red Fang", "Red Fang", "Reverse Thunder");

	universe = xmmsv_new_coll (XMMS_COLLECTION_TYPE_UNIVERSE);
	session = xmms_medialib_session_begin (medialib);
	entry = xmms_medialib_query_random_id (session, universe);
	xmms_medialib_session_commit (session);
	xmmsv_unref (universe);

	CU_ASSERT (entry == first || entry == second);
}

CASE (test_session)
{
	xmms_medialib_session_t *session;
	xmms_medialib_entry_t first;
	xmms_error_t err;
	xmmsv_t *universe, *result, *spec;
	gint tracknr;

	first = xmms_mock_entry (medialib, 1, "Red Fang", "Red Fang", "Prehistoric Dog");

	session = xmms_medialib_session_begin (medialib);
	xmms_medialib_entry_remove (session, first);
	xmms_medialib_session_abort (session);

	session = xmms_medialib_session_begin (medialib);
	result = xmms_medialib_entry_property_get_value (session, first, "tracknr");
	xmms_medialib_session_commit (session);
	xmmsv_get_int (result, &tracknr);
	CU_ASSERT_EQUAL (1, tracknr);
	xmmsv_unref (result);

	session = xmms_medialib_session_begin (medialib);
	xmms_medialib_entry_remove (session, first);
	xmms_medialib_session_commit (session);

	session = xmms_medialib_session_begin (medialib);
	result = xmms_medialib_entry_property_get_value (session, first, "tracknr");
	xmms_medialib_session_commit (session);
	CU_ASSERT_PTR_NULL (result);

	universe = xmmsv_new_coll (XMMS_COLLECTION_TYPE_UNIVERSE);
	spec = xmmsv_from_xson ("{ 'type': 'count' }");

	session = xmms_medialib_session_begin (medialib);
	xmms_medialib_query (session, universe, spec, &err);
	xmms_medialib_session_abort (session);

	xmmsv_unref (universe);
	xmmsv_unref (spec);
}

CASE (test_metadata_fetch_spec)
{
	xmmsv_t *universe, *spec, *result;
	xmms_error_t err;

	xmms_mock_entry (medialib, 1, "Red Fang", "Red Fang", "Prehistoric Dog");

	universe = xmmsv_new_coll (XMMS_COLLECTION_TYPE_UNIVERSE);

	/* missing 'type' parameter */
	spec = xmmsv_from_xson ("{ 'notype': 'metadata' }");
	CU_ASSERT_PTR_NULL (medialib_query (universe, spec, &err));
	CU_ASSERT_TRUE (xmms_error_iserror (&err));
	xmmsv_unref (spec);

	/* missing 'get' parameter */
	spec = xmmsv_from_xson ("{ 'type': 'metadata' }");
	CU_ASSERT_PTR_NULL (medialib_query (universe, spec, &err));
	CU_ASSERT_TRUE (xmms_error_iserror (&err));
	xmmsv_unref (spec);

	/* invalid 'get' entry */
	spec = xmmsv_from_xson ("{ 'type': 'metadata', 'get': ['crap'] }");
	CU_ASSERT_PTR_NULL (medialib_query (universe, spec, &err));
	CU_ASSERT_TRUE (xmms_error_iserror (&err));
	xmmsv_unref (spec);

	/* invalid 'get' parameter */
	spec = xmmsv_from_xson ("{ 'type': 'metadata', 'get': {} }");
	CU_ASSERT_PTR_NULL (medialib_query (universe, spec, &err));
	CU_ASSERT_TRUE (xmms_error_iserror (&err));
	xmmsv_unref (spec);

	/* invalid 'get' parameter type */
	spec = xmmsv_from_xson ("{ 'type': 'metadata', 'get': [0] }");
	CU_ASSERT_PTR_NULL (medialib_query (universe, spec, &err));
	CU_ASSERT_TRUE (xmms_error_iserror (&err));
	xmmsv_unref (spec);

	/* empty 'get' parameter */
	spec = xmmsv_from_xson ("{ 'type': 'metadata', 'get': [] }");
	CU_ASSERT_PTR_NULL (medialib_query (universe, spec, &err));
	CU_ASSERT_TRUE (xmms_error_iserror (&err));
	xmmsv_unref (spec);

	/* duplicate 'get' entries */
	spec = xmmsv_from_xson ("{ 'type': 'metadata', 'get': ['value', 'value'] }");
	CU_ASSERT_PTR_NULL (medialib_query (universe, spec, &err));
	CU_ASSERT_TRUE (xmms_error_iserror (&err));
	xmmsv_unref (spec);

	/* all 'get' entries */
	spec = xmmsv_from_xson ("{ 'type': 'metadata', 'get': ['id', 'field', 'source', 'value'] }");
	result = medialib_query (universe, spec, &err);
	CU_ASSERT_FALSE (xmms_error_iserror (&err));
	CU_ASSERT_PTR_NOT_NULL (result);
	xmmsv_unref (spec);
	xmmsv_unref (result);

	/* valid 'fields' content */
	spec = xmmsv_from_xson ("{ 'type': 'metadata', 'fields': ['artist', 'title'], 'get': ['value'] }");
	result = medialib_query (universe, spec, &err);
	CU_ASSERT_PTR_NOT_NULL (result);
	CU_ASSERT_FALSE (xmms_error_iserror (&err));
	xmmsv_unref (spec);
	xmmsv_unref (result);

	/* duplicate 'fields' content */
	spec = xmmsv_from_xson ("{ 'type': 'metadata', 'fields': ['a', 'a'], 'get': ['value'] }");
	CU_ASSERT_PTR_NULL (medialib_query (universe, spec, &err));
	CU_ASSERT_TRUE (xmms_error_iserror (&err));
	xmmsv_unref (spec);

	/* invalid 'fields' parameter */
	spec = xmmsv_from_xson ("{ 'type': 'metadata', 'fields': 0, 'get': ['value'] }");
	CU_ASSERT_PTR_NULL (medialib_query (universe, spec, &err));
	CU_ASSERT_TRUE (xmms_error_iserror (&err));
	xmmsv_unref (spec);

	/* empty 'fields' parameter, fetch all fields */
	spec = xmmsv_from_xson ("{ 'type': 'metadata', 'fields': [], 'get': ['value'] }");
	result = medialib_query (universe, spec, &err);
	CU_ASSERT_FALSE (xmms_error_iserror (&err));
	xmmsv_unref (result);
	xmmsv_unref (spec);

	/* invalid 'fields' content */
	spec = xmmsv_from_xson ("{ 'type': 'metadata', 'fields': [0], 'get': ['value'] }");
	CU_ASSERT_PTR_NULL (medialib_query (universe, spec, &err));
	CU_ASSERT_TRUE (xmms_error_iserror (&err));
	xmmsv_unref (spec);


	/* valid source-preferences parameter */
	spec = xmmsv_from_xson ("{ 'type': 'metadata', 'get': ['value'], 'source-preference': ['*'] }");
	result = medialib_query (universe, spec, &err);
	CU_ASSERT_FALSE (xmms_error_iserror (&err));
	xmmsv_unref (result);
	xmmsv_unref (spec);

	/* invalid source-preferences parameter */
	spec = xmmsv_from_xson ("{ 'type': 'metadata', 'get': ['value'], 'source-preference': 0 }");
	CU_ASSERT_PTR_NULL (medialib_query (universe, spec, &err));
	CU_ASSERT_TRUE (xmms_error_iserror (&err));
	xmmsv_unref (spec);

	/* empty source-preferences parameter */
	spec = xmmsv_from_xson ("{ 'type': 'metadata', 'get': ['value'], 'source-preference': [] }");
	result = medialib_query (universe, spec, &err);
	CU_ASSERT_FALSE (xmms_error_iserror (&err));
	xmmsv_unref (result);
	xmmsv_unref (spec);

	/* invalid source-preferences parameter */
	spec = xmmsv_from_xson ("{ 'type': 'metadata', 'get': ['value'], 'source-preference': [0] }");
	CU_ASSERT_PTR_NULL (medialib_query (universe, spec, &err));
	CU_ASSERT_TRUE (xmms_error_iserror (&err));
	xmmsv_unref (spec);

	/* invalid aggregate parameter */
	spec = xmmsv_from_xson ("{ 'type': 'metadata', 'get': ['value'], 'aggregate': ['avg'] }");
	CU_ASSERT_PTR_NULL (medialib_query (universe, spec, &err));
	CU_ASSERT_TRUE (xmms_error_iserror (&err));
	xmmsv_unref (spec);

	/* invalid aggregate function */
	spec = xmmsv_from_xson ("{ 'type': 'metadata', 'get': ['value'], 'aggregate': 'sausage' }");
	CU_ASSERT_PTR_NULL (medialib_query (universe, spec, &err));
	CU_ASSERT_TRUE (xmms_error_iserror (&err));
	xmmsv_unref (spec);


	xmmsv_unref (universe);
}

CASE (test_cluster_dict_and_list_fetch_spec)
{
	xmmsv_t *universe, *spec, *result;
	xmms_error_t err;

	xmms_mock_entry (medialib, 1, "Red Fang", "Red Fang", "Prehistoric Dog");

	universe = xmmsv_new_coll (XMMS_COLLECTION_TYPE_UNIVERSE);

	/* missing 'cluster-by' parameter, defaults to 'value' */
	spec = xmmsv_from_xson ("{ 'type': 'cluster-dict', 'cluster-field': 'artist', 'data': { 'type': 'count' } }");
	result = medialib_query (universe, spec, &err);
	CU_ASSERT_PTR_NOT_NULL (result);
	CU_ASSERT_FALSE (xmms_error_iserror (&err));
	xmmsv_unref (spec);
	xmmsv_unref (result);

	/* invalid 'cluster-by' entry */
	spec = xmmsv_from_xson ("{ 'type': 'cluster-dict', 'cluster-by': ['crap'] }");
	CU_ASSERT_PTR_NULL (medialib_query (universe, spec, &err));
	CU_ASSERT_TRUE (xmms_error_iserror (&err));
	xmmsv_unref (spec);

	/* invalid 'cluster-by' entry, missing 'cluster-field' */
	spec = xmmsv_from_xson ("{ 'type': 'cluster-dict', 'cluster-by': 'value', 'data': { 'type': 'count' } }");
	CU_ASSERT_PTR_NULL (medialib_query (universe, spec, &err));
	CU_ASSERT_TRUE (xmms_error_iserror (&err));
	xmmsv_unref (spec);

	/* bogous 'cluster-by' entry */
	spec = xmmsv_from_xson ("{ 'type': 'cluster-dict', 'cluster-by': 'sausage', 'data': { 'type': 'count' } }");
	CU_ASSERT_PTR_NULL (medialib_query (universe, spec, &err));
	CU_ASSERT_TRUE (xmms_error_iserror (&err));
	xmmsv_unref (spec);

	/* missing 'data' entry */
	spec = xmmsv_from_xson ("{ 'type': 'cluster-dict', 'cluster-field': 'artist' }");
	CU_ASSERT_PTR_NULL (medialib_query (universe, spec, &err));
	CU_ASSERT_TRUE (xmms_error_iserror (&err));
	xmmsv_unref (spec);

	/* bogous 'data' entry */
	spec = xmmsv_from_xson ("{ 'type': 'cluster-list', 'cluster-field': 'artist', 'data': { 'type': 'sausage' } }");
	CU_ASSERT_PTR_NULL (medialib_query (universe, spec, &err));
	CU_ASSERT_TRUE (xmms_error_iserror (&err));
	xmmsv_unref (spec);

	xmmsv_unref (universe);
}

CASE (test_organize_fetch_spec)
{
	xmmsv_t *universe, *spec, *result;
	xmms_error_t err;

	xmms_mock_entry (medialib, 1, "Red Fang", "Red Fang", "Prehistoric Dog");

	universe = xmmsv_new_coll (XMMS_COLLECTION_TYPE_UNIVERSE);

	/* missing 'data' entry */
	spec = xmmsv_from_xson ("{ 'type': 'organize' }");
	CU_ASSERT_PTR_NULL (medialib_query (universe, spec, &err));
	CU_ASSERT_TRUE (xmms_error_iserror (&err));
	xmmsv_unref (spec);

	/* wrong type for 'data' entry */
	spec = xmmsv_from_xson ("{ 'type': 'organize', 'data': [] }");
	CU_ASSERT_PTR_NULL (medialib_query (universe, spec, &err));
	CU_ASSERT_TRUE (xmms_error_iserror (&err));
	xmmsv_unref (spec);

	/* bogous 'data' entry */
	spec = xmmsv_from_xson ("{ 'type': 'organize', 'data': { 'korv': { 'type': 'sausage' } } }");
	CU_ASSERT_PTR_NULL (medialib_query (universe, spec, &err));
	CU_ASSERT_TRUE (xmms_error_iserror (&err));
	xmmsv_unref (spec);

	/* valid 'data' entry */
	spec = xmmsv_from_xson ("{ 'type': 'organize', 'data': { 'count': { 'type': 'count' } } }");
	result = medialib_query (universe, spec, &err);
	CU_ASSERT_PTR_NOT_NULL (result);
	CU_ASSERT_FALSE (xmms_error_iserror (&err));
	xmmsv_unref (spec);
	xmmsv_unref (result);

	xmmsv_unref (universe);
}

CASE(test_client_rehash)
{
	xmms_medialib_session_t *session;
	xmms_medialib_entry_t first, second;
	xmmsv_t *value;
	gint status;

	/* rehashing missing id should fail */
	value = XMMS_IPC_CALL (medialib, XMMS_IPC_CMD_REHASH, xmmsv_new_int (1));
	CU_ASSERT (xmmsv_is_type (value, XMMSV_TYPE_ERROR));
	xmmsv_unref (value);

	first = xmms_mock_entry (medialib, 1, "Red Fang", "Red Fang", "Prehistoric Dog");
	second = xmms_mock_entry (medialib, 2, "Red Fang", "Red Fang", "Reverse Thunder");

	/* check base status */
	session = xmms_medialib_session_begin (medialib);

	status = xmms_medialib_entry_property_get_int (session, first, "status");
	CU_ASSERT_EQUAL (XMMS_MEDIALIB_ENTRY_STATUS_OK, status);

	status = xmms_medialib_entry_property_get_int (session, second, "status");
	CU_ASSERT_EQUAL (XMMS_MEDIALIB_ENTRY_STATUS_OK, status);

	xmms_medialib_session_abort (session);

	/* rehash first entry */
	value = XMMS_IPC_CALL (medialib, XMMS_IPC_CMD_REHASH, xmmsv_new_int (first));
	CU_ASSERT (xmmsv_is_type (value, XMMSV_TYPE_NONE));
	xmmsv_unref (value);

	/* verify that only entry 1 have rehash status */
	session = xmms_medialib_session_begin (medialib);

	status = xmms_medialib_entry_property_get_int (session, first, "status");
	CU_ASSERT_EQUAL (XMMS_MEDIALIB_ENTRY_STATUS_REHASH, status);

	status = xmms_medialib_entry_property_get_int (session, second, "status");
	CU_ASSERT_EQUAL (XMMS_MEDIALIB_ENTRY_STATUS_OK, status);

	xmms_medialib_session_abort (session);

	/* rehash all entries */
	value = XMMS_IPC_CALL (medialib, XMMS_IPC_CMD_REHASH, xmmsv_new_int (0));
	xmmsv_unref (value);

	/* verify that both entries have rehash status */
	session = xmms_medialib_session_begin (medialib);

	status = xmms_medialib_entry_property_get_int (session, first, "status");
	CU_ASSERT_EQUAL (XMMS_MEDIALIB_ENTRY_STATUS_REHASH, status);

	status = xmms_medialib_entry_property_get_int (session, second, "status");
	CU_ASSERT_EQUAL (XMMS_MEDIALIB_ENTRY_STATUS_REHASH, status);

	xmms_medialib_session_abort (session);
}

CASE(test_client_get_info)
{
	xmmsv_t *result, *title, *server;
	const gchar *value;

	xmms_mock_entry (medialib, 1, "Red Fang", "Red Fang", "Prehistoric Dog");

	result = XMMS_IPC_CALL (medialib, XMMS_IPC_CMD_INFO, xmmsv_new_int (0));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_ERROR));
	xmmsv_unref (result);

	result = XMMS_IPC_CALL (medialib, XMMS_IPC_CMD_INFO, xmmsv_new_int (1337));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_ERROR));
	xmmsv_unref (result);

	result = XMMS_IPC_CALL (medialib, XMMS_IPC_CMD_INFO, xmmsv_new_int (1));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_DICT));
	CU_ASSERT (xmmsv_dict_get (result, "title", &title));
	CU_ASSERT (xmmsv_dict_get (title, "server", &server));
	CU_ASSERT (xmmsv_get_string (server, &value));
	CU_ASSERT_STRING_EQUAL ("Prehistoric Dog", value);
	xmmsv_unref (result);
}

CASE(test_client_entry_add)
{
	xmms_medialib_session_t *session;
	xmmsv_t *result;

	session = xmms_medialib_session_begin (medialib);
	CU_ASSERT_FALSE (xmms_medialib_check_id (session, 1));
	xmms_medialib_session_abort (session);

	result = XMMS_IPC_CALL (medialib, XMMS_IPC_CMD_MLIB_ADD_URL, xmmsv_new_string ("file:///apankorv.mp3"));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);

	session = xmms_medialib_session_begin (medialib);
	CU_ASSERT_TRUE (xmms_medialib_check_id (session, 1));
	xmms_medialib_session_abort (session);
}


CASE(test_client_entry_remove)
{
	xmms_medialib_session_t *session;
	xmms_medialib_entry_t entry;
	xmmsv_t *result;

	result = XMMS_IPC_CALL (medialib, XMMS_IPC_CMD_REMOVE_ID, xmmsv_new_int (1337));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_ERROR));
	xmmsv_unref (result);

	entry = xmms_mock_entry (medialib, 1, "Red Fang", "Red Fang", "Prehistoric Dog");

	session = xmms_medialib_session_begin (medialib);
	CU_ASSERT_TRUE (xmms_medialib_check_id (session, entry));
	xmms_medialib_session_abort (session);

	result = XMMS_IPC_CALL (medialib, XMMS_IPC_CMD_REMOVE_ID, xmmsv_new_int (entry));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);

	session = xmms_medialib_session_begin (medialib);
	CU_ASSERT_FALSE (xmms_medialib_check_id (session, entry));
	xmms_medialib_session_abort (session);
}

CASE(test_client_get_id)
{
	xmms_medialib_entry_t entry;
	xmmsv_t *result;
	gint mid;

	entry = xmms_mock_entry (medialib, 1, "Red Fang", "Red Fang", "Prehistoric Dog");

	result = XMMS_IPC_CALL (medialib, XMMS_IPC_CMD_GET_ID, xmmsv_new_string ("Red+FangRed+FangPrehistoric+Dog"));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_INT32));
	CU_ASSERT (xmmsv_get_int (result, &mid));
	CU_ASSERT_EQUAL (entry, mid);
	xmmsv_unref (result);
}

CASE(test_client_property_set)
{
	xmms_medialib_entry_t entry;
	xmmsv_t *result, *title, *tracknr, *client;
	const gchar *string_value;
	gint int_value;

	entry = xmms_mock_entry (medialib, 1, "Red Fang", "Red Fang", "Prehistoric Dog");

	/* clients must not overwrite server properties */
	result = XMMS_IPC_CALL (medialib, XMMS_IPC_CMD_PROPERTY_SET_INT,
	                        xmmsv_new_int (entry),
	                        xmmsv_new_string ("server"),
	                        xmmsv_new_string ("tracknr"),
	                        xmmsv_new_int (2));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_ERROR));
	xmmsv_unref (result);

	result = XMMS_IPC_CALL (medialib, XMMS_IPC_CMD_PROPERTY_SET_INT,
	                        xmmsv_new_int (0),
	                        xmmsv_new_string ("client/unittest"),
	                        xmmsv_new_string ("tracknr"),
	                        xmmsv_new_int (2));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_ERROR));
	xmmsv_unref (result);

	result = XMMS_IPC_CALL (medialib, XMMS_IPC_CMD_PROPERTY_SET_INT,
	                        xmmsv_new_int (1337),
	                        xmmsv_new_string ("client/unittest"),
	                        xmmsv_new_string ("tracknr"),
	                        xmmsv_new_int (2));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_ERROR));
	xmmsv_unref (result);

	result = XMMS_IPC_CALL (medialib, XMMS_IPC_CMD_PROPERTY_SET_INT,
	                        xmmsv_new_int (entry),
	                        xmmsv_new_string ("client/unittest"),
	                        xmmsv_new_string ("tracknr"),
	                        xmmsv_new_int (2));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);

	/* clients must not overwrite server properties */
	result = XMMS_IPC_CALL (medialib, XMMS_IPC_CMD_PROPERTY_SET_STR,
	                        xmmsv_new_int (entry),
	                        xmmsv_new_string ("server"),
	                        xmmsv_new_string ("title"),
	                        xmmsv_new_string ("Reverse Thunder"));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_ERROR));
	xmmsv_unref (result);

	result = XMMS_IPC_CALL (medialib, XMMS_IPC_CMD_PROPERTY_SET_STR,
	                        xmmsv_new_int (0),
	                        xmmsv_new_string ("client/unittest"),
	                        xmmsv_new_string ("title"),
	                        xmmsv_new_string ("Reverse Thunder"));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_ERROR));
	xmmsv_unref (result);

	result = XMMS_IPC_CALL (medialib, XMMS_IPC_CMD_PROPERTY_SET_STR,
	                        xmmsv_new_int (1337),
	                        xmmsv_new_string ("client/unittest"),
	                        xmmsv_new_string ("title"),
	                        xmmsv_new_string ("Reverse Thunder"));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_ERROR));
	xmmsv_unref (result);

	result = XMMS_IPC_CALL (medialib, XMMS_IPC_CMD_PROPERTY_SET_STR,
	                        xmmsv_new_int (entry),
	                        xmmsv_new_string ("client/unittest"),
	                        xmmsv_new_string ("title"),
	                        xmmsv_new_string ("Reverse Thunder"));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);

	result = XMMS_IPC_CALL (medialib, XMMS_IPC_CMD_INFO, xmmsv_new_int (entry));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_DICT));

	CU_ASSERT (xmmsv_dict_get (result, "title", &title));
	CU_ASSERT (xmmsv_dict_get (title, "client/unittest", &client));
	CU_ASSERT (xmmsv_get_string (client, &string_value));
	CU_ASSERT_STRING_EQUAL ("Reverse Thunder", string_value);

	CU_ASSERT (xmmsv_dict_get (result, "tracknr", &tracknr));
	CU_ASSERT (xmmsv_dict_get (tracknr, "client/unittest", &client));
	CU_ASSERT (xmmsv_get_int (client, &int_value));
	CU_ASSERT_EQUAL (2, int_value);

	xmmsv_unref (result);
}

CASE(test_client_property_remove)
{
	xmms_medialib_entry_t entry;
	xmmsv_t *result, *tracknr, *client;
	gint int_value;

	entry = xmms_mock_entry (medialib, 1, "Red Fang", "Red Fang", "Prehistoric Dog");

	/* clients must not remove server properties */
	result = XMMS_IPC_CALL (medialib, XMMS_IPC_CMD_PROPERTY_REMOVE,
	                        xmmsv_new_int (entry),
	                        xmmsv_new_string ("server"),
	                        xmmsv_new_string ("tracknr"));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_ERROR));
	xmmsv_unref (result);

	/* removing properties from non-existing entries should fail */
	result = XMMS_IPC_CALL (medialib, XMMS_IPC_CMD_PROPERTY_REMOVE,
	                        xmmsv_new_int (1337),
	                        xmmsv_new_string ("client/unittest"),
	                        xmmsv_new_string ("tracknr"));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_ERROR));
	xmmsv_unref (result);

	result = XMMS_IPC_CALL (medialib, XMMS_IPC_CMD_PROPERTY_SET_INT,
	                        xmmsv_new_int (entry),
	                        xmmsv_new_string ("client/unittest"),
	                        xmmsv_new_string ("tracknr"),
	                        xmmsv_new_int (2));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);

	result = XMMS_IPC_CALL (medialib, XMMS_IPC_CMD_INFO, xmmsv_new_int (entry));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_DICT));
	CU_ASSERT (xmmsv_dict_get (result, "tracknr", &tracknr));
	CU_ASSERT (xmmsv_dict_get (tracknr, "client/unittest", &client));
	CU_ASSERT (xmmsv_get_int (client, &int_value));
	CU_ASSERT_EQUAL (2, int_value);
	xmmsv_unref (result);

	result = XMMS_IPC_CALL (medialib, XMMS_IPC_CMD_PROPERTY_REMOVE,
	                        xmmsv_new_int (entry),
	                        xmmsv_new_string ("client/unittest"),
	                        xmmsv_new_string ("tracknr"));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);

	result = XMMS_IPC_CALL (medialib, XMMS_IPC_CMD_INFO, xmmsv_new_int (entry));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_DICT));
	CU_ASSERT (xmmsv_dict_get (result, "tracknr", &tracknr));
	CU_ASSERT_FALSE (xmmsv_dict_get (tracknr, "client/unittest", &client));

	xmmsv_unref (result);
}

CASE(test_client_move_entry)
{
	xmms_medialib_session_t *session;
	xmms_medialib_entry_t entry;
	xmmsv_t *result;
	gchar *string;

	entry = xmms_mock_entry (medialib, 1, "Red Fang", "Red Fang", "Prehistoric Dog");

	result = XMMS_IPC_CALL (medialib, XMMS_IPC_CMD_MOVE_URL,
	                        xmmsv_new_int (0),
	                        xmmsv_new_string ("file://test.mp3"));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_ERROR));
	xmmsv_unref (result);

	result = XMMS_IPC_CALL (medialib, XMMS_IPC_CMD_MOVE_URL,
	                        xmmsv_new_int (1337),
	                        xmmsv_new_string ("file://test.mp3"));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_ERROR));
	xmmsv_unref (result);

	session = xmms_medialib_session_begin (medialib);
	string = xmms_medialib_entry_property_get_str (session, entry, "url");
	CU_ASSERT_STRING_NOT_EQUAL ("file://test.mp3", string);
	xmms_medialib_session_abort (session);
	g_free (string);

	result = XMMS_IPC_CALL (medialib, XMMS_IPC_CMD_MOVE_URL,
	                        xmmsv_new_int (entry),
	                        xmmsv_new_string ("file://test.mp3"));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);

	session = xmms_medialib_session_begin (medialib);
	string = xmms_medialib_entry_property_get_str (session, entry, "url");
	CU_ASSERT_STRING_EQUAL ("file://test.mp3", string);
	xmms_medialib_session_abort (session);
	g_free (string);
}

CASE (test_session_locking)
{
	xmms_medialib_session_t *session, *inner_session;
	xmms_medialib_entry_t first;
	gint status, inner_status, new_status;

	first = xmms_mock_entry (medialib, 1, "Red Fang", "Red Fang", "Prehistoric Dog");

	session = xmms_medialib_session_begin (medialib);

	status = xmms_medialib_entry_property_get_int (session, first,
	                                               XMMS_MEDIALIB_ENTRY_PROPERTY_STATUS);

	/* this should be ok as there have been no writes to that property */
	inner_session = xmms_medialib_session_begin_ro (medialib);
	inner_status = xmms_medialib_entry_property_get_int (inner_session, first,
	                                                     XMMS_MEDIALIB_ENTRY_PROPERTY_STATUS);
	CU_ASSERT_EQUAL (status, inner_status);
	xmms_medialib_session_abort (inner_session);

	xmms_medialib_entry_status_set (session, first,
	                                XMMS_MEDIALIB_ENTRY_STATUS_NEW);

	/* this should be ok as 'inner_session' aborted, and the stuff it did was read-only operations */
	CU_ASSERT_TRUE (xmms_medialib_session_commit (session));

	session = xmms_medialib_session_begin (medialib);
	new_status = xmms_medialib_entry_property_get_int (session, first,
	                                                   XMMS_MEDIALIB_ENTRY_PROPERTY_STATUS);
	xmms_medialib_session_abort (session);

	CU_ASSERT_NOT_EQUAL (status, new_status);
}
