#include "xcu.h"

/* nasty hack to make the hack required in waf less nasty */
#include "../src/xmms/error.c"
#include "../src/xmms/utils.c"
#include "../src/xmms/ipc.c"
#include "../src/xmms/object.c"
#include "../src/xmms/config.c"
#include "../src/xmms/medialib.c"
#include "../src/xmms/fetchinfo.c"
#include "../src/xmms/fetchspec.c"

#include "utils/jsonism.h"
#include "utils/value_utils.h"
#include "utils/coll_utils.h"

static void xmms_mock_entry (gint tracknr, const gchar *artist,
                             const gchar *album, const gchar *title);

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

SETUP (mlib) {
	g_thread_init (0);
	const gchar *indices[] = {
		XMMS_MEDIALIB_ENTRY_PROPERTY_URL,
		XMMS_MEDIALIB_ENTRY_PROPERTY_STATUS,
		NULL
	};

	/* initialize the global source preferences */
	default_sp = s4_sourcepref_create (source_pref);

	/* initialize the global medialib */
	medialib = xmms_object_new (xmms_medialib_t, NULL);
	medialib->s4 = s4_open (NULL, indices, S4_MEMORY);
	return 0;
}

CLEANUP () {
	s4_sourcepref_unref (default_sp);
	s4_close (medialib->s4);
	xmms_object_unref (medialib);
	return 0;
}

static xmmsv_t *
medialib_query (xmmsv_coll_t *coll, xmmsv_t *spec, xmms_error_t *err)
{
	xmmsv_t *ret;

	SESSION (ret = xmms_medialib_query (session, coll, spec, err));

	return ret;
}

CASE (test_entry_property_get)
{
	xmmsv_t *result;
	gint tracknr;
	const gchar *title;
	gchar *string;

	xmms_mock_entry (1, "Red Fang", "Red Fang", "Prehistoric Dog");
	xmms_mock_entry (4, "Red Fang", "Red Fang", "Humans Remain Human Remains");

	SESSION (result = xmms_medialib_entry_property_get_value (session, 1, "tracknr"));
	xmmsv_get_int (result, &tracknr);
	CU_ASSERT_EQUAL (4, tracknr);
	xmmsv_unref (result);

	SESSION (result = xmms_medialib_entry_property_get_value (session, 0, "title"));
	xmmsv_get_string (result, &title);
	CU_ASSERT_STRING_EQUAL ("Prehistoric Dog", title);
	xmmsv_unref (result);

	SESSION (result = xmms_medialib_entry_property_get_value (session, 1337, "tracknr"));
	CU_ASSERT_EQUAL (NULL, result);

	SESSION (string = xmms_medialib_entry_property_get_str (session, 0, "title"));
	CU_ASSERT_STRING_EQUAL ("Prehistoric Dog", string);
	g_free (string);

	SESSION (string = xmms_medialib_entry_property_get_str (session, 1, "tracknr"));
	CU_ASSERT_STRING_EQUAL ("4", string);
	g_free (string);

	SESSION (string = xmms_medialib_entry_property_get_str (session, 1337, "monkey"));
	CU_ASSERT_EQUAL (NULL, string);

	SESSION (tracknr = xmms_medialib_entry_property_get_int (session, 1, "tracknr"));
	CU_ASSERT_EQUAL (4, tracknr);

	SESSION (tracknr = xmms_medialib_entry_property_get_int (session, 1337, "tracknr"));
	CU_ASSERT_EQUAL (-1, tracknr);

	/*
	result = xmms_medialib_entry_property_get_value (1337, "id");
	CU_ASSERT_PTR_NULL (result);
	*/
}

CASE (test_entry_remove)
{
	xmmsv_t *result;

	xmms_mock_entry (1, "Red Fang", "Red Fang", "Prehistoric Dog");

	SESSION (result = xmms_medialib_entry_property_get_value (session, 0, "tracknr"));
	CU_ASSERT_PTR_NOT_NULL (result);
	xmmsv_unref (result);

	SESSION (xmms_medialib_entry_remove (session, 0));

	SESSION (result = xmms_medialib_entry_property_get_value (session, 0, "tracknr"));
	CU_ASSERT_PTR_NULL (result);
}

CASE (test_entry_cleanup)
{
	xmmsv_coll_t *universe;
	xmmsv_t *spec, *result;
	xmms_error_t err;
	gint count;

	xmms_error_reset (&err);

	xmms_mock_entry (1, "Red Fang", "Red Fang", "Prehistoric Dog");

	SESSION (xmms_medialib_entry_cleanup (session, 0));

	SESSION (result = xmms_medialib_entry_property_get_value (session, 0, "tracknr"));
	CU_ASSERT_PTR_NULL (result);

	/* Should be cleaned up to check with _get_value once
	 * it actually checks the db when getting value by id.
	 */
	universe = xmmsv_coll_universe ();

	spec = xmmsv_from_json ("{ 'type': 'count' }");
	result = medialib_query (universe, spec, &err);

	xmmsv_get_int (result, &count);

	CU_ASSERT_EQUAL (1, count);

	xmmsv_unref (spec);
	xmmsv_unref (result);
	xmmsv_coll_unref (universe);
}

CASE (test_not_resolved)
{
	xmms_medialib_entry_t entry;
	guint count;

	xmms_mock_entry (1, "Red Fang", "Red Fang", "Prehistoric Dog");
	xmms_mock_entry (2, "Red Fang", "Red Fang", "Reverse Thunder");

	SESSION (count = xmms_medialib_num_not_resolved (session));

	CU_ASSERT_EQUAL (2, count);

	SESSION (entry = xmms_medialib_entry_not_resolved_get (session));
	CU_ASSERT (entry == 0 || entry == 1);
}

CASE (test_query_random_id)
{
	xmms_medialib_entry_t entry;
	xmmsv_coll_t *universe;

	xmms_mock_entry (1, "Red Fang", "Red Fang", "Prehistoric Dog");
	xmms_mock_entry (2, "Red Fang", "Red Fang", "Reverse Thunder");

	universe = xmmsv_coll_universe ();
	SESSION (entry = xmms_medialib_query_random_id (session, universe));
	xmmsv_coll_unref (universe);

	CU_ASSERT (entry == 0 || entry == 1);
}

CASE (test_session)
{
	xmmsv_t *result, *spec;
	gint tracknr;
	xmms_medialib_session_t *session;
	xmmsv_coll_t *universe;
	xmms_error_t err;

	xmms_mock_entry (1, "Red Fang", "Red Fang", "Prehistoric Dog");
	xmms_mock_entry (4, "Red Fang", "Red Fang", "Humans Remain Human Remains");

	session = xmms_medialib_begin (medialib);
	xmms_medialib_entry_remove (session, 0);
	xmms_medialib_abort (session);

	SESSION (result = xmms_medialib_entry_property_get_value (session, 0, "tracknr"));
	xmmsv_get_int (result, &tracknr);
	CU_ASSERT_EQUAL (1, tracknr);
	xmmsv_unref (result);

	SESSION (xmms_medialib_entry_remove (session, 0));
	SESSION (result = xmms_medialib_entry_property_get_value (session, 0, "tracknr"));
	CU_ASSERT_PTR_NULL (result);

	universe = xmmsv_coll_universe ();
	spec = xmmsv_from_json ("{ 'type': 'count' }");

	session = xmms_medialib_begin (medialib);
	result = xmms_medialib_query (session, universe, spec, &err);
	xmms_medialib_abort (session);

	xmmsv_coll_unref (universe);
	xmmsv_unref (spec);
}

CASE (test_metadata_fetch_spec)
{
	xmmsv_coll_t *universe;
	xmmsv_t *spec, *result;
	xmms_error_t err;

	xmms_mock_entry (1, "Red Fang", "Red Fang", "Prehistoric Dog");

	universe = xmmsv_coll_universe ();


	/* missing 'get' parameter */
	spec = xmmsv_from_json ("{ 'type': 'metadata' }");
	CU_ASSERT_PTR_NULL (medialib_query (universe, spec, &err));
	CU_ASSERT_TRUE (xmms_error_iserror (&err));
	xmmsv_unref (spec);

	/* invalid 'get' entry */
	spec = xmmsv_from_json ("{ 'type': 'metadata', 'get': ['crap'] }");
	CU_ASSERT_PTR_NULL (medialib_query (universe, spec, &err));
	CU_ASSERT_TRUE (xmms_error_iserror (&err));
	xmmsv_unref (spec);

	/* invalid 'get' parameter */
	spec = xmmsv_from_json ("{ 'type': 'metadata', 'get': {} }");
	CU_ASSERT_PTR_NULL (medialib_query (universe, spec, &err));
	CU_ASSERT_TRUE (xmms_error_iserror (&err));
	xmmsv_unref (spec);

	/* invalid 'get' parameter type */
	spec = xmmsv_from_json ("{ 'type': 'metadata', 'get': [0] }");
	CU_ASSERT_PTR_NULL (medialib_query (universe, spec, &err));
	CU_ASSERT_TRUE (xmms_error_iserror (&err));
	xmmsv_unref (spec);

	/* empty 'get' parameter */
	spec = xmmsv_from_json ("{ 'type': 'metadata', 'get': [] }");
	CU_ASSERT_PTR_NULL (medialib_query (universe, spec, &err));
	CU_ASSERT_TRUE (xmms_error_iserror (&err));
	xmmsv_unref (spec);

	/* duplicate 'get' entries */
	spec = xmmsv_from_json ("{ 'type': 'metadata', 'get': ['value', 'value'] }");
	CU_ASSERT_PTR_NULL (medialib_query (universe, spec, &err));
	CU_ASSERT_TRUE (xmms_error_iserror (&err));
	xmmsv_unref (spec);

	/* all 'get' entries */
	spec = xmmsv_from_json ("{ 'type': 'metadata', 'get': ['id', 'field', 'source', 'value'] }");
	result = medialib_query (universe, spec, &err);
	CU_ASSERT_FALSE (xmms_error_iserror (&err));
	CU_ASSERT_PTR_NOT_NULL (result);
	xmmsv_unref (spec);
	xmmsv_unref (result);


	/* duplicate 'fields' content */
	spec = xmmsv_from_json ("{ 'type': 'metadata', 'fields': ['a', 'a'], 'get': ['value'] }");
	CU_ASSERT_PTR_NULL (medialib_query (universe, spec, &err));
	CU_ASSERT_TRUE (xmms_error_iserror (&err));
	xmmsv_unref (spec);

	/* invalid 'fields' parameter */
	spec = xmmsv_from_json ("{ 'type': 'metadata', 'fields': 0, 'get': ['value'] }");
	CU_ASSERT_PTR_NULL (medialib_query (universe, spec, &err));
	CU_ASSERT_TRUE (xmms_error_iserror (&err));
	xmmsv_unref (spec);

	/* empty 'fields' parameter, fetch all fields */
	spec = xmmsv_from_json ("{ 'type': 'metadata', 'fields': [], 'get': ['value'] }");
	result = medialib_query (universe, spec, &err);
	CU_ASSERT_FALSE (xmms_error_iserror (&err));
	xmmsv_unref (result);
	xmmsv_unref (spec);

	/* invalid 'fields' content */
	spec = xmmsv_from_json ("{ 'type': 'metadata', 'fields': [0], 'get': ['value'] }");
	CU_ASSERT_PTR_NULL (medialib_query (universe, spec, &err));
	CU_ASSERT_TRUE (xmms_error_iserror (&err));
	xmmsv_unref (spec);


	/* valid source-preferences parameter */
	spec = xmmsv_from_json ("{ 'type': 'metadata', 'get': ['value'], 'source-preference': ['*'] }");
	result = medialib_query (universe, spec, &err);
	CU_ASSERT_FALSE (xmms_error_iserror (&err));
	xmmsv_unref (result);
	xmmsv_unref (spec);

	/* invalid source-preferences parameter */
	spec = xmmsv_from_json ("{ 'type': 'metadata', 'get': ['value'], 'source-preference': 0 }");
	CU_ASSERT_PTR_NULL (medialib_query (universe, spec, &err));
	CU_ASSERT_TRUE (xmms_error_iserror (&err));
	xmmsv_unref (spec);

	/* empty source-preferences parameter */
	spec = xmmsv_from_json ("{ 'type': 'metadata', 'get': ['value'], 'source-preference': [] }");
	result = medialib_query (universe, spec, &err);
	CU_ASSERT_FALSE (xmms_error_iserror (&err));
	xmmsv_unref (result);
	xmmsv_unref (spec);

	/* invalid source-preferences parameter */
	spec = xmmsv_from_json ("{ 'type': 'metadata', 'get': ['value'], 'source-preference': [0] }");
	CU_ASSERT_PTR_NULL (medialib_query (universe, spec, &err));
	CU_ASSERT_TRUE (xmms_error_iserror (&err));
	xmmsv_unref (spec);


	/* invalid aggregate function */
	spec = xmmsv_from_json ("{ 'type': 'metadata', 'get': ['value'], 'aggregate': 'sausage' }");
	CU_ASSERT_PTR_NULL (medialib_query (universe, spec, &err));
	CU_ASSERT_TRUE (xmms_error_iserror (&err));
	xmmsv_unref (spec);


	xmmsv_coll_unref (universe);
}

static void
xmms_mock_entry (gint tracknr, const gchar *artist, const gchar *album, const gchar *title)
{
	xmms_medialib_entry_t entry;
	xmms_error_t err;
	gchar *path;

	xmms_error_reset (&err);

	path = g_strconcat (artist, album, title, NULL);
	SESSION (entry = xmms_medialib_entry_new (session, path, &err));

	SESSION (
	xmms_medialib_entry_property_set_int (session, entry,
	                                      XMMS_MEDIALIB_ENTRY_PROPERTY_TRACKNR,
	                                      tracknr);
	xmms_medialib_entry_property_set_str (session, entry,
	                                      XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST,
	                                      artist);
	xmms_medialib_entry_property_set_str (session, entry,
	                                      XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM,
	                                      album);
	xmms_medialib_entry_property_set_str (session, entry,
	                                      XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE,
	                                      title);
	xmms_medialib_entry_property_set_int (session, entry,
	                                      XMMS_MEDIALIB_ENTRY_PROPERTY_STATUS,
	                                      XMMS_MEDIALIB_ENTRY_STATUS_NEW));

	g_free (path);
}

/* START: Stub some crap so we don't have to pull in the whole daemon */
gboolean
xmms_playlist_remove_by_entry (xmms_playlist_t *playlist,
                               xmms_medialib_entry_t entry)
{
	return TRUE;
}

void
xmms_playlist_insert_entry (xmms_playlist_t *playlist, const gchar *plname,
                            guint32 pos, xmms_medialib_entry_t file, xmms_error_t *err)
{
}

void
xmms_playlist_add_entry (xmms_playlist_t *playlist, const gchar *plname,
                         xmms_medialib_entry_t file, xmms_error_t *err)
{
}

GList *
xmms_xform_browse (const gchar *url, xmms_error_t *error)
{
	return NULL;
}

xmms_mediainfo_reader_t *
xmms_playlist_mediainfo_reader_get (xmms_playlist_t *playlist)
{
	return NULL;
};

void
xmms_mediainfo_reader_wakeup (xmms_mediainfo_reader_t *mr)
{
}
/* END */
