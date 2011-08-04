#include "xcu.h"

#include "xmmspriv/xmms_log.h"
#include "xmmspriv/xmms_ipc.h"
#include "xmmspriv/xmms_config.h"
#include "xmmspriv/xmms_medialib.h"
#include "xmmspriv/xmms_collection.h"
#include "xmmspriv/xmms_playlist.h"

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

	signals = xmms_future_await_many (future, 4);

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

	signals = xmms_future_await_many (future, 3);

	expected = xmmsv_from_json ("[{ 'position': 0, 'name': 'Default' },"
	                            " { 'position': 1, 'name': 'Default' },"
	                            " { 'position': 0, 'name': 'Default' }]");

	CU_ASSERT (xmmsv_compare (expected, signals));
	xmmsv_unref (signals);
	xmmsv_unref (expected);
	xmms_future_free (future);
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
}

CASE(test_client_add_url)
{
}

CASE(test_client_clear)
{
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
}

CASE(test_client_radd)
{
}

CASE(test_client_remove_entry)
{
}

CASE(test_client_rinsert)
{
}

CASE(test_client_shuffle)
{
}

CASE(test_client_sort)
{
}
