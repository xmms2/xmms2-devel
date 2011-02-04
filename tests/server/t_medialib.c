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

static void xmms_mock_entry (gint tracknr, const gchar *artist,
                             const gchar *album, const gchar *title);
static void xmms_dump (xmmsv_t *value);

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


CASE (test_query_ids_order_by_id)
{
	xmmsv_coll_t *universe, *ordered;
	xmmsv_t *meta, *spec, *result;
	xmms_error_t err;

	xmms_error_reset (&err);

	xmms_mock_entry (1, "Vibrasphere", "Lungs for Life", "Decade");
	xmms_mock_entry (2, "Vibrasphere", "Lungs for Life", "Breathing Place");
	xmms_mock_entry (3, "Vibrasphere", "Lungs for Life", "Ensueno (Morning mix)");

	universe = xmmsv_coll_universe ();
	ordered = xmmsv_coll_add_order_operator (universe, "id");

	meta = xmmsv_build_metadata (NULL, xmmsv_new_string ("id"), "first", NULL);
	spec = xmmsv_build_cluster_list (xmmsv_new_string ("_row"), meta);

	result = medialib_query (ordered, spec, &err);

	CU_ASSERT_LIST_INT_EQUAL (result, 0, 0);
	CU_ASSERT_LIST_INT_EQUAL (result, 1, 1);
	CU_ASSERT_LIST_INT_EQUAL (result, 2, 2);

	xmmsv_unref (spec);
	xmmsv_unref (result);
	xmmsv_coll_unref (ordered);
	xmmsv_coll_unref (universe);
}

CASE (test_query_infos_order_by_tracknr)
{
	xmmsv_coll_t *universe, *ordered, *limited;
	xmmsv_t *meta, *org_data, *org_dict, *spec, *result, *group_by;
	xmms_error_t err;

	xmms_error_reset (&err);

	xmms_mock_entry (1, "Red Fang", "Red Fang", "Prehistoric Dog");
	xmms_mock_entry (4, "Red Fang", "Red Fang", "Humans Remain Human Remains");
	xmms_mock_entry (3, "Red Fang", "Red Fang", "Night Destroyer"); // selecting this one
	xmms_mock_entry (2, "Red Fang", "Red Fang", "Reverse Thunder"); // selecting this one

	universe = xmmsv_coll_universe ();
	ordered = xmmsv_coll_add_order_operator (universe, "tracknr");
	limited = xmmsv_coll_add_limit_operator (ordered, 1, 2);

	group_by = xmmsv_new_string ("title");

	org_data = xmmsv_new_dict ();

	meta = xmmsv_build_metadata (NULL, xmmsv_new_string ("id"), "first", NULL);
	xmmsv_dict_set (org_data, "id", meta);
	xmmsv_unref (meta);

	meta = xmmsv_build_metadata (xmmsv_new_string ("title"), xmmsv_new_string ("value"), "first", NULL);
	xmmsv_dict_set (org_data, "title", meta);
	xmmsv_unref (meta);

	meta = xmmsv_build_metadata (xmmsv_new_string ("tracknr"), xmmsv_new_string ("value"), "first", NULL);
	xmmsv_dict_set (org_data, "tracknr", meta);
	xmmsv_unref (meta);

	org_dict = xmmsv_build_organize (org_data);
	spec = xmmsv_build_cluster_list (group_by, org_dict);

	result = medialib_query (limited, spec, &err);

	CU_ASSERT_LIST_DICT_INT_EQUAL (result, 0, "tracknr", 2);
	CU_ASSERT_LIST_DICT_INT_EQUAL (result, 1, "tracknr", 3);

	xmmsv_unref (spec);
	xmmsv_unref (result);
	xmmsv_coll_unref (limited);
	xmmsv_coll_unref (ordered);
	xmmsv_coll_unref (universe);
}


CASE (test_query_count_all)
{
	xmmsv_coll_t *universe;
	xmmsv_t *spec, *result;
	xmms_error_t err;
	gint count;

	xmms_error_reset (&err);

	xmms_mock_entry (1, "Red Fang", "Red Fang", "Prehistoric Dog");
	xmms_mock_entry (2, "Red Fang", "Red Fang", "Reverse Thunder");
	xmms_mock_entry (3, "Red Fang", "Red Fang", "Night Destroyer");
	xmms_mock_entry (4, "Red Fang", "Red Fang", "Humans Remain Human Remains");

	universe = xmmsv_coll_universe ();

	spec = xmmsv_build_count ();
	result = medialib_query (universe, spec, &err);

	xmmsv_get_int (result, &count);

	CU_ASSERT_EQUAL (4, count);

	xmmsv_unref (spec);
	xmmsv_unref (result);
	xmmsv_coll_unref (universe);
}

CASE (test_query_aggregate_sum)
{
	xmmsv_coll_t *universe;
	xmmsv_t *spec, *result;
	xmms_error_t err;
	gint count;

	xmms_error_reset (&err);

	xmms_mock_entry (1, "Red Fang", "Red Fang", "Prehistoric Dog");
	xmms_mock_entry (2, "Red Fang", "Red Fang", "Reverse Thunder");
	xmms_mock_entry (3, "Red Fang", "Red Fang", "Night Destroyer");
	xmms_mock_entry (4, "Red Fang", "Red Fang", "Humans Remain Human Remains");

	universe = xmmsv_coll_universe ();

	spec = xmmsv_build_metadata (xmmsv_new_string ("tracknr"), xmmsv_new_string ("value"), "sum", NULL);
	result = medialib_query (universe, spec, &err);

	xmmsv_get_int (result, &count);

	CU_ASSERT_EQUAL (4+3+2+1, count);

	xmmsv_unref (spec);
	xmmsv_unref (result);
	xmmsv_coll_unref (universe);
}

CASE (test_query_ordered_union)
{
	xmmsv_coll_t *universe, *first, *ordered_first, *second, *ordered_second, *union_, *ordered_union;
	xmmsv_t *spec, *result, *org_dict, *group_by, *org_data, *meta;
	xmms_error_t err;
	gint i;

	xmms_error_reset (&err);

	xmms_mock_entry (1, "Red Fang", "Red Fang", "Prehistoric Dog");
	xmms_mock_entry (2, "Red Fang", "Red Fang", "Reverse Thunder");
	xmms_mock_entry (3, "Red Fang", "Red Fang", "Night Destroyer");
	xmms_mock_entry (4, "Red Fang", "Red Fang", "Humans Remain Human Remains");

	xmms_mock_entry (1, "Vibrasphere", "Lungs for Life", "Decade");
	xmms_mock_entry (2, "Vibrasphere", "Lungs for Life", "Breathing Place");
	xmms_mock_entry (3, "Vibrasphere", "Lungs for Life", "Ensueno (Morning mix)");

	universe = xmmsv_coll_universe ();

	first = xmmsv_coll_new (XMMS_COLLECTION_TYPE_FILTER);
	xmmsv_coll_attribute_set (first, "operation", XMMS_COLLECTION_FILTER_EQUAL);
	xmmsv_coll_attribute_set (first, "field", "artist");
	xmmsv_coll_attribute_set (first, "value", "Vibrasphere");
	xmmsv_coll_add_operand (first, universe);

	ordered_first = xmmsv_coll_add_order_operator (first, "tracknr");
	xmmsv_coll_unref (first);

	second = xmmsv_coll_new (XMMS_COLLECTION_TYPE_FILTER);
	xmmsv_coll_attribute_set (second, "operation", XMMS_COLLECTION_FILTER_EQUAL);
	xmmsv_coll_attribute_set (second, "field", "artist");
	xmmsv_coll_attribute_set (second, "value", "Red Fang");
	xmmsv_coll_add_operand (second, universe);

	ordered_second = xmmsv_coll_add_order_operator (second, "tracknr");
	xmmsv_coll_unref (second);

	union_ = xmmsv_coll_new (XMMS_COLLECTION_TYPE_UNION);

	xmmsv_coll_add_operand (union_, ordered_first);
	xmmsv_coll_unref (ordered_first);

	xmmsv_coll_add_operand (union_, ordered_second);
	xmmsv_coll_unref (ordered_second);

	xmmsv_coll_unref (universe);

	ordered_union = xmmsv_coll_add_order_operator (union_, "id");
	xmmsv_coll_unref (union_);

	group_by = xmmsv_new_string ("title");

	org_data = xmmsv_new_dict ();

	meta = xmmsv_build_metadata (NULL, xmmsv_new_string ("id"), "first", NULL);
	xmmsv_dict_set (org_data, "id", meta);
	xmmsv_unref (meta);

	meta = xmmsv_build_metadata (xmmsv_new_string ("title"), xmmsv_new_string ("value"), "first", NULL);
	xmmsv_dict_set (org_data, "title", meta);
	xmmsv_unref (meta);

	meta = xmmsv_build_metadata (xmmsv_new_string ("tracknr"), xmmsv_new_string ("value"), "first", NULL);
	xmmsv_dict_set (org_data, "tracknr", meta);
	xmmsv_unref (meta);

	org_dict = xmmsv_build_organize (org_data);
	spec = xmmsv_build_cluster_list (group_by, org_dict);

	result = medialib_query (ordered_union, spec, &err);

	for (i = 0; i < 7; i++)
		CU_ASSERT_LIST_DICT_INT_EQUAL (result, i, "id", i);

	xmmsv_unref (spec);
	xmmsv_unref (result);
	xmmsv_coll_unref (ordered_union);
}

CASE (test_query_intersection)
{
	xmmsv_coll_t *universe, *first, *second, *intersection;
	xmmsv_t *spec, *result, *org_dict, *group_by, *org_data, *meta;
	xmms_error_t err;

	xmms_error_reset (&err);

	xmms_mock_entry (1, "Red Fang", "Red Fang", "Prehistoric Dog");

	xmms_mock_entry (1, "Vibrasphere", "Lungs for Life", "Decade");
	xmms_mock_entry (2, "Vibrasphere", "Lungs for Life", "Breathing Place");
	xmms_mock_entry (3, "Vibrasphere", "Lungs for Life", "Ensueno (Morning mix)");

	universe = xmmsv_coll_universe ();

	first = xmmsv_coll_new (XMMS_COLLECTION_TYPE_FILTER);
	xmmsv_coll_attribute_set (first, "operation", XMMS_COLLECTION_FILTER_EQUAL);
	xmmsv_coll_attribute_set (first, "field", "artist");
	xmmsv_coll_attribute_set (first, "value", "Vibrasphere");
	xmmsv_coll_add_operand (first, universe);

	second = xmmsv_coll_new (XMMS_COLLECTION_TYPE_FILTER);
	xmmsv_coll_attribute_set (second, "operation", XMMS_COLLECTION_FILTER_EQUAL);
	xmmsv_coll_attribute_set (second, "field", "tracknr");
	xmmsv_coll_attribute_set (second, "value", "1");
	xmmsv_coll_add_operand (second, universe);

	intersection = xmmsv_coll_new (XMMS_COLLECTION_TYPE_INTERSECTION);

	xmmsv_coll_add_operand (intersection, first);
	xmmsv_coll_unref (first);

	xmmsv_coll_add_operand (intersection, second);
	xmmsv_coll_unref (second);

	xmmsv_coll_unref (universe);

	// TODO: Should group_by really be mandatory, or default to "id" ?
	group_by = xmmsv_new_string ("id");

	org_data = xmmsv_new_dict ();

	meta = xmmsv_build_metadata (NULL, xmmsv_new_string ("id"), "first", NULL);
	xmmsv_dict_set (org_data, "id", meta);
	xmmsv_unref (meta);

	org_dict = xmmsv_build_organize (org_data);
	spec = xmmsv_build_cluster_list (group_by, org_dict);

	MEDIALIB_SESSION (medialib, result = xmms_medialib_query (session, intersection, spec, &err));

	CU_ASSERT_LIST_DICT_INT_EQUAL (result, 0, "id", 1);

	xmmsv_unref (spec);
	xmmsv_unref (result);
	xmmsv_coll_unref (intersection);
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

	spec = xmmsv_build_count ();
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
	spec = xmmsv_build_count ();

	session = xmms_medialib_begin (medialib);
	result = xmms_medialib_query (session, universe, spec, &err);
	xmms_medialib_abort (session);

	xmmsv_coll_unref (universe);
	xmmsv_unref (spec);
}

static void
_xmms_dump_indent (gint indent)
{
	gint i;
	for (i = 0; i < indent; i++)
		printf (" ");
}

static void
_xmms_dump (xmmsv_t *value, gint indent)
{
	gint type;

	if (value == NULL) {
		printf ("xmmsv_t is NULL!\n");
		return;
	}

	if (xmmsv_is_error (value)) {
		const gchar *message;
		xmmsv_get_error (value, &message);
		printf ("error: %s\n", message);
		return;
	}

	type = xmmsv_get_type (value);

	switch (type) {
	case XMMSV_TYPE_INT32: {
		gint val;
		xmmsv_get_int (value, &val);
		printf ("%d", val);
		break;
	}
	case XMMSV_TYPE_STRING: {
		const gchar *val;
		xmmsv_get_string (value, &val);
		printf ("%s", val);
		break;
	}
	case XMMSV_TYPE_LIST: {
		xmmsv_list_iter_t *iter;
		xmmsv_get_list_iter (value, &iter);

		printf ("[");
		while (xmmsv_list_iter_valid (iter)) {
			xmmsv_t *item;

			xmmsv_list_iter_entry (iter, &item);

			_xmms_dump (item, indent + 1);

			xmmsv_list_iter_next (iter);
			if (xmmsv_list_iter_valid (iter))
				printf (", ");
		}
		printf ("]");
		break;
	}
	case XMMSV_TYPE_DICT: {
		xmmsv_dict_iter_t *iter;

		xmmsv_get_dict_iter (value, &iter);

		printf ("{\n");
		while (xmmsv_dict_iter_valid (iter)) {
			const gchar *key;
			xmmsv_t *item;

			xmmsv_dict_iter_pair (iter, &key, &item);

			_xmms_dump_indent (indent + 1);
			printf ("%s: ", key);
			_xmms_dump (item, indent + 1);

			xmmsv_dict_iter_next (iter);
			if (xmmsv_dict_iter_valid (iter))
				printf (", ");
			printf ("\n");
		}
		_xmms_dump_indent (indent);
		printf ("}");

		break;
	}
	default:
		printf ("invalid type: %d\n", type);
	}
}

static void
xmms_dump (xmmsv_t *value)
{
	_xmms_dump (value, 0);
	printf ("\n");
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
