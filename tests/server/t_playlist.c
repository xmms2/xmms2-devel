#include "xcu.h"

#include "xmmspriv/xmms_log.h"
#include "xmmspriv/xmms_ipc.h"
#include "xmmspriv/xmms_config.h"
#include "xmmspriv/xmms_medialib.h"
#include "xmmspriv/xmms_collection.h"
#include "xmmspriv/xmms_playlist.h"
#include "xmmspriv/xmms_playlist_updater.h"

#include "utils/jsonism.h"
#include "utils/value_utils.h"
#include "utils/coll_utils.h"
#include "utils/ipc_call.h"
#include "utils/mlib_utils.h"

static xmms_medialib_t *medialib;
static xmms_coll_dag_t *colldag;
static xmms_playlist_t *playlist;

static void
setup_default_playlist (void)
{
	xmmsv_coll_t *coll;

	coll = xmmsv_coll_new (XMMS_COLLECTION_TYPE_IDLIST);

	xmms_collection_dag_replace (colldag, XMMS_COLLECTION_NSID_PLAYLISTS,
	                             "Default", coll);
	xmmsv_coll_ref (coll);

	xmms_collection_dag_replace (colldag, XMMS_COLLECTION_NSID_PLAYLISTS,
	                             XMMS_ACTIVE_PLAYLIST, coll);
}

SETUP (mlib) {
	g_thread_init (0);

	xmms_ipc_init ();
	xmms_log_init (0);

	xmms_config_init (NULL);

	xmms_config_property_register ("medialib.path", "memory://", NULL, NULL);
	xmms_config_property_register ("playlist.repeat_one", "0", NULL, NULL);
	xmms_config_property_register ("playlist.repeat_all", "0", NULL, NULL);

	medialib = xmms_medialib_init ();
	colldag = xmms_collection_init (medialib);

	setup_default_playlist ();

	playlist = xmms_playlist_init (medialib, colldag);

	return 0;
}

CLEANUP () {
	xmms_object_unref (playlist);
	xmms_object_unref (colldag);
	xmms_object_unref (medialib);
	xmms_config_shutdown ();
	xmms_ipc_shutdown ();

	return 0;
}

CASE (test_basic_functionality)
{
	xmms_medialib_session_t *session;
	xmms_error_t err;
	xmms_future_t *future;
	gint first, second, result;
	xmmsv_t *signals, *expected;

	session = xmms_medialib_session_begin (medialib);

	xmms_medialib_entry_new_encoded (session, "dummy", &err);

	first = xmms_medialib_entry_new_encoded (session, "1", &err);
	second = xmms_medialib_entry_new_encoded (session, "2", &err);

	xmms_medialib_session_commit (session);

	future = XMMS_IPC_CHECK_SIGNAL (playlist, XMMS_IPC_SIGNAL_PLAYLIST_CHANGED);

	/* emits CHANGED_UPDATE, and CHANGED_ADD */
	xmms_playlist_add_entry (playlist, XMMS_ACTIVE_PLAYLIST, first, &err);
	/* emits CHANGED_UPDATE, and CHANGED_ADD */
	xmms_playlist_add_entry (playlist, XMMS_ACTIVE_PLAYLIST, second, &err);

	signals = xmms_future_await (future, 4);

	/* XMMS_PLAYLIST_CHANGED_ADD = 0, XMMS_PLAYLIST_CHANGED_UPDATE = 7 */
	expected = xmmsv_from_json ("[{                         'type': 7, 'name': 'Default' },"
	                            " { 'position': 0, 'id': 2, 'type': 0, 'name': 'Default' },"
	                            " {                         'type': 7, 'name': 'Default' },"
	                            " { 'position': 1, 'id': 3, 'type': 0, 'name': 'Default' }]");

	CU_ASSERT (xmmsv_compare (expected, signals));
	xmmsv_unref (signals);
	xmmsv_unref (expected);
	xmms_future_free (future);

	future = XMMS_IPC_CHECK_SIGNAL (playlist, XMMS_IPC_SIGNAL_PLAYLIST_CURRENT_POS);

	/* emit CURRPOS_MSG pointing at first position as last position was -1 */
	result = xmms_playlist_current_entry (playlist);
	CU_ASSERT_EQUAL (first, result);

	/* emit CURRPOS_MSG -> moves to second entry */
	result = xmms_playlist_advance (playlist);
	CU_ASSERT_EQUAL (TRUE, result);

	result = xmms_playlist_current_entry (playlist);
	CU_ASSERT_EQUAL (second, result);

	/* emit CURRPOS_MSG -> hits end of list, back to first position */
	result = xmms_playlist_advance (playlist);
	CU_ASSERT_EQUAL (FALSE, result);

	result = xmms_playlist_current_entry (playlist);
	CU_ASSERT_EQUAL (first, result);

	signals = xmms_future_await (future, 3);

	expected = xmmsv_from_json ("[{ 'position': 0, 'name': 'Default' },"
	                            " { 'position': 1, 'name': 'Default' },"
	                            " { 'position': 0, 'name': 'Default' }]");

	CU_ASSERT (xmmsv_compare (expected, signals));
	xmmsv_unref (signals);
	xmmsv_unref (expected);
	xmms_future_free (future);
}

CASE(test_repeat)
{
	xmms_medialib_entry_t first, second;
	xmms_config_property_t *property;
	xmms_error_t err;

	first  = xmms_mock_entry (medialib, 1, "Red Fang", "Red Fang", "Prehistoric Dog");
	second = xmms_mock_entry (medialib, 2, "Red Fang", "Red Fang", "Reverse Thunder");

	xmms_playlist_add_entry (playlist, XMMS_ACTIVE_PLAYLIST, first, &err);
	xmms_playlist_add_entry (playlist, XMMS_ACTIVE_PLAYLIST, second, &err);

	property = xmms_config_lookup ("playlist.repeat_one");
	xmms_config_property_set_data (property, "1");

	CU_ASSERT_TRUE (xmms_playlist_advance (playlist));
	CU_ASSERT_TRUE (xmms_playlist_advance (playlist));
	CU_ASSERT_TRUE (xmms_playlist_advance (playlist));
	CU_ASSERT_EQUAL (first, xmms_playlist_current_entry (playlist));

	property = xmms_config_lookup ("playlist.repeat_one");
	xmms_config_property_set_data (property, "0");

	property = xmms_config_lookup ("playlist.repeat_all");
	xmms_config_property_set_data (property, "1");

	CU_ASSERT_TRUE (xmms_playlist_advance (playlist));
	CU_ASSERT_EQUAL (second, xmms_playlist_current_entry (playlist));
	CU_ASSERT_TRUE (xmms_playlist_advance (playlist));
	CU_ASSERT_EQUAL (first, xmms_playlist_current_entry (playlist));

	property = xmms_config_lookup ("playlist.repeat_all");
	xmms_config_property_set_data (property, "0");
}

CASE(test_medialib_remove)
{
	xmms_medialib_entry_t first, second, entry;
	xmms_medialib_session_t *session;
	xmms_error_t err;
	xmmsv_t *result, *expected;
	xmms_future_t *future1, *future2, *future3;

	first  = xmms_mock_entry (medialib, 1, "Red Fang", "Red Fang", "Prehistoric Dog");
	second = xmms_mock_entry (medialib, 2, "Red Fang", "Red Fang", "Reverse Thunder");

	xmms_playlist_add_entry (playlist, XMMS_ACTIVE_PLAYLIST, first, &err);
	xmms_playlist_add_entry (playlist, XMMS_ACTIVE_PLAYLIST, second, &err);

	/* position is now 0 */
	xmms_playlist_advance (playlist);

	future1 = XMMS_IPC_CHECK_SIGNAL (playlist, XMMS_IPC_SIGNAL_PLAYLIST_CHANGED);
	future2 = XMMS_IPC_CHECK_SIGNAL (playlist, XMMS_IPC_SIGNAL_PLAYLIST_CURRENT_POS);
	future3 = XMMS_IPC_CHECK_SIGNAL (medialib, XMMS_IPC_SIGNAL_MEDIALIB_ENTRY_REMOVED);

	session = xmms_medialib_session_begin (medialib);
	xmms_medialib_entry_remove (session, first);
	xmms_medialib_session_commit (session);

	result = xmms_future_await (future1, 2);
	expected = xmmsv_from_json ("[{                'type': 7, 'name': 'Default' },"
	                            " { 'position': 0, 'type': 3, 'name': 'Default' }]");
	CU_ASSERT (xmmsv_compare (expected, result));
	xmmsv_unref (result);
	xmmsv_unref (expected);

	result = xmms_future_await (future2, 1);
	expected = xmmsv_from_json ("[{ 'position': 0, 'name': 'Default' }]");
	CU_ASSERT (xmmsv_compare (expected, result));
	xmmsv_unref (result);
	xmmsv_unref (expected);

	result = xmms_future_await (future3, 1);
	CU_ASSERT_TRUE (xmmsv_list_get_int (result, 0, &entry));
	CU_ASSERT_EQUAL (first, entry);
	xmmsv_unref (result);

	xmms_future_free (future1);
	xmms_future_free (future2);
	xmms_future_free (future3);
}

CASE(test_client_add_collection)
{
	xmmsv_coll_t *universe;
	xmmsv_t *result, *order;

	xmms_mock_entry (medialib, 1, "Red Fang", "Red Fang", "Prehistoric Dog");
	xmms_mock_entry (medialib, 2, "Red Fang", "Red Fang", "Reverse Thunder");

	universe = xmmsv_coll_new (XMMS_COLLECTION_TYPE_UNIVERSE);

	order = xmmsv_build_list (XMMSV_LIST_ENTRY_STR ("artist"),
	                          XMMSV_LIST_ENTRY_STR ("album"),
	                          XMMSV_LIST_ENTRY_STR ("tracknr"),
	                          XMMSV_LIST_END);

	result = XMMS_IPC_CALL (playlist, XMMS_IPC_CMD_ADD_COLL,
	                        xmmsv_new_string ("Default"),
	                        xmmsv_new_coll (universe),
	                        order);
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);

	result = XMMS_IPC_CALL (playlist, XMMS_IPC_CMD_LIST,
	                        xmmsv_new_string ("Default"));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_LIST));
	CU_ASSERT_EQUAL (2, xmmsv_list_get_size (result));
	xmmsv_unref (result);

	xmmsv_coll_unref (universe);
}

CASE(test_client_add_idlist)
{
	xmms_medialib_entry_t first, second;
	xmmsv_coll_t *list;
	xmmsv_t *result;

	first = xmms_mock_entry (medialib, 1, "Red Fang", "Red Fang", "Prehistoric Dog");
	second = xmms_mock_entry (medialib, 2, "Red Fang", "Red Fang", "Reverse Thunder");

	list = xmmsv_coll_new (XMMS_COLLECTION_TYPE_IDLIST);
	xmmsv_coll_idlist_append (list, first);
	xmmsv_coll_idlist_append (list, second);

	result = XMMS_IPC_CALL (playlist, XMMS_IPC_CMD_ADD_IDLIST,
	                        xmmsv_new_string ("Default"),
	                        xmmsv_new_coll (list));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);

	result = XMMS_IPC_CALL (playlist, XMMS_IPC_CMD_LIST,
	                        xmmsv_new_string ("Default"));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_LIST));
	CU_ASSERT_EQUAL (2, xmmsv_list_get_size (result));
	xmmsv_unref (result);

	xmmsv_coll_unref (list);
}

CASE(test_client_add_id)
{
	xmms_medialib_entry_t first;
	xmmsv_t *result;

	first = xmms_mock_entry (medialib, 1, "Red Fang", "Red Fang", "Prehistoric Dog");

	result = XMMS_IPC_CALL (playlist, XMMS_IPC_CMD_ADD_ID,
	                        xmmsv_new_string ("Default"),
	                        xmmsv_new_int (first));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);

	result = XMMS_IPC_CALL (playlist, XMMS_IPC_CMD_LIST,
	                        xmmsv_new_string ("Default"));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_LIST));
	CU_ASSERT_EQUAL (1, xmmsv_list_get_size (result));
	xmmsv_unref (result);
}

CASE(test_client_add_url)
{
	xmmsv_t *result;

	result = XMMS_IPC_CALL (playlist, XMMS_IPC_CMD_ADD_URL,
	                        xmmsv_new_string ("Default"),
	                        xmmsv_new_string ("file:///test/file.mp3"));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);

	result = XMMS_IPC_CALL (playlist, XMMS_IPC_CMD_LIST,
	                        xmmsv_new_string ("Default"));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_LIST));
	CU_ASSERT_EQUAL (1, xmmsv_list_get_size (result));
	xmmsv_unref (result);
}

CASE(test_client_clear)
{
	xmms_medialib_entry_t first;
	xmmsv_t *result;

	first = xmms_mock_entry (medialib, 1, "Red Fang", "Red Fang", "Prehistoric Dog");

	result = XMMS_IPC_CALL (playlist, XMMS_IPC_CMD_LIST,
	                        xmmsv_new_string ("Default"));
	/* TODO: Is this what we want? */
	CU_ASSERT_PTR_NULL (result);

	result = XMMS_IPC_CALL (playlist, XMMS_IPC_CMD_ADD_ID,
	                        xmmsv_new_string ("Default"),
	                        xmmsv_new_int (first));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);

	result = XMMS_IPC_CALL (playlist, XMMS_IPC_CMD_CLEAR,
	                        xmmsv_new_string ("Default"));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);

	result = XMMS_IPC_CALL (playlist, XMMS_IPC_CMD_LIST,
	                        xmmsv_new_string ("Default"));
	/* TODO: Is this what we want? */
	CU_ASSERT_PTR_NULL (result);

	result = XMMS_IPC_CALL (playlist, XMMS_IPC_CMD_ADD_ID,
	                        xmmsv_new_string ("Default"),
	                        xmmsv_new_int (first));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);

	result = XMMS_IPC_CALL (playlist, XMMS_IPC_CMD_LIST,
	                        xmmsv_new_string ("Default"));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_LIST));
	CU_ASSERT_EQUAL (1, xmmsv_list_get_size (result));
	xmmsv_unref (result);
}

CASE(test_client_insert_collection)
{
}

CASE(test_client_insert_id)
{
}

CASE(test_client_insert_url)
{
}

CASE(test_client_load)
{
}

CASE(test_client_move_entry)
{
	xmms_medialib_entry_t first, second, third;
	xmmsv_t *result, *expected;
	xmms_error_t err;

	first = xmms_mock_entry (medialib, 1, "Red Fang", "Red Fang", "Prehistoric Dog");
	second = xmms_mock_entry (medialib, 2, "Red Fang", "Red Fang", "Reverse Thunder");
	third = xmms_mock_entry (medialib, 3, "Red Fang", "Red Fang", "Night Destroyer");

	xmms_playlist_add_entry (playlist, XMMS_ACTIVE_PLAYLIST, first, &err);
	xmms_playlist_add_entry (playlist, XMMS_ACTIVE_PLAYLIST, second, &err);
	xmms_playlist_add_entry (playlist, XMMS_ACTIVE_PLAYLIST, third, &err);

	result = XMMS_IPC_CALL (playlist, XMMS_IPC_CMD_LIST,
	                        xmmsv_new_string ("Default"));
	expected = xmmsv_from_json ("[1, 2, 3]");
	CU_ASSERT (xmmsv_compare (expected, result));
	xmmsv_unref (result);
	xmmsv_unref (expected);

	result = XMMS_IPC_CALL (playlist, XMMS_IPC_CMD_MOVE_ENTRY,
	                        xmmsv_new_string ("Default"),
	                        xmmsv_new_int (2),
	                        xmmsv_new_int (0));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);

	result = XMMS_IPC_CALL (playlist, XMMS_IPC_CMD_LIST,
	                        xmmsv_new_string ("Default"));
	expected = xmmsv_from_json ("[3, 1, 2]");
	CU_ASSERT (xmmsv_compare (expected, result));
	xmmsv_unref (result);
	xmmsv_unref (expected);
}

CASE(test_client_remove_entry)
{
	xmms_medialib_entry_t first;
	xmmsv_t *result;

	first = xmms_mock_entry (medialib, 1, "Red Fang", "Red Fang", "Prehistoric Dog");

	result = XMMS_IPC_CALL (playlist, XMMS_IPC_CMD_ADD_ID,
	                        xmmsv_new_string ("Default"),
	                        xmmsv_new_int (first));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);

	result = XMMS_IPC_CALL (playlist, XMMS_IPC_CMD_LIST,
	                        xmmsv_new_string ("Default"));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_LIST));
	CU_ASSERT_EQUAL (1, xmmsv_list_get_size (result));
	xmmsv_unref (result);

	result = XMMS_IPC_CALL (playlist, XMMS_IPC_CMD_REMOVE_ENTRY,
	                        xmmsv_new_string ("Default"),
	                        xmmsv_new_int (0));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);

	result = XMMS_IPC_CALL (playlist, XMMS_IPC_CMD_REMOVE_ENTRY,
	                        xmmsv_new_string ("Default"),
	                        xmmsv_new_int (0));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_ERROR));
	xmmsv_unref (result);

	result = XMMS_IPC_CALL (playlist, XMMS_IPC_CMD_LIST,
	                        xmmsv_new_string ("Default"));
	/* TODO: Is this what we want? */
	CU_ASSERT_PTR_NULL (result);
}

CASE(test_client_rinsert)
{
}

CASE(test_client_shuffle)
{
	xmms_medialib_entry_t first, second;
	xmms_future_t *future1, *future2;
	xmmsv_t *result, *signals, *expected;
	xmms_error_t err;

	first = xmms_mock_entry (medialib, 1, "Red Fang", "Red Fang", "Prehistoric Dog");
	second = xmms_mock_entry (medialib, 2, "Red Fang", "Red Fang", "Reverse Thunder");

	xmms_playlist_add_entry (playlist, XMMS_ACTIVE_PLAYLIST, first, &err);
	xmms_playlist_add_entry (playlist, XMMS_ACTIVE_PLAYLIST, second, &err);

	/* advance position from -1 to 0, and from 0 to 1 */
	xmms_playlist_advance (playlist);
	xmms_playlist_advance (playlist);
	CU_ASSERT_EQUAL (second, xmms_playlist_current_entry (playlist));

	future1 = XMMS_IPC_CHECK_SIGNAL (playlist, XMMS_IPC_SIGNAL_PLAYLIST_CHANGED);
	future2 = XMMS_IPC_CHECK_SIGNAL (playlist, XMMS_IPC_SIGNAL_PLAYLIST_CURRENT_POS);

	/* emits CHANGED_UPDATE, CHANGED_SHUFFLE, and CURRENT_POS */
	result = XMMS_IPC_CALL (playlist, XMMS_IPC_CMD_SHUFFLE,
	                        xmmsv_new_string ("Default"));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);

	signals = xmms_future_await (future1, 2);

	/* XMMS_PLAYLIST_CHANGED_UPDATE = 7, XMMS_PLAYLIST_CHANGED_SHUFFLE = 2 */
	expected = xmmsv_from_json ("[{ 'type': 7, 'name': 'Default' },"
	                            " { 'type': 2, 'name': 'Default' }]");
	CU_ASSERT (xmmsv_compare (expected, signals));
	xmmsv_unref (signals);
	xmmsv_unref (expected);

	xmms_future_free (future1);

	signals = xmms_future_await (future2, 1);

	/* current entry is moved to position 0 of the playlist */
	expected = xmmsv_from_json ("[{ 'position': 0, 'name': 'Default' }]");
	CU_ASSERT (xmmsv_compare (expected, signals));
	xmmsv_unref (signals);
	xmmsv_unref (expected);

	xmms_future_free (future2);

	/* verify that the current entry is still the same */
	CU_ASSERT_EQUAL (second, xmms_playlist_current_entry (playlist));
}

CASE(test_client_sort)
{
	xmms_medialib_entry_t first, second, third, fourth, current, entry;
	xmmsv_t *order, *result;
	xmms_error_t err;

	result = XMMS_IPC_CALL (playlist, XMMS_IPC_CMD_SORT,
	                        xmmsv_new_string ("Default"),
	                        xmmsv_new_list ());
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_ERROR));
	xmmsv_unref (result);

	first  = xmms_mock_entry (medialib, 1, "Red Fang", "Red Fang", "Prehistoric Dog");
	second = xmms_mock_entry (medialib, 2, "Red Fang", "Red Fang", "Reverse Thunder");
	third  = xmms_mock_entry (medialib, 1, "Vibrasphere", "Lungs for Life", "Decade");
	fourth = xmms_mock_entry (medialib, 2, "Vibrasphere", "Lungs for Life", "Breathing Place");

	xmms_playlist_add_entry (playlist, XMMS_ACTIVE_PLAYLIST, fourth, &err); // Vibrasphere Track 2
	xmms_playlist_add_entry (playlist, XMMS_ACTIVE_PLAYLIST, second, &err); // Red Fang Track 2
	xmms_playlist_add_entry (playlist, XMMS_ACTIVE_PLAYLIST, third, &err);  // Vibrasphere Track 1
	xmms_playlist_add_entry (playlist, XMMS_ACTIVE_PLAYLIST, first, &err);  // Red Fang Track 1
	xmms_playlist_add_entry (playlist, XMMS_ACTIVE_PLAYLIST, second, &err); // Red Fang Track 2

	result = XMMS_IPC_CALL (playlist, XMMS_IPC_CMD_LIST,
	                        xmmsv_new_string ("Default"));
	CU_ASSERT_EQUAL (5, xmmsv_list_get_size (result));
	xmmsv_unref (result);

	current = xmms_playlist_current_entry (playlist);

	order = xmmsv_build_list (XMMSV_LIST_ENTRY_STR ("artist"),
	                          XMMSV_LIST_ENTRY_STR ("album"),
	                          XMMSV_LIST_ENTRY_STR ("tracknr"),
	                          XMMSV_LIST_END);

	result = XMMS_IPC_CALL (playlist, XMMS_IPC_CMD_SORT,
	                        xmmsv_new_string ("Default"),
	                        order);
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);

	result = XMMS_IPC_CALL (playlist, XMMS_IPC_CMD_LIST,
	                        xmmsv_new_string ("Default"));
	CU_ASSERT_EQUAL (5, xmmsv_list_get_size (result));
	CU_ASSERT_TRUE (xmmsv_list_get_int (result, 0, &entry));
	CU_ASSERT_EQUAL (first, entry);
	CU_ASSERT_TRUE (xmmsv_list_get_int (result, 1, &entry));
	CU_ASSERT_EQUAL (second, entry);
	CU_ASSERT_TRUE (xmmsv_list_get_int (result, 2, &entry));
	CU_ASSERT_EQUAL (second, entry);
	CU_ASSERT_TRUE (xmmsv_list_get_int (result, 3, &entry));
	CU_ASSERT_EQUAL (third, entry);
	CU_ASSERT_TRUE (xmmsv_list_get_int (result, 4, &entry));
	CU_ASSERT_EQUAL (fourth, entry);
	xmmsv_unref (result);
}


/**
 * Party Shuffle should work like the following:
 *
 * [3, 2]        // -1 is currpos, 3 and 2 are upcoming.
 * [3, 2, 4]     //  3 is currpos, 2, 4 are upcoming.
 * [3, 2, 4, 4]  //  3 is history, 3 is currpos, 4, 4 are upcoming.
 * [2, 4, 4, 2]  //  2 is history, 4 is currpos, 4, 2 are upcoming.
 */
CASE(test_party_shuffle)
{
	xmms_playlist_updater_t *updater;
	xmmsv_t *result, *expected, *signal;
	xmmsv_coll_t *coll, *universe;
	xmms_future_t *future;
	gint type, current, entry;
	gint first_upcoming, second_upcoming, third_upcoming, fourth_upcoming;

	xmms_mock_entry (medialib, 1, "Red Fang", "Red Fang", "Prehistoric Dog");
	xmms_mock_entry (medialib, 2, "Red Fang", "Red Fang", "Reverse Thunder");
	xmms_mock_entry (medialib, 3, "Red Fang", "Red Fang", "Night Destroyer");
	xmms_mock_entry (medialib, 4, "Red Fang", "Red Fang", "Human Remain Human Remains");

	future = XMMS_IPC_CHECK_SIGNAL (playlist, XMMS_IPC_SIGNAL_PLAYLIST_CHANGED);

	updater = xmms_playlist_updater_init (playlist);

	/* reconfigure the active playlist as party shuffle */
	coll = xmmsv_coll_new (XMMS_COLLECTION_TYPE_IDLIST);
	xmmsv_coll_attribute_set (coll, "type", "pshuffle");
	xmmsv_coll_attribute_set (coll, "history", "1");
	xmmsv_coll_attribute_set (coll, "upcoming", "2");

	universe = xmmsv_coll_new (XMMS_COLLECTION_TYPE_UNIVERSE);
	xmmsv_coll_add_operand (coll, universe);
	xmmsv_coll_unref (universe);

	result = XMMS_IPC_CALL (colldag, XMMS_IPC_CMD_COLLECTION_SAVE,
	                        xmmsv_new_string ("Default"),
	                        xmmsv_new_string (XMMS_COLLECTION_NS_PLAYLISTS),
	                        xmmsv_new_coll (coll));
	CU_ASSERT (xmmsv_is_type (result, XMMSV_TYPE_NONE));
	xmmsv_unref (result);
	xmmsv_coll_unref (coll);

	/* saving the collection to 'Default' and '_active' */
	result = xmms_future_await (future, 2);
	expected = xmmsv_from_json ("[{ 'type': 7, 'name': 'Default' },"
	                            " { 'type': 7, 'name': 'Default' }]");
	CU_ASSERT (xmmsv_compare (expected, result));
	xmmsv_unref (result);
	xmmsv_unref (expected);

	/* adding the two 'upcoming' tracks to the playlist, 'id' is random thus the verbosity */
	result = xmms_future_await (future, 4);
	CU_ASSERT (xmmsv_list_get (result, 0, &signal));
	CU_ASSERT (xmmsv_dict_entry_get_int (signal, "type", &type));
	CU_ASSERT_EQUAL (XMMS_PLAYLIST_CHANGED_UPDATE, type);
	CU_ASSERT (xmmsv_list_get (result, 1, &signal));
	CU_ASSERT (xmmsv_dict_entry_get_int (signal, "type", &type));
	CU_ASSERT_EQUAL (XMMS_PLAYLIST_CHANGED_ADD, type);

	CU_ASSERT (xmmsv_list_get (result, 2, &signal));
	CU_ASSERT (xmmsv_dict_entry_get_int (signal, "type", &type));
	CU_ASSERT_EQUAL (XMMS_PLAYLIST_CHANGED_UPDATE, type);
	CU_ASSERT (xmmsv_list_get (result, 3, &signal));
	CU_ASSERT (xmmsv_dict_entry_get_int (signal, "type", &type));
	CU_ASSERT_EQUAL (XMMS_PLAYLIST_CHANGED_ADD, type);
	xmmsv_unref (result);

	result = XMMS_IPC_CALL (playlist, XMMS_IPC_CMD_LIST,
	                        xmmsv_new_string ("Default"));
	CU_ASSERT (xmmsv_list_get_int (result, 0, &first_upcoming));
	CU_ASSERT (xmmsv_list_get_int (result, 1, &second_upcoming));
	xmmsv_unref (result);

	/* position moves to 0, which should result in a new upcoming */
	xmms_playlist_advance (playlist);

	current = xmms_playlist_current_entry (playlist);
	CU_ASSERT_EQUAL (first_upcoming, current);

	/* _UPDATE and _ADD will be emited when the new entry has been added */
	result = xmms_future_await (future, 2);
	xmmsv_unref (result);

	result = XMMS_IPC_CALL (playlist, XMMS_IPC_CMD_LIST,
	                        xmmsv_new_string ("Default"));
	CU_ASSERT_EQUAL (3, xmmsv_list_get_size (result));
	CU_ASSERT (xmmsv_list_get_int (result, 2, &third_upcoming));
	xmmsv_unref (result);

	/* after this advance there should be at least one entry of each type:
	 * 1x history.
	 * 1x current position.
	 * 2x upcoming.
	 */
	xmms_playlist_advance (playlist);

	/* _UPDATE and _ADD will be emited when the new entry has been added */
	result = xmms_future_await (future, 2);
	xmmsv_unref (result);

	current = xmms_playlist_current_entry (playlist);
	CU_ASSERT_EQUAL (second_upcoming, current);

	result = XMMS_IPC_CALL (playlist, XMMS_IPC_CMD_LIST,
	                        xmmsv_new_string ("Default"));
	CU_ASSERT_EQUAL (4, xmmsv_list_get_size (result));
	CU_ASSERT (xmmsv_list_get_int (result, 0, &entry));
	CU_ASSERT_EQUAL (first_upcoming, entry);
	CU_ASSERT (xmmsv_list_get_int (result, 1, &entry));
	CU_ASSERT_EQUAL (second_upcoming, entry);
	CU_ASSERT (xmmsv_list_get_int (result, 2, &entry));
	CU_ASSERT_EQUAL (third_upcoming, entry);
	CU_ASSERT (xmmsv_list_get_int (result, 3, &fourth_upcoming));
	xmmsv_unref (result);

	xmms_playlist_advance (playlist);

	/* _UPDATE and _ADD will be emited when the new entry has been added */
	result = xmms_future_await (future, 2);
	xmmsv_unref (result);

	/* verify that the list is the same size, and the history item has changed */
	result = XMMS_IPC_CALL (playlist, XMMS_IPC_CMD_LIST,
	                        xmmsv_new_string ("Default"));
	CU_ASSERT_EQUAL (4, xmmsv_list_get_size (result));
	CU_ASSERT (xmmsv_list_get_int (result, 0, &entry));
	CU_ASSERT_EQUAL (second_upcoming, entry);
	CU_ASSERT (xmmsv_list_get_int (result, 1, &entry));
	CU_ASSERT_EQUAL (third_upcoming, entry);
	CU_ASSERT (xmmsv_list_get_int (result, 2, &entry));
	CU_ASSERT_EQUAL (fourth_upcoming, entry);
	xmmsv_unref (result);

	xmms_object_unref (updater);

	xmms_future_free (future);
}
