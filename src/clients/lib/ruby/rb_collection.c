/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2014 XMMS2 Team
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

#include <xmmsclient/xmmsclient.h>
#include <xmmsc/xmmsc_idnumbers.h>
#include <xmmscpriv/xmmsc_util.h>

#include <ruby.h>

#include "rb_collection.h"
#include "rb_xmmsclient.h"
#include "rb_result.h"

#define DEF_CONST(mod, prefix, name) \
	rb_define_const ((mod), #name, \
	                 INT2FIX (prefix##name));

#define CHECK_IS_COLL(o) \
	if (!rb_obj_is_kind_of ((o), cColl)) \
		rb_raise (rb_eTypeError, \
		          "wrong argument type %s (expected Collection)", \
		          rb_obj_classname ((o)));

#define COLL_METHOD_HANDLER_HEADER \
	RbCollection *coll = NULL; \
\
	Data_Get_Struct (self, RbCollection, coll); \

#define COLL_METHOD_ADD_HANDLER_RET(action) \
	COLL_METHOD_HANDLER_HEADER \
\
	ret = xmmsv_coll_##action (coll->real); \

typedef struct {
	VALUE attributes;
	VALUE operands;
	xmmsc_coll_t *real;
} RbCollection;

static VALUE cColl, cAttributes, cOperands;
static VALUE eCollectionError, eDisconnectedError, eClientError, ePatternError;

static void
c_free (RbCollection *coll)
{
	xmmsv_unref (coll->real);

	free (coll);
}

static void
c_mark (RbCollection *coll)
{
	rb_gc_mark (coll->attributes);
	rb_gc_mark (coll->operands);
}

static VALUE
c_alloc (VALUE klass)
{
	VALUE obj;
	RbCollection *coll = NULL;

	obj = Data_Make_Struct (klass, RbCollection, c_mark, c_free, coll);

	coll->attributes = Qnil;
	coll->operands = Qnil;

	return obj;
}

VALUE
TO_XMMS_CLIENT_COLLECTION (xmmsc_coll_t *coll)
{
	VALUE self = rb_obj_alloc (cColl);
	RbCollection *rbcoll = NULL;

	Data_Get_Struct (self, RbCollection, rbcoll);

	rbcoll->real = coll;

	/* should we call initialize here?
	 * the problem is, our initialize method creates a new collection,
	 * which we don't want.
	 * otoh i don't know whether we have a sane Ruby object yet here.
	 *
	 * rb_obj_call_init (self, 0, NULL);
	 */

	return self;
}

xmmsc_coll_t *
FROM_XMMS_CLIENT_COLLECTION (VALUE rbcoll)
{
	RbCollection *coll = NULL;

	CHECK_IS_COLL (rbcoll);
	Data_Get_Struct (rbcoll, RbCollection, coll);

	return coll->real;
}

/* call-seq:
 * c = Xmms::Collection.new(type)
 *
 * Returns a new Xmms::Collection object of type _type_.
 */
static VALUE
c_coll_init (VALUE self, VALUE type)
{
	COLL_METHOD_HANDLER_HEADER

	coll->real = xmmsv_new_coll (check_int32 (type));

	return self;
}

/* call-seq:
 * c = Xmms::Collection.universe
 *
 * Returns a collection referencing the "All Media" set.
 */
static VALUE
c_coll_universe (VALUE klass)
{
	VALUE obj = rb_obj_alloc (klass);
	RbCollection *coll = NULL;

	Data_Get_Struct (obj, RbCollection, coll);

	coll->real = xmmsv_new_coll (XMMS_COLLECTION_TYPE_UNIVERSE);

	return obj;
}

/* call-seq:
 * c = Xmms::Collection.parse(pattern)
 *
 * Returns a collection matching a String _pattern_. See the wiki for details
 * on how to construct a pattern string.
 */
static VALUE
c_coll_parse (VALUE klass, VALUE pattern)
{
	VALUE obj = rb_obj_alloc (klass);
	RbCollection *coll = NULL;

	Data_Get_Struct (obj, RbCollection, coll);

	if (!xmmsv_coll_parse (StringValuePtr (pattern), &coll->real)) {
		rb_raise (ePatternError, "invalid pattern");
	}

	return obj;
}

/* call-seq:
 * c.type
 *
 * Returns the type of the collection as an Integer.
 */
static VALUE
c_coll_type_get (VALUE self)
{
	xmmsc_coll_type_t ret;

	COLL_METHOD_ADD_HANDLER_RET (get_type)

	return INT2NUM (ret);
}

/* call-seq:
 * c.operands
 *
 * Gets a list of the operands that make up the collection.
 */
static VALUE
c_coll_operands (VALUE self)
{
	RbCollection *coll = NULL;

	Data_Get_Struct (self, RbCollection, coll);

	if (NIL_P (coll->operands))
		coll->operands = rb_class_new_instance (1, &self, cOperands);

	return coll->operands;
}

/* call-seq:
 * c.idlist
 *
 * Gets the list of media ids that make up the collection.
 */
static VALUE
c_coll_idlist_get (VALUE self)
{
	VALUE ary = rb_ary_new ();
	xmmsv_t *ret = NULL;
	xmmsv_list_iter_t *it = NULL;
	int32_t entry;

	COLL_METHOD_ADD_HANDLER_RET (idlist_get)

	xmmsv_get_list_iter (ret, &it);
	for (xmmsv_list_iter_first (it);
	     xmmsv_list_iter_valid (it);
	     xmmsv_list_iter_next (it)) {

		xmmsv_list_iter_entry_int (it, &entry);
		rb_ary_push (ary, INT2NUM (entry));
	}
	xmmsv_list_iter_explicit_destroy (it);

	return ary;
}

/* call-seq:
 * c.idlist=(_idlist_)
 *
 * Sets the list of media ids that make up the collection.
 */
static VALUE
c_coll_idlist_set (VALUE self, VALUE ids)
{
	int i;
	int *ary = NULL;
	VALUE *rb_ary;
	int rb_ary_len;

	Check_Type (ids, T_ARRAY);
	COLL_METHOD_HANDLER_HEADER

	rb_ary = RARRAY_PTR (ids);
	rb_ary_len = RARRAY_LEN (ids);

	ary = malloc (sizeof (int *) * (rb_ary_len + 1));

	for (i = 0; i < rb_ary_len; i++)
		ary[i] = NUM2INT (rb_ary[i]);

	ary[i] = 0;

	xmmsv_coll_set_idlist (coll->real, ary);

	return self;
}

/* call-seq:
 * c.attributes
 *
 * Returns the attributes of the collection.
 */
static VALUE
c_coll_attributes (VALUE self)
{
	RbCollection *coll = NULL;

	Data_Get_Struct (self, RbCollection, coll);

	if (NIL_P (coll->attributes))
		coll->attributes = rb_class_new_instance (1, &self, cAttributes);

	return coll->attributes;
}

static VALUE
c_attrs_init (VALUE self, VALUE collection)
{
	rb_iv_set (self, "collection", collection);

	return self;
}

#ifdef HAVE_RB_PROTECT_INSPECT
static VALUE
attrs_inspect_cb (VALUE args, VALUE s)
{
	VALUE key, value;

	key = RARRAY (args)->ptr[0];
	value = RARRAY (args)->ptr[1];

	if (RSTRING_LEN (s) > 1)
		rb_str_buf_cat2 (s, ", ");

	rb_str_buf_append (s, rb_inspect (key));
	rb_str_buf_cat2 (s, "=>");
	rb_str_buf_append (s, rb_inspect (value));

	return Qnil;
}

static VALUE
attrs_inspect (VALUE self)
{
	VALUE ret;

	ret = rb_str_new2 ("{");
	rb_iterate (rb_each, self, attrs_inspect_cb, ret);
	rb_str_buf_cat2 (ret, "}");

	OBJ_INFECT (ret, self);

	return ret;
}

static VALUE
c_attrs_inspect (VALUE self)
{
	return rb_protect_inspect (attrs_inspect, self, 0);
}
#endif /* HAVE_RB_PROTECT_INSPECT */

static VALUE
c_attrs_aref (VALUE self, VALUE key)
{
	RbCollection *coll = NULL;
	VALUE tmp;
	int s;
	const char *value;

	StringValue (key);

	tmp = rb_iv_get (self, "collection");
	Data_Get_Struct (tmp, RbCollection, coll);

	s = xmmsv_coll_attribute_get_string (coll->real, StringValuePtr (key), &value);
	if (!s)
		return Qnil;

	return rb_str_new2 (value);
}

static VALUE
c_attrs_aset (VALUE self, VALUE key, VALUE value)
{
	RbCollection *coll = NULL;
	VALUE tmp;

	StringValue (key);
	StringValue (value);

	tmp = rb_iv_get (self, "collection");
	Data_Get_Struct (tmp, RbCollection, coll);

	xmmsv_coll_attribute_set_string (coll->real, StringValuePtr (key),
	                                 StringValuePtr (value));

	return Qnil;
}

static VALUE
c_attrs_has_key (VALUE self, VALUE key)
{
	RbCollection *coll = NULL;
	VALUE tmp;
	int s;

	StringValue (key);

	tmp = rb_iv_get (self, "collection");
	Data_Get_Struct (tmp, RbCollection, coll);

	s = xmmsv_coll_attribute_get_string (coll->real, StringValuePtr (key), NULL);

	return s ? Qtrue : Qfalse;
}

static VALUE
c_attrs_delete (VALUE self, VALUE key)
{
	RbCollection *coll = NULL;
	VALUE tmp;

	StringValue (key);

	tmp = rb_iv_get (self, "collection");
	Data_Get_Struct (tmp, RbCollection, coll);

	xmmsv_coll_attribute_remove (coll->real, StringValuePtr (key));

	return Qnil;
}

static void
attr_each (const char *key, xmmsv_t *value, void *udata)
{
	const char *s;

	xmmsv_get_string (value, &s);

	switch (XPOINTER_TO_INT (udata)) {
		case EACH_PAIR:
			rb_yield_values (2, rb_str_new2 (key), rb_str_new2 (s));
			break;
		case EACH_KEY:
			rb_yield_values (1, rb_str_new2 (key));
			break;
		case EACH_VALUE:
			rb_yield_values (1, rb_str_new2 (s));
			break;
	}
}

static VALUE
c_attrs_each (VALUE self)
{
	RbCollection *coll = NULL;
	xmmsv_t *attributes;
	VALUE tmp;

	tmp = rb_iv_get (self, "collection");
	Data_Get_Struct (tmp, RbCollection, coll);

	attributes = xmmsv_coll_attributes_get (coll->real);

	xmmsv_dict_foreach (attributes, attr_each,
	                    XINT_TO_POINTER (EACH_PAIR));

	return self;
}

static VALUE
c_attrs_each_key (VALUE self)
{
	RbCollection *coll = NULL;
	xmmsv_t *attributes;
	VALUE tmp;

	tmp = rb_iv_get (self, "collection");
	Data_Get_Struct (tmp, RbCollection, coll);

	attributes = xmmsv_coll_attributes_get (coll->real);

	xmmsv_dict_foreach (attributes, attr_each,
	                    XINT_TO_POINTER (EACH_KEY));

	return self;
}

static VALUE
c_attrs_each_value (VALUE self)
{
	RbCollection *coll = NULL;
	xmmsv_t *attributes;
	VALUE tmp;

	tmp = rb_iv_get (self, "collection");
	Data_Get_Struct (tmp, RbCollection, coll);

	attributes = xmmsv_coll_attributes_get (coll->real);

	xmmsv_dict_foreach (attributes, attr_each,
	                    XINT_TO_POINTER (EACH_VALUE));

	return self;
}

static VALUE
c_operands_init (VALUE self, VALUE collection)
{
	rb_iv_set (self, "collection", collection);

	return self;
}

static VALUE
c_operands_push (VALUE self, VALUE arg)
{
	RbCollection *coll = NULL, *coll2 = NULL;
	VALUE tmp;

	tmp = rb_iv_get (self, "collection");
	Data_Get_Struct (tmp, RbCollection, coll);

	Data_Get_Struct (arg, RbCollection, coll2);

	xmmsv_coll_add_operand (coll->real, coll2->real);

	return self;
}

static VALUE
c_operands_delete (VALUE self, VALUE arg)
{
	RbCollection *coll = NULL, *coll2 = NULL;
	VALUE tmp;

	tmp = rb_iv_get (self, "collection");
	Data_Get_Struct (tmp, RbCollection, coll);

	Data_Get_Struct (arg, RbCollection, coll2);

	xmmsv_coll_remove_operand (coll->real, coll2->real);

	return Qnil;
}

static void
operands_each (xmmsv_t *operand, void *user_data)
{
	rb_yield (TO_XMMS_CLIENT_COLLECTION (xmmsv_ref (operand)));
}

static VALUE
c_operands_each (VALUE self)
{
	RbCollection *coll = NULL;
	xmmsv_t *operands_list;
	VALUE tmp;

	tmp = rb_iv_get (self, "collection");
	Data_Get_Struct (tmp, RbCollection, coll);

	operands_list = xmmsv_coll_operands_get (coll->real);

	xmmsv_list_foreach (operands_list, operands_each, NULL);

	return self;
}

void
Init_Collection (VALUE mXmms)
{
	cColl = rb_define_class_under (mXmms, "Collection", rb_cObject);

	rb_define_alloc_func (cColl, c_alloc);

	rb_define_singleton_method (cColl, "universe", c_coll_universe, 0);
	rb_define_singleton_method (cColl, "parse", c_coll_parse, 1);

	rb_define_method (cColl, "initialize", c_coll_init, 1);

	/* type methods */
	rb_define_method (cColl, "type", c_coll_type_get, 0);

	/* idlist methods */
	rb_define_method (cColl, "idlist", c_coll_idlist_get, 0);
	rb_define_method (cColl, "idlist=", c_coll_idlist_set, 1);

	/* operand methods */
	rb_define_method (cColl, "operands", c_coll_operands, 0);

	/* attribute methods */
	rb_define_method (cColl, "attributes", c_coll_attributes, 0);

	rb_define_const (cColl, "NS_ALL", rb_str_new2 (XMMS_COLLECTION_NS_ALL));
	rb_define_const (cColl, "NS_COLLECTIONS",
	                 rb_str_new2 (XMMS_COLLECTION_NS_COLLECTIONS));
	rb_define_const (cColl, "NS_PLAYLISTS",
	                 rb_str_new2 (XMMS_COLLECTION_NS_PLAYLISTS));
	DEF_CONST (cColl, XMMS_COLLECTION_, TYPE_REFERENCE)
	DEF_CONST (cColl, XMMS_COLLECTION_, TYPE_UNIVERSE)
	DEF_CONST (cColl, XMMS_COLLECTION_, TYPE_UNION)
	DEF_CONST (cColl, XMMS_COLLECTION_, TYPE_INTERSECTION)
	DEF_CONST (cColl, XMMS_COLLECTION_, TYPE_COMPLEMENT)
	DEF_CONST (cColl, XMMS_COLLECTION_, TYPE_HAS)
	DEF_CONST (cColl, XMMS_COLLECTION_, TYPE_MATCH)
	DEF_CONST (cColl, XMMS_COLLECTION_, TYPE_TOKEN)
	DEF_CONST (cColl, XMMS_COLLECTION_, TYPE_EQUALS)
	DEF_CONST (cColl, XMMS_COLLECTION_, TYPE_NOTEQUAL)
	DEF_CONST (cColl, XMMS_COLLECTION_, TYPE_SMALLER)
	DEF_CONST (cColl, XMMS_COLLECTION_, TYPE_SMALLEREQ)
	DEF_CONST (cColl, XMMS_COLLECTION_, TYPE_GREATER)
	DEF_CONST (cColl, XMMS_COLLECTION_, TYPE_GREATEREQ)
	DEF_CONST (cColl, XMMS_COLLECTION_, TYPE_ORDER)
	DEF_CONST (cColl, XMMS_COLLECTION_, TYPE_LIMIT)
	DEF_CONST (cColl, XMMS_COLLECTION_, TYPE_MEDIASET)
	DEF_CONST (cColl, XMMS_COLLECTION_, TYPE_IDLIST)
	DEF_CONST (cColl, XMMS_COLLECTION_CHANGED_, ADD)
	DEF_CONST (cColl, XMMS_COLLECTION_CHANGED_, UPDATE)
	DEF_CONST (cColl, XMMS_COLLECTION_CHANGED_, RENAME)
	DEF_CONST (cColl, XMMS_COLLECTION_CHANGED_, REMOVE)

	ePatternError = rb_define_class_under (cColl, "PatternError",
	                                       rb_eStandardError);
	eCollectionError = rb_define_class_under (cColl, "CollectionError",
	                                          rb_eStandardError);
	eClientError = rb_define_class_under (cColl, "ClientError",
	                                      rb_eStandardError);
	eDisconnectedError = rb_define_class_under (cColl, "DisconnectedError",
	                                            eClientError);

	cAttributes = rb_define_class_under (cColl, "Attributes", rb_cObject);

	rb_define_method (cAttributes, "initialize", c_attrs_init, 1);
#ifdef HAVE_RB_PROTECT_INSPECT
	rb_define_method (cAttributes, "inspect", c_attrs_inspect, 0);
#endif /* HAVE_RB_PROTECT_INSPECT */

	rb_define_method (cAttributes, "[]", c_attrs_aref, 1);
	rb_define_method (cAttributes, "[]=", c_attrs_aset, 2);
	rb_define_method (cAttributes, "has_key?", c_attrs_has_key, 1);
	rb_define_method (cAttributes, "delete", c_attrs_delete, 1);
	rb_define_method (cAttributes, "each", c_attrs_each, 0);
	rb_define_method (cAttributes, "each_key", c_attrs_each_key, 0);
	rb_define_method (cAttributes, "each_value", c_attrs_each_value, 0);

	rb_define_alias (cAttributes, "include?", "has_key?");
	rb_define_alias (cAttributes, "key?", "has_key?");
	rb_define_alias (cAttributes, "member?", "has_key?");
	rb_define_alias (cAttributes, "each_pair", "each");

	rb_include_module (cAttributes, rb_mEnumerable);

	cOperands = rb_define_class_under (cColl, "Operands", rb_cObject);

	rb_define_method (cOperands, "initialize", c_operands_init, 1);
	rb_define_method (cOperands, "push", c_operands_push, 1);
	rb_define_method (cOperands, "delete", c_operands_delete, 1);
	rb_define_method (cOperands, "each", c_operands_each, 0);

	rb_define_alias (cOperands, "<<", "push");

	rb_include_module (cOperands, rb_mEnumerable);
}
