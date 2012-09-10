#include <locale.h>

#include "xcu.h"

#include "xmmspriv/xmms_log.h"
#include "xmmspriv/xmms_ipc.h"
#include "xmmspriv/xmms_config.h"
#include "xmmspriv/xmms_medialib.h"
#include "xmmspriv/xmms_collection.h"

#include "utils/jsonism.h"
#include "utils/value_utils.h"
#include "utils/coll_utils.h"
#include "server-utils/ipc_call.h"
#include "server-utils/mlib_utils.h"

static xmms_medialib_t *medialib;
static xmms_coll_dag_t *dag;

SETUP (coll) {
	g_thread_init (0);

	setlocale (LC_COLLATE, "");

	xmms_ipc_init ();

	xmms_log_init (0);

	xmms_config_init ("memory://");
	xmms_config_property_register ("medialib.path", "memory://", NULL, NULL);

	medialib = xmms_medialib_init ();
	dag = xmms_collection_init (medialib);

	return 0;
}

CLEANUP () {
	xmms_object_unref (medialib);
	xmms_object_unref (dag);

	xmms_config_shutdown ();

	xmms_ipc_shutdown ();

	return 0;
}


CASE (test_client_save) {
	xmms_future_t *future;
	xmmsv_coll_t *universe;
	xmmsv_t *result, *signals, *expected;

	future = XMMS_IPC_CHECK_SIGNAL (dag, XMMS_IPC_SIGNAL_COLLECTION_CHANGED);

	universe = xmmsv_coll_new (XMMS_COLLECTION_TYPE_UNIVERSE);

	/* emits XMMS_COLLECTION_CHANGED_ADD */
	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_SAVE,
	                        xmmsv_new_string ("Test"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS),
	                        xmmsv_new_coll (universe));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);

	xmmsv_coll_unref (universe);

	/* each save requires a new collection, normally handled by IPC deserialization */
	universe = xmmsv_coll_new (XMMS_COLLECTION_TYPE_UNIVERSE);

	/* replace the collection, emits XMMS_COLLECTION_CHANGED_UPDATE */
	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_SAVE,
	                        xmmsv_new_string ("Test"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS),
	                        xmmsv_new_coll (universe));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);

	xmmsv_coll_unref (universe);

	signals = xmms_future_await (future, 2);

	/* XMMS_COLLECTION_CHANGED_ADD = 0, XMMS_COLLECTION_CHANGED_UPDATE = 1 */
	expected = xmmsv_from_xson ("[{ 'type': 0, 'namespace': 'Collections', 'name': 'Test' },"
	                            " { 'type': 1, 'namespace': 'Collections', 'name': 'Test' }]");
	CU_ASSERT (xmmsv_compare (expected, signals));
	xmmsv_unref (signals);
	xmmsv_unref (expected);
}

CASE (test_client_get) {
	xmmsv_coll_t *universe;
	xmmsv_t *result;

	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_GET,
	                        xmmsv_new_string ("whatever"),
	                        xmmsv_new_string ("invalid namespace"));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_ERROR));
	xmmsv_unref (result);

	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_GET,
	                        xmmsv_new_string ("missing collection"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_ERROR));
	xmmsv_unref (result);

	universe = xmmsv_coll_new (XMMS_COLLECTION_TYPE_UNIVERSE);
	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_SAVE,
	                        xmmsv_new_string ("Test"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS),
	                        xmmsv_new_coll (universe));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);
	xmmsv_coll_unref (universe);

	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_GET,
	                        xmmsv_new_string ("Test"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_COLL));
	xmmsv_unref (result);
}

CASE (test_client_remove)
{
	xmms_future_t *future;
	xmmsv_coll_t *universe;
	xmmsv_t *result, *signals, *expected;

	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_REMOVE,
	                        xmmsv_new_string ("Test"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_ERROR));
	xmmsv_unref (result);

	future = XMMS_IPC_CHECK_SIGNAL (dag, XMMS_IPC_SIGNAL_COLLECTION_CHANGED);

	universe = xmmsv_coll_new (XMMS_COLLECTION_TYPE_UNIVERSE);
	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_SAVE,
	                        xmmsv_new_string ("Test"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS),
	                        xmmsv_new_coll (universe));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);
	xmmsv_coll_unref (universe);

	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_REMOVE,
	                        xmmsv_new_string ("Test"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);

	signals = xmms_future_await (future, 2);

	/* XMMS_COLLECTION_CHANGED_ADD = 0, XMMS_COLLECTION_CHANGED_REMOVE = 3 */
	expected = xmmsv_from_xson ("[{ 'type': 0, 'namespace': 'Collections', 'name': 'Test' },"
	                            " { 'type': 3, 'namespace': 'Collections', 'name': 'Test' }]");
	CU_ASSERT (xmmsv_compare (expected, signals));
	xmmsv_unref (signals);
	xmmsv_unref (expected);
	xmms_future_free (future);

	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_GET,
	                        xmmsv_new_string ("Test"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_ERROR));
	xmmsv_unref (result);
}

CASE (test_client_rename)
{
	xmmsv_coll_t *universe;
	xmmsv_t *result;

	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_RENAME,
	                        xmmsv_new_string ("Test A"),
	                        xmmsv_new_string ("Test B"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_ERROR));
	xmmsv_unref (result);

	universe = xmmsv_coll_new (XMMS_COLLECTION_TYPE_UNIVERSE);
	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_SAVE,
	                        xmmsv_new_string ("Test A"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS),
	                        xmmsv_new_coll (universe));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);
	xmmsv_coll_unref (universe);

	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_RENAME,
	                        xmmsv_new_string ("Test A"),
	                        xmmsv_new_string ("Test B"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);

	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_GET,
	                        xmmsv_new_string ("Test A"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_ERROR));
	xmmsv_unref (result);

	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_GET,
	                        xmmsv_new_string ("Test B"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_COLL));
	xmmsv_unref (result);
}

CASE (test_client_find)
{
	xmms_medialib_entry_t entry;
	xmmsv_coll_t *universe, *equals;
	xmmsv_t *result;
	const gchar *string;

	entry = xmms_mock_entry (medialib, 1, "Red Fang", "Red Fang", "Prehistoric Dog");

	/* should be found */
	universe = xmmsv_coll_new (XMMS_COLLECTION_TYPE_UNIVERSE);
	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_SAVE,
	                        xmmsv_new_string ("Test A"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS),
	                        xmmsv_new_coll (universe));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);
	xmmsv_coll_unref (universe);

	/* should be found */
	universe = xmmsv_coll_new (XMMS_COLLECTION_TYPE_UNIVERSE);
	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_SAVE,
	                        xmmsv_new_string ("Test B"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS),
	                        xmmsv_new_coll (universe));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);
	xmmsv_coll_unref (universe);

	/* should not be found */
	universe = xmmsv_coll_new (XMMS_COLLECTION_TYPE_UNIVERSE);
	equals = xmmsv_coll_new (XMMS_COLLECTION_TYPE_EQUALS);
	xmmsv_coll_attribute_set (equals, "type", "id");
	xmmsv_coll_attribute_set (equals, "value", "1337");
	xmmsv_coll_add_operand (equals, universe);
	xmmsv_coll_unref (universe);

	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_SAVE,
	                        xmmsv_new_string ("Test C"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS),
	                        xmmsv_new_coll (equals));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);
	xmmsv_coll_unref (equals);

	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_FIND,
	                        xmmsv_new_int (entry),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_LIST));
	CU_ASSERT_EQUAL (2, xmmsv_list_get_size (result));
	CU_ASSERT (xmmsv_list_get_string (result, 0, &string));
	CU_ASSERT (strcmp ("Test A", string) == 0 || strcmp ("Test B", string) == 0);
	CU_ASSERT (xmmsv_list_get_string (result, 1, &string));
	CU_ASSERT (strcmp ("Test A", string) == 0 || strcmp ("Test B", string) == 0);
	xmmsv_unref (result);
}

CASE (test_client_list)
{
	xmmsv_coll_t *universe, *idlist;
	xmmsv_t *result;

	universe = xmmsv_coll_new (XMMS_COLLECTION_TYPE_UNIVERSE);
	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_SAVE,
	                        xmmsv_new_string ("Test A"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS),
	                        xmmsv_new_coll (universe));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);
	xmmsv_coll_unref (universe);

	idlist = xmmsv_coll_new (XMMS_COLLECTION_TYPE_IDLIST);
	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_SAVE,
	                        xmmsv_new_string ("Test B"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_PLAYLISTS),
	                        xmmsv_new_coll (idlist));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);
	xmmsv_coll_unref (idlist);

	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_LIST,
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_LIST));
	CU_ASSERT_EQUAL (1, xmmsv_list_get_size (result));
	xmmsv_unref (result);

	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_LIST,
	                        xmmsv_new_string (XMMS_COLLECTION_NS_PLAYLISTS));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_LIST));
	CU_ASSERT_EQUAL (1, xmmsv_list_get_size (result));
	xmmsv_unref (result);
}

CASE (test_client_query_infos)
{
	xmms_medialib_entry_t entry;
	xmms_medialib_session_t *session;
	xmmsv_coll_t *universe, *ordered;
	xmmsv_t *expected, *result, *order, *fetch, *group;

	xmms_mock_entry (medialib, 1, "Red Fang", "Red Fang", "Prehistoric Dog");
	entry = xmms_mock_entry (medialib, 2, "Red Fang", "Red Fang", "Reverse Thunder");

	session = xmms_medialib_session_begin (medialib);
	xmms_medialib_entry_property_set_str_source (session, entry,
	                                             XMMS_MEDIALIB_ENTRY_PROPERTY_YEAR,
	                                             "2009", "client/unittest");
	xmms_medialib_session_commit (session);


	universe = xmmsv_coll_new (XMMS_COLLECTION_TYPE_UNIVERSE);

	order = xmmsv_build_list (XMMSV_LIST_ENTRY_STR ("artist"),
	                          XMMSV_LIST_ENTRY_STR ("album"),
	                          XMMSV_LIST_ENTRY_STR ("tracknr"),
	                          XMMSV_LIST_END);

	ordered = xmmsv_coll_add_order_operators (universe, order);
	xmmsv_coll_unref (universe);
	xmmsv_unref (order);

	fetch = xmmsv_build_list (XMMSV_LIST_ENTRY_STR ("artist"),
	                          XMMSV_LIST_ENTRY_STR ("album"),
	                          XMMSV_LIST_ENTRY_STR ("title"),
	                          XMMSV_LIST_ENTRY_STR ("tracknr"),
	                          XMMSV_LIST_ENTRY_STR ("date"),
	                          XMMSV_LIST_END);

	group = xmmsv_build_list (XMMSV_LIST_ENTRY_STR ("position"),
	                          XMMSV_LIST_END);

	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_QUERY_INFOS,
	                        xmmsv_new_coll (ordered),
	                        xmmsv_new_int (0), xmmsv_new_int (0),
	                        fetch, group);
	xmmsv_coll_unref (ordered);

	expected = xmmsv_from_xson ("[{                             "
	                            "  'artist': 'Red Fang',        "
	                            "   'album': 'Red Fang',        "
	                            " 'tracknr':  1,                "
	                            "   'title': 'Prehistoric Dog'  "
	                            "}, {                           "
	                            "   'album': 'Red Fang',        "
	                            " 'tracknr':  2,                "
	                            "   'title': 'Reverse Thunder', "
	                            "  'artist': 'Red Fang',        "
	                            "    'date': '2009'             "
	                            "}]                             ");

	CU_ASSERT (xmmsv_compare (expected, result));

	xmmsv_unref (expected);
	xmmsv_unref (result);
}

CASE (test_client_query_infos2)
{
	xmmsv_coll_t *universe, *ordered;
	xmmsv_t *expected, *result, *order, *fetch, *group;
	gint limit_start, limit_length;

	xmms_mock_entry (medialib, 1, "Beat Bizarre", "If You Knew a Few New", "Scoville Heat Unit");
	xmms_mock_entry (medialib, 2, "Beat Bizarre", "If You Knew a Few New", "SdrawkcaB");
	xmms_mock_entry (medialib, 1, "Beat Bizarre", "Lewd", "Pony Sauce");
	xmms_mock_entry (medialib, 2, "Beat Bizarre", "Lewd", "Monochrome");
	xmms_mock_entry (medialib, 3, "Beat Bizarre", "Lewd", "Brain Drain (remix)");
	xmms_mock_entry (medialib, 4, "Beat Bizarre", "Lewd", "Depth Pitch");
	xmms_mock_entry (medialib, 5, "Beat Bizarre", "Lewd", "The New Breed");
	xmms_mock_entry (medialib, 6, "Beat Bizarre", "Lewd", "Error");
	xmms_mock_entry (medialib, 7, "Beat Bizarre", "Lewd", "Funk Fluid");
	xmms_mock_entry (medialib, 2, "Beat Bizarre", "Pop the Question / Swallow", "Pop the Question");
	xmms_mock_entry (medialib, 2, "Beat Bizarre", "Pop the Question / Swallow", "Swallow");

	universe = xmmsv_coll_new (XMMS_COLLECTION_TYPE_UNIVERSE);

	order = xmmsv_build_list (XMMSV_LIST_ENTRY_STR ("artist"),
	                          XMMSV_LIST_ENTRY_STR ("album"),
	                          XMMSV_LIST_END);

	ordered = xmmsv_coll_add_order_operators (universe, order);
	xmmsv_coll_unref (universe);
	xmmsv_unref (order);

	fetch = xmmsv_build_list (XMMSV_LIST_ENTRY_STR ("artist"),
	                          XMMSV_LIST_ENTRY_STR ("album"),
	                          XMMSV_LIST_END);

	group = xmmsv_build_list (XMMSV_LIST_ENTRY_STR ("artist"),
	                          XMMSV_LIST_ENTRY_STR ("album"),
	                          XMMSV_LIST_END);

	limit_start = 0;
	limit_length = 0;

	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_QUERY_INFOS,
	                        xmmsv_new_coll (ordered),
	                        xmmsv_new_int (limit_start),
	                        xmmsv_new_int (limit_length),
	                        xmmsv_ref (fetch), xmmsv_ref (group));

	expected = xmmsv_from_xson ("[{                                       "
	                            "  'artist': 'Beat Bizarre',              "
	                            "   'album': 'If You Knew a Few New'      "
	                            "}, {                                     "
	                            "  'artist': 'Beat Bizarre',              "
	                            "   'album': 'Lewd'                       "
	                            "}, {                                     "
	                            "  'artist': 'Beat Bizarre',              "
	                            "   'album': 'Pop the Question / Swallow' "
	                            "}]                                       ");


	CU_ASSERT (xmmsv_compare (expected, result));
	xmmsv_unref (expected);
	xmmsv_unref (result);

	limit_start = 0;
	limit_length = 2;

	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_QUERY_INFOS,
	                        xmmsv_new_coll (ordered),
	                        xmmsv_new_int (limit_start),
	                        xmmsv_new_int (limit_length),
	                        xmmsv_ref (fetch), xmmsv_ref (group));

	expected = xmmsv_from_xson ("[{                                       "
	                            "  'artist': 'Beat Bizarre',              "
	                            "   'album': 'If You Knew a Few New'      "
	                            "}, {                                     "
	                            "  'artist': 'Beat Bizarre',              "
	                            "   'album': 'Lewd'                       "
	                            "}]                                       ");
	CU_ASSERT (xmmsv_compare (expected, result));
	xmmsv_unref (expected);
	xmmsv_unref (result);

	limit_start = 1;
	limit_length = 2;

	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_QUERY_INFOS,
	                        xmmsv_new_coll (ordered),
	                        xmmsv_new_int (limit_start),
	                        xmmsv_new_int (limit_length),
	                        xmmsv_ref (fetch), xmmsv_ref (group));
	expected = xmmsv_from_xson ("[{                                       "
	                            "  'artist': 'Beat Bizarre',              "
	                            "   'album': 'Lewd'                       "
	                            "}, {                                     "
	                            "  'artist': 'Beat Bizarre',              "
	                            "   'album': 'Pop the Question / Swallow' "
	                            "}]                                       ");
	CU_ASSERT (xmmsv_compare (expected, result));
	xmmsv_unref (expected);
	xmmsv_unref (result);

	xmmsv_unref (fetch);
	xmmsv_unref (group);
	xmmsv_coll_unref (ordered);
}
