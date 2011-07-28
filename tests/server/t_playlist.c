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
	gint first, second, result;

	session = xmms_medialib_session_begin (medialib);

	xmms_medialib_entry_new_encoded (session, "dummy", &err);

	first = xmms_medialib_entry_new_encoded (session, "1", &err);
	second = xmms_medialib_entry_new_encoded (session, "2", &err);

	xmms_medialib_session_commit (session);

	xmms_playlist_add_entry (playlist, XMMS_ACTIVE_PLAYLIST, first, &err);
	xmms_playlist_add_entry (playlist, XMMS_ACTIVE_PLAYLIST, second, &err);

	result = xmms_playlist_current_entry (playlist);
	CU_ASSERT_EQUAL (first, result);

	result = xmms_playlist_advance (playlist);
	CU_ASSERT_EQUAL (TRUE, result);

	result = xmms_playlist_current_entry (playlist);
	CU_ASSERT_EQUAL (second, result);

	result = xmms_playlist_advance (playlist);
	CU_ASSERT_EQUAL (FALSE, result);

	result = xmms_playlist_current_entry (playlist);
	CU_ASSERT_EQUAL (first, result);
}
