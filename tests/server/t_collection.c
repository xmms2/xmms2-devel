#include <locale.h>

#include "xcu.h"

#include <xmmspriv/xmms_log.h>
#include <xmmspriv/xmms_ipc.h>
#include <xmmspriv/xmms_config.h>
#include <xmmspriv/xmms_medialib.h>
#include <xmmspriv/xmms_collection.h>

#include "utils/jsonism.h"
#include "utils/value_utils.h"
#include "utils/coll_utils.h"
#include "server-utils/ipc_call.h"
#include "server-utils/mlib_utils.h"

static xmms_medialib_t *medialib;
static xmms_coll_dag_t *dag;

SETUP (coll) {
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
	xmms_object_unref (medialib); medialib = NULL;
	xmms_object_unref (dag); dag = NULL;

	xmms_config_shutdown ();

	xmms_ipc_shutdown ();

	return 0;
}


CASE (test_client_save) {
	xmms_future_t *future;
	xmmsv_t *universe;
	xmmsv_t *result, *signals, *expected;

	future = XMMS_IPC_CHECK_SIGNAL (dag, XMMS_IPC_SIGNAL_COLLECTION_CHANGED);

	universe = xmmsv_new_coll (XMMS_COLLECTION_TYPE_UNIVERSE);

	/* emits XMMS_COLLECTION_CHANGED_ADD */
	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_SAVE,
	                        xmmsv_new_string ("Test"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS),
	                        xmmsv_ref (universe));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);

	xmmsv_unref (universe);

	/* each save requires a new collection, normally handled by IPC deserialization */
	universe = xmmsv_new_coll (XMMS_COLLECTION_TYPE_UNIVERSE);

	/* replace the collection, emits XMMS_COLLECTION_CHANGED_UPDATE */
	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_SAVE,
	                        xmmsv_new_string ("Test"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS),
	                        xmmsv_ref (universe));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);

	xmmsv_unref (universe);

	signals = xmms_future_await (future, 2);
	xmms_future_free (future);

	/* XMMS_COLLECTION_CHANGED_ADD = 0, XMMS_COLLECTION_CHANGED_UPDATE = 1 */
	expected = xmmsv_from_xson ("[{ 'type': 0, 'namespace': 'Collections', 'name': 'Test' },"
	                            " { 'type': 1, 'namespace': 'Collections', 'name': 'Test' }]");
	CU_ASSERT (xmmsv_compare (expected, signals));
	xmmsv_unref (signals);
	xmmsv_unref (expected);
}

CASE (test_client_get) {
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

	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_SAVE,
	                        xmmsv_new_string ("Test"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS),
	                        xmmsv_new_coll (XMMS_COLLECTION_TYPE_UNIVERSE));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);

	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_GET,
	                        xmmsv_new_string ("Test"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_COLL));
	xmmsv_unref (result);
}

CASE (test_client_remove)
{
	xmms_future_t *future;
	xmmsv_t *universe;
	xmmsv_t *result, *signals, *expected;

	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_REMOVE,
	                        xmmsv_new_string ("Test"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_ERROR));
	xmmsv_unref (result);

	future = XMMS_IPC_CHECK_SIGNAL (dag, XMMS_IPC_SIGNAL_COLLECTION_CHANGED);

	universe = xmmsv_new_coll (XMMS_COLLECTION_TYPE_UNIVERSE);
	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_SAVE,
	                        xmmsv_new_string ("Test"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS),
	                        xmmsv_ref (universe));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);
	xmmsv_unref (universe);

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
	xmmsv_t *result;

	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_RENAME,
	                        xmmsv_new_string ("Test A"),
	                        xmmsv_new_string ("Test B"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_ERROR));
	xmmsv_unref (result);

	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_SAVE,
	                        xmmsv_new_string ("Test A"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS),
	                        xmmsv_new_coll (XMMS_COLLECTION_TYPE_UNIVERSE));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);

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

CASE (test_client_rename_referenced)
{
	xmmsv_t *result, *reference;
	const char *name;

	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_SAVE,
	                        xmmsv_new_string ("Before Rename"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS),
	                        xmmsv_new_coll (XMMS_COLLECTION_TYPE_UNIVERSE));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);

	reference = xmmsv_new_coll (XMMS_COLLECTION_TYPE_REFERENCE);
	xmmsv_coll_attribute_set_string (reference, "namespace", XMMS_COLLECTION_NS_COLLECTIONS);
	xmmsv_coll_attribute_set_string (reference, "reference", "Before Rename");

	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_SAVE,
	                        xmmsv_new_string ("Test Reference"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS),
	                        xmmsv_ref (reference));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);
	xmmsv_unref (reference);

	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_RENAME,
	                        xmmsv_new_string ("Before Rename"),
	                        xmmsv_new_string ("After Rename"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);

	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_GET,
	                        xmmsv_new_string ("Test Reference"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_COLL));
	CU_ASSERT (xmmsv_coll_attribute_get_string (result, "reference", &name));
	CU_ASSERT_STRING_EQUAL ("After Rename", name);
	xmmsv_unref (result);
}

CASE (test_client_find)
{
	xmms_medialib_entry_t entry;
	xmmsv_t *universe, *equals;
	xmmsv_t *result;
	const gchar *string;

	entry = xmms_mock_entry (medialib, 1, "Red Fang", "Red Fang", "Prehistoric Dog");

	/* should be found */
	universe = xmmsv_new_coll (XMMS_COLLECTION_TYPE_UNIVERSE);
	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_SAVE,
	                        xmmsv_new_string ("Test A"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS),
	                        xmmsv_ref (universe));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);
	xmmsv_unref (universe);

	/* should be found */
	universe = xmmsv_new_coll (XMMS_COLLECTION_TYPE_UNIVERSE);
	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_SAVE,
	                        xmmsv_new_string ("Test B"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS),
	                        xmmsv_ref (universe));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);
	xmmsv_unref (universe);

	/* should not be found */
	universe = xmmsv_new_coll (XMMS_COLLECTION_TYPE_UNIVERSE);
	equals = xmmsv_new_coll (XMMS_COLLECTION_TYPE_EQUALS);
	xmmsv_coll_attribute_set_string (equals, "type", "id");
	xmmsv_coll_attribute_set_string (equals, "value", "1337");
	xmmsv_coll_add_operand (equals, universe);
	xmmsv_unref (universe);

	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_SAVE,
	                        xmmsv_new_string ("Test C"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS),
	                        xmmsv_ref (equals));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);
	xmmsv_unref (equals);

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
	xmmsv_t *universe, *idlist;
	xmmsv_t *result;

	universe = xmmsv_new_coll (XMMS_COLLECTION_TYPE_UNIVERSE);
	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_SAVE,
	                        xmmsv_new_string ("Test A"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS),
	                        xmmsv_ref (universe));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);
	xmmsv_unref (universe);

	idlist = xmmsv_new_coll (XMMS_COLLECTION_TYPE_IDLIST);
	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_SAVE,
	                        xmmsv_new_string ("Test B"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_PLAYLISTS),
	                        xmmsv_ref (idlist));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);
	xmmsv_unref (idlist);

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
	xmmsv_t *universe, *ordered;
	xmmsv_t *expected, *result, *order, *fetch, *group;

	xmms_mock_entry (medialib, 1, "Red Fang", "Red Fang", "Prehistoric Dog");
	entry = xmms_mock_entry (medialib, 2, "Red Fang", "Red Fang", "Reverse Thunder");

	session = xmms_medialib_session_begin (medialib);
	xmms_medialib_entry_property_set_str_source (session, entry,
	                                             XMMS_MEDIALIB_ENTRY_PROPERTY_YEAR,
	                                             "2009", "client/unittest");
	xmms_medialib_session_commit (session);


	universe = xmmsv_new_coll (XMMS_COLLECTION_TYPE_UNIVERSE);

	order = xmmsv_build_list (XMMSV_LIST_ENTRY_STR ("artist"),
	                          XMMSV_LIST_ENTRY_STR ("album"),
	                          XMMSV_LIST_ENTRY_STR ("tracknr"),
	                          XMMSV_LIST_END);

	ordered = xmmsv_coll_add_order_operators (universe, order);
	xmmsv_unref (universe);
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
	                        ordered,
	                        xmmsv_new_int (0), xmmsv_new_int (0),
	                        fetch, group);

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
	xmmsv_t *universe, *ordered;
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

	universe = xmmsv_new_coll (XMMS_COLLECTION_TYPE_UNIVERSE);

	order = xmmsv_build_list (XMMSV_LIST_ENTRY_STR ("artist"),
	                          XMMSV_LIST_ENTRY_STR ("album"),
	                          XMMSV_LIST_END);

	ordered = xmmsv_coll_add_order_operators (universe, order);
	xmmsv_unref (universe);
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
	                        xmmsv_ref (ordered),
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
	                        xmmsv_ref (ordered),
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
	                        xmmsv_ref (ordered),
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
	xmmsv_unref (ordered);
}

CASE (test_reject_direct_cyclic_collections)
{
	xmmsv_t *reference, *result;

	/* To create a cycle the collection must already exist, check that it fails */
	reference = xmmsv_new_coll (XMMS_COLLECTION_TYPE_REFERENCE);
	xmmsv_coll_attribute_set_string (reference, "namespace", XMMS_COLLECTION_NS_COLLECTIONS);
	xmmsv_coll_attribute_set_string (reference, "reference", "Cycle");

	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_SAVE,
	                        xmmsv_new_string ("Cycle"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS),
	                        xmmsv_ref (reference));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_ERROR));

	xmmsv_unref (result);
	xmmsv_unref (reference);

	/* Create the non cyclic version and then later update it to be cyclic */
	reference = xmmsv_new_coll (XMMS_COLLECTION_TYPE_REFERENCE);
	xmmsv_coll_attribute_set_string (reference, "namespace", XMMS_COLLECTION_NS_COLLECTIONS);
	xmmsv_coll_attribute_set_string (reference, "reference", "All Media");

	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_SAVE,
	                        xmmsv_new_string ("Cycle"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS),
	                        xmmsv_ref (reference));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));

	xmmsv_unref (result);
	xmmsv_unref (reference);

	/* Overwrite the existing collection with a circular reference */
	reference = xmmsv_new_coll (XMMS_COLLECTION_TYPE_REFERENCE);
	xmmsv_coll_attribute_set_string (reference, "namespace", XMMS_COLLECTION_NS_COLLECTIONS);
	xmmsv_coll_attribute_set_string (reference, "reference", "Cycle");

	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_SAVE,
	                        xmmsv_new_string ("Cycle"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS),
	                        xmmsv_ref (reference));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_ERROR));

	xmmsv_unref (result);
	xmmsv_unref (reference);
}

CASE (test_reject_indirect_cyclic_collections)
{
	xmmsv_t *reference, *intersection, *result;

	/* Create a correct collection pointing to all media */
	reference = xmmsv_new_coll (XMMS_COLLECTION_TYPE_REFERENCE);
	xmmsv_coll_attribute_set_string (reference, "namespace", XMMS_COLLECTION_NS_COLLECTIONS);
	xmmsv_coll_attribute_set_string (reference, "reference", "All Media");

	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_SAVE,
	                        xmmsv_new_string ("First"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS),
	                        xmmsv_ref (reference));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));

	xmmsv_unref (result);
	xmmsv_unref (reference);

	/* Create a correct collection intersecting the first collection with all media */
	intersection = xmmsv_new_coll (XMMS_COLLECTION_TYPE_INTERSECTION);

	reference = xmmsv_new_coll (XMMS_COLLECTION_TYPE_REFERENCE);
	xmmsv_coll_attribute_set_string (reference, "namespace", XMMS_COLLECTION_NS_COLLECTIONS);
	xmmsv_coll_attribute_set_string (reference, "reference", "First");
	xmmsv_coll_add_operand (intersection, reference);
	xmmsv_unref (reference);

	reference = xmmsv_new_coll (XMMS_COLLECTION_TYPE_REFERENCE);
	xmmsv_coll_attribute_set_string (reference, "namespace", XMMS_COLLECTION_NS_COLLECTIONS);
	xmmsv_coll_attribute_set_string (reference, "reference", "All Media");
	xmmsv_coll_add_operand (intersection, reference);
	xmmsv_unref (reference);

	/* Overwrite the existing collection with a circular reference */
	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_SAVE,
	                        xmmsv_new_string ("Second"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS),
	                        xmmsv_ref (intersection));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));

	xmmsv_unref (result);
	xmmsv_unref (intersection);

	/* Update the first collection to point to the second collection thus creating
	 * an indirect cyclic graph.. which should be rejected */
	reference = xmmsv_new_coll (XMMS_COLLECTION_TYPE_REFERENCE);
	xmmsv_coll_attribute_set_string (reference, "namespace", XMMS_COLLECTION_NS_COLLECTIONS);
	xmmsv_coll_attribute_set_string (reference, "reference", "Second");

	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_SAVE,
	                        xmmsv_new_string ("First"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS),
	                        xmmsv_ref (reference));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_ERROR));

	xmmsv_unref (result);
	xmmsv_unref (reference);
}

CASE (test_references)
{
	xmmsv_t *universe, *reference, *match;
	xmmsv_t *result;

	reference = xmmsv_new_coll (XMMS_COLLECTION_TYPE_REFERENCE);
	xmmsv_coll_attribute_set_string (reference, "namespace", XMMS_COLLECTION_NS_COLLECTIONS);
	xmmsv_coll_attribute_set_string (reference, "reference", "All Media");

	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_SAVE,
	                        xmmsv_new_string ("Cycle"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS),
	                        xmmsv_ref (reference));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);
	xmmsv_unref (reference);

	universe = xmmsv_new_coll (XMMS_COLLECTION_TYPE_UNIVERSE);

	match = xmmsv_new_coll (XMMS_COLLECTION_TYPE_MATCH);
	xmmsv_coll_attribute_set_string (match, "field", "artist");
	xmmsv_coll_attribute_set_string (match, "value", "The Doors");
	xmmsv_coll_add_operand (match, universe);

	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_SAVE,
	                        xmmsv_new_string ("The Doors"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS),
	                        xmmsv_ref (match));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));

	xmmsv_unref (result);

	reference = xmmsv_new_coll (XMMS_COLLECTION_TYPE_REFERENCE);
	xmmsv_coll_attribute_set_string (reference, "namespace", XMMS_COLLECTION_NS_COLLECTIONS);
	xmmsv_coll_attribute_set_string (reference, "reference", "The Doors");

	result = XMMS_IPC_CALL (dag, XMMS_IPC_CMD_COLLECTION_SAVE,
	                        xmmsv_new_string ("Test Reference"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_COLLECTIONS),
	                        xmmsv_ref (reference));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);

	xmmsv_unref (reference);
	xmmsv_unref (universe);
	xmmsv_unref (match);
}

CASE (test_collection_snapshot_restore)
{
	xmmsv_t *universe, *playlist, *techno, *reference, *_union, *collections, *playlists, *snapshot, *result, *expected;

	universe = xmmsv_new_coll (XMMS_COLLECTION_TYPE_UNIVERSE);

	playlist = xmmsv_new_coll (XMMS_COLLECTION_TYPE_IDLIST);
	xmmsv_coll_idlist_append (playlist, 1);
	xmmsv_coll_idlist_append (playlist, 2);
	xmmsv_coll_idlist_append (playlist, 3);

	techno = xmmsv_new_coll (XMMS_COLLECTION_TYPE_MATCH);
	xmmsv_coll_add_operand (techno, universe);
	xmmsv_coll_attribute_set_string (techno, "type", "value");
	xmmsv_coll_attribute_set_string (techno, "field", "genre");
	xmmsv_coll_attribute_set_string (techno, "value", "techno");
	xmmsv_unref (universe);

	reference = xmmsv_new_coll (XMMS_COLLECTION_TYPE_REFERENCE);
	xmmsv_coll_attribute_set_string (reference, "namespace", XMMS_COLLECTION_NS_PLAYLISTS);
	xmmsv_coll_attribute_set_string (reference, "reference", "Test Playlist");

	_union = xmmsv_new_coll (XMMS_COLLECTION_TYPE_UNION);
	xmmsv_coll_add_operand (_union, techno);
	xmmsv_coll_add_operand (_union, reference);
	xmmsv_unref (techno);
	xmmsv_unref (reference);

	collections = xmmsv_new_dict ();
	xmmsv_dict_set (collections, "Test Collection", _union);
	xmmsv_unref (_union);

	playlists = xmmsv_new_dict ();
	xmmsv_dict_set (playlists, "Test Playlist", playlist);
	xmmsv_unref (playlist);

	expected = xmmsv_new_dict ();
	xmmsv_dict_set (expected, "collections", collections);
	xmmsv_unref (collections);
	xmmsv_dict_set (expected, "playlists", playlists);
	xmmsv_unref (playlists);
	xmmsv_dict_set_string (expected, "active-playlist", "Test Playlist");

	snapshot = xmmsv_copy (expected);

	xmms_collection_restore (dag, snapshot);
	xmmsv_unref (snapshot);

	result = xmms_collection_snapshot (dag);

	CU_ASSERT (xmmsv_compare (expected, result));

	xmmsv_unref (result);
	xmmsv_unref (expected);
}
