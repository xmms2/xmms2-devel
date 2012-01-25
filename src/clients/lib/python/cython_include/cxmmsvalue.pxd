cdef extern from *:
	ctypedef char const_char "const char"
	ctypedef unsigned char const_uchar "const unsigned char"
	ctypedef int const_int "const int"

cdef extern from "xmmsc/xmmsc_idnumbers.h":
	ctypedef enum xmmsv_coll_type_t:
		XMMS_COLLECTION_TYPE_REFERENCE
		XMMS_COLLECTION_TYPE_UNIVERSE
		XMMS_COLLECTION_TYPE_UNION
		XMMS_COLLECTION_TYPE_INTERSECTION
		XMMS_COLLECTION_TYPE_COMPLEMENT
		XMMS_COLLECTION_TYPE_HAS
		XMMS_COLLECTION_TYPE_MATCH
		XMMS_COLLECTION_TYPE_TOKEN
		XMMS_COLLECTION_TYPE_EQUALS
		XMMS_COLLECTION_TYPE_NOTEQUAL
		XMMS_COLLECTION_TYPE_SMALLER
		XMMS_COLLECTION_TYPE_SMALLEREQ
		XMMS_COLLECTION_TYPE_GREATER
		XMMS_COLLECTION_TYPE_GREATEREQ
		XMMS_COLLECTION_TYPE_ORDER
		XMMS_COLLECTION_TYPE_LIMIT
		XMMS_COLLECTION_TYPE_MEDIASET
		XMMS_COLLECTION_TYPE_IDLIST

	ctypedef char *xmmsv_coll_namespace_t
	# XXX Trick cython compiler which doesn't deal well with extern
	# variables. Requires an explicit cast to <char *> when used directly.
	enum:
		XMMS_COLLECTION_NS_COLLECTIONS
		XMMS_COLLECTION_NS_PLAYLISTS
		XMMS_COLLECTION_NS_ALL

	# XXX Same trick. Requires an explicit cast <char *>
	enum:
		XMMS_ACTIVE_PLAYLIST

cdef extern from "xmmsc/xmmsv.h":
	ctypedef enum xmmsv_type_t:
		XMMSV_TYPE_NONE
		XMMSV_TYPE_ERROR
		XMMSV_TYPE_INT32
		XMMSV_TYPE_STRING
		XMMSV_TYPE_COLL
		XMMSV_TYPE_BIN
		XMMSV_TYPE_LIST
		XMMSV_TYPE_DICT

	ctypedef struct xmmsv_t
	ctypedef struct xmmsv_coll_t

	ctypedef struct xmmsv_list_iter_t
	ctypedef struct xmmsv_dict_iter_t

	xmmsv_t *xmmsv_new_none   ()
	xmmsv_t *xmmsv_new_error  (char *errstr)
	xmmsv_t *xmmsv_new_int    (int i)
	xmmsv_t *xmmsv_new_string (char *s)
	xmmsv_t *xmmsv_new_coll   (xmmsv_coll_t *coll)
	xmmsv_t *xmmsv_new_bin    (unsigned char *data, unsigned int len)

	xmmsv_t *xmmsv_new_list ()
	xmmsv_t *xmmsv_new_dict ()

	xmmsv_t *xmmsv_ref   (xmmsv_t *val)
	void     xmmsv_unref (xmmsv_t *val)

	xmmsv_type_t xmmsv_get_type (xmmsv_t *val)
	bint         xmmsv_is_type  (xmmsv_t *val, xmmsv_type_t t)

	bint xmmsv_is_error (xmmsv_t *val)

	ctypedef void (*xmmsv_list_foreach_func) (xmmsv_t *value, void *user_data)
	ctypedef void (*xmmsv_dict_foreach_func) (char *key, xmmsv_t *value, void *user_data)

	bint xmmsv_get_error  (xmmsv_t *value, const_char **r)
	bint xmmsv_get_int    (xmmsv_t *res, int *r)
	bint xmmsv_get_string (xmmsv_t *res, const_char **r)
	bint xmmsv_get_coll   (xmmsv_t *value, xmmsv_coll_t **coll)
	bint xmmsv_get_bin    (xmmsv_t *res, const_uchar **r, unsigned int *rlen)

	bint xmmsv_get_list_iter (xmmsv_t *val, xmmsv_list_iter_t **it)
	bint xmmsv_get_dict_iter (xmmsv_t *val, xmmsv_dict_iter_t **it)

	void xmmsv_list_iter_explicit_destroy (xmmsv_list_iter_t *it)
	void xmmsv_dict_iter_explicit_destroy (xmmsv_dict_iter_t *it)

	# List
	bint xmmsv_list_get      (xmmsv_t *listv, int pos, xmmsv_t **val)
	bint xmmsv_list_set      (xmmsv_t *listv, int pos, xmmsv_t *val)
	bint xmmsv_list_append   (xmmsv_t *listv, xmmsv_t *val)
	bint xmmsv_list_insert   (xmmsv_t *listv, int pos, xmmsv_t *val)
	bint xmmsv_list_remove   (xmmsv_t *listv, int pos)
	bint xmmsv_list_move     (xmmsv_t *listv, int old_pos, int new_pos)
	bint xmmsv_list_clear    (xmmsv_t *listv)
	bint xmmsv_list_foreach  (xmmsv_t *listv, xmmsv_list_foreach_func func, void* user_data)
	int  xmmsv_list_get_size (xmmsv_t *listv)
	bint xmmsv_list_restrict_type (xmmsv_t *listv, xmmsv_type_t type)
	bint xmmsv_list_has_type (xmmsv_t *listv, xmmsv_type_t type)

	bint xmmsv_list_get_string (xmmsv_t *v, int pos, const_char **val)
	bint xmmsv_list_get_int    (xmmsv_t *v, int pos, int *val)
	bint xmmsv_list_get_coll   (xmmsv_t *v, int pos, xmmsv_coll_t **val)

	bint xmmsv_list_set_string (xmmsv_t *v, int pos, const_char *val)
	bint xmmsv_list_set_int    (xmmsv_t *v, int pos, int val)
	bint xmmsv_list_set_coll   (xmmsv_t *v, int pos, xmmsv_coll_t *val)

	bint xmmsv_list_insert_string (xmmsv_t *v, int pos, const_char *val)
	bint xmmsv_list_insert_int    (xmmsv_t *v, int pos, int val)
	bint xmmsv_list_insert_coll   (xmmsv_t *v, int pos, xmmsv_coll_t *val)

	bint xmmsv_list_append_string (xmmsv_t *v, const_char *val)
	bint xmmsv_list_append_int    (xmmsv_t *v, int val)
	bint xmmsv_list_append_coll   (xmmsv_t *v, xmmsv_coll_t *val)

	bint     xmmsv_list_iter_entry (xmmsv_list_iter_t *it, xmmsv_t **val)
	bint     xmmsv_list_iter_valid (xmmsv_list_iter_t *it)
	void     xmmsv_list_iter_first (xmmsv_list_iter_t *it)
	void     xmmsv_list_iter_last  (xmmsv_list_iter_t *it)
	void     xmmsv_list_iter_next  (xmmsv_list_iter_t *it)
	void     xmmsv_list_iter_prev  (xmmsv_list_iter_t *it)
	bint     xmmsv_list_iter_seek  (xmmsv_list_iter_t *it, int pos)
	bint     xmmsv_list_iter_tell  (xmmsv_list_iter_t *it)
	xmmsv_t *xmmsv_list_iter_get_parent (xmmsv_list_iter_t *it)

	bint xmmsv_list_iter_insert (xmmsv_list_iter_t *it, xmmsv_t *val)
	bint xmmsv_list_iter_remove (xmmsv_list_iter_t *it)

	bint xmmsv_list_iter_entry_string (xmmsv_list_iter_t *v, const_char **valval)
	bint xmmsv_list_iter_entry_int    (xmmsv_list_iter_t *v, int *val)
	bint xmmsv_list_iter_entry_coll   (xmmsv_list_iter_t *v, xmmsv_coll_t **val)

	bint xmmsv_list_iter_insert_string (xmmsv_list_iter_t *v, const_char *val)
	bint xmmsv_list_iter_insert_int    (xmmsv_list_iter_t *v, int val)
	bint xmmsv_list_iter_insert_coll   (xmmsv_list_iter_t *v, xmmsv_coll_t *val)

	xmmsv_t *xmmsv_list_flatten (xmmsv_t *list, int depth)

	# Dict
	bint xmmsv_dict_get      (xmmsv_t *dictv, char *key, xmmsv_t **val)
	bint xmmsv_dict_set      (xmmsv_t *dictv, char *key, xmmsv_t *val)
	bint xmmsv_dict_remove   (xmmsv_t *dictv, char *key)
	bint xmmsv_dict_clear    (xmmsv_t *dictv)
	bint xmmsv_dict_foreach  (xmmsv_t *dictv, xmmsv_dict_foreach_func func, void *user_data)
	int  xmmsv_dict_get_size (xmmsv_t *dictv)
	bint xmmsv_dict_has_key  (xmmsv_t *dictv, const_char *key)

	bint xmmsv_dict_entry_get_string (xmmsv_t *val, const_char *key, const_char **r)
	bint xmmsv_dict_entry_get_int    (xmmsv_t *val, const_char *key, int *r)
	bint xmmsv_dict_entry_get_coll   (xmmsv_t *val, const_char *key, xmmsv_coll_t **coll)

	bint xmmsv_dict_set_string (xmmsv_t *val, const_char *key, const_char *el)
	bint xmmsv_dict_set_int    (xmmsv_t *val, const_char *key, int el)
	bint xmmsv_dict_set_coll   (xmmsv_t *val, const_char *key, xmmsv_coll_t *el)

	bint xmmsv_dict_iter_pair  (xmmsv_dict_iter_t *it, const_char **key, xmmsv_t **val)
	bint xmmsv_dict_iter_valid (xmmsv_dict_iter_t *it)
	void xmmsv_dict_iter_first (xmmsv_dict_iter_t *it)
	void xmmsv_dict_iter_next  (xmmsv_dict_iter_t *it)
	bint xmmsv_dict_iter_find  (xmmsv_dict_iter_t *it, const_char *key)

	bint xmmsv_dict_iter_set    (xmmsv_dict_iter_t *it, xmmsv_t *val)
	bint xmmsv_dict_iter_remove (xmmsv_dict_iter_t *it)

	bint xmmsv_dict_iter_pair_string (xmmsv_dict_iter_t *it, const_char **key, const_char **val)
	bint xmmsv_dict_iter_pair_int    (xmmsv_dict_iter_t *it, const_char **key, int *val)
	bint xmmsv_dict_iter_pair_coll   (xmmsv_dict_iter_t *it, const_char **key, xmmsv_coll_t **val)

	bint xmmsv_dict_iter_set_string (xmmsv_dict_iter_t *it, const_char *el)
	bint xmmsv_dict_iter_set_int    (xmmsv_dict_iter_t *it, int el)
	bint xmmsv_dict_iter_set_coll   (xmmsv_dict_iter_t *it, xmmsv_coll_t *el)


	# Collection
	xmmsv_coll_t *xmmsv_coll_new   (xmmsv_coll_type_t type)
	xmmsv_coll_t *xmmsv_coll_ref   (xmmsv_coll_t *coll)
	void          xmmsv_coll_unref (xmmsv_coll_t *coll)

	void     xmmsv_coll_set_idlist     (xmmsv_coll_t *coll, int *ids)

	void     xmmsv_coll_add_operand    (xmmsv_coll_t *coll, xmmsv_coll_t *op)
	void     xmmsv_coll_remove_operand (xmmsv_coll_t *coll, xmmsv_coll_t *op)
	xmmsv_t *xmmsv_coll_operands_get   (xmmsv_coll_t *coll)

	bint xmmsv_coll_idlist_append    (xmmsv_coll_t *coll, int id)
	bint xmmsv_coll_idlist_insert    (xmmsv_coll_t *coll, int index, int id)
	bint xmmsv_coll_idlist_move      (xmmsv_coll_t *coll, int index, int newindex)
	bint xmmsv_coll_idlist_remove    (xmmsv_coll_t *coll, int index)
	bint xmmsv_coll_idlist_clear     (xmmsv_coll_t *coll)
	bint xmmsv_coll_idlist_get_index (xmmsv_coll_t *coll, int index, int *val)
	bint xmmsv_coll_idlist_set_index (xmmsv_coll_t *coll, int index, int val)
	int  xmmsv_coll_idlist_get_size  (xmmsv_coll_t *coll)

	xmmsv_coll_type_t  xmmsv_coll_get_type   (xmmsv_coll_t *coll)
	xmmsv_t           *xmmsv_coll_idlist_get (xmmsv_coll_t *coll)

	void xmmsv_coll_attribute_set    (xmmsv_coll_t *coll, char *key, char *value)
	bint xmmsv_coll_attribute_remove (xmmsv_coll_t *coll, char *key)
	bint xmmsv_coll_attribute_get    (xmmsv_coll_t *coll, char *key, const_char **value)

	xmmsv_t *xmmsv_coll_attributes_get (xmmsv_coll_t *coll)

	xmmsv_coll_t *xmmsv_coll_universe ()

	xmmsv_coll_t *xmmsv_coll_add_order_operator  (xmmsv_coll_t *coll, const_char *key)
	xmmsv_coll_t *xmmsv_coll_add_order_operators (xmmsv_coll_t *coll, xmmsv_t *order)
	xmmsv_coll_t *xmmsv_coll_add_limit_operator  (xmmsv_coll_t *coll, int lim_start, int lim_len)


cdef extern from "xmmsclient/xmmsclient.h":
	bint xmmsv_coll_parse (char *pattern, xmmsv_coll_t **coll)
