/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2007 XMMS2 Team
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

#include <ruby.h>

#include "rb_collection.h"
#include "rb_xmmsclient.h"
#include "rb_result.h"

#define DEF_CONST(mod, prefix, name) \
	rb_define_const ((mod), #name, \
	                 INT2FIX (prefix##name));

#define COLL_METHOD_HANDLER_HEADER \
	RbCollection *coll = NULL; \
\
	Data_Get_Struct (self, RbCollection, coll); \

#define COLL_METHOD_ADD_HANDLER_RET(action) \
	COLL_METHOD_HANDLER_HEADER \
\
	ret = xmmsc_coll_##action (coll->real); \

#define COLL_METHOD_ADD_HANDLER_UINT(action, arg1) \
	COLL_METHOD_HANDLER_HEADER \
\
	xmmsc_coll_##action (coll->real, check_uint32 (arg1)); \

#define COLL_METHOD_ADD_HANDLER_COLL(action, arg1) \
	RbCollection *arg = NULL; \
	COLL_METHOD_HANDLER_HEADER \
\
	/* FIXME: check that we actually have a Collection object */ \
	Data_Get_Struct (arg1, RbCollection, arg); \
\
	xmmsc_coll_##action (coll->real, arg->real); \

#define COLL_METHOD_ADD_HANDLER_OPERATOR(operand, operation) \
	xmmsc_coll_t *ucoll = NULL; \
	RbCollection *opcoll = NULL; \
	COLL_METHOD_HANDLER_HEADER \
\
	/* FIXME: Check that we actually have a Collection object */ \
	Data_Get_Struct (operand, RbCollection, opcoll); \
\
	ucoll = xmmsc_coll_new (XMMS_COLLECTION_TYPE_##operation); \
	xmmsc_coll_add_operand (ucoll, coll->real); \
	xmmsc_coll_add_operand (ucoll, opcoll->real); \
\
	return TO_XMMS_CLIENT_COLLECTION (ucoll); \

typedef struct {
	xmmsc_coll_t *real;
} RbCollection;

static VALUE cColl, eCollectionError, eDisconnectedError, eClientError;

static void
c_free (RbCollection *coll)
{
	xmmsc_coll_unref (coll->real);

	free (coll);
}

static VALUE
c_alloc (VALUE klass)
{
	RbCollection *coll = NULL;

	return Data_Make_Struct (klass, RbCollection, NULL, c_free, coll);
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

	/* FIXME: Check that we actually have a Collection object */
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

	coll->real = xmmsc_coll_new (check_uint32 (type));

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

	coll->real = xmmsc_coll_universe ();

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

	return UINT2NUM (ret);
}

/* call-seq:
 * c.type=(type)
 *
 * Sets the type of the collection according to _type_.
 */
static VALUE
c_coll_type_set (VALUE self, VALUE type)
{
	COLL_METHOD_ADD_HANDLER_UINT (set_type, type)

	return self;
}

/* call-seq:
 * c.operands
 *
 * Gets a list of the operands that make up the collection.
 */
static VALUE
c_coll_operands (VALUE self)
{
	VALUE arr = rb_ary_new ();
	xmmsc_coll_t *operand = NULL;
	COLL_METHOD_HANDLER_HEADER

	if (!xmmsc_coll_operand_list_first (coll->real))
		return Qnil;

	for (; xmmsc_coll_operand_list_valid (coll->real);
	     xmmsc_coll_operand_list_next (coll->real))
	{
		xmmsc_coll_operand_list_entry (coll->real, &operand);
		rb_ary_push (arr, TO_XMMS_CLIENT_COLLECTION (operand));
	}

	return arr;
}

/* call-seq:
 * c.operand_add(op)
 *
 * Adds an operand (another collection) _op_ to the current collection.
 */
static VALUE
c_coll_operand_add (VALUE self, VALUE op)
{
	COLL_METHOD_ADD_HANDLER_COLL (add_operand, op)

	return self;
}

/* call-seq:
 * c.operand_remove(op)
 *
 * Removes an operand (another collection) _op_ from the current collection.
 */
static VALUE
c_coll_operand_remove (VALUE self, VALUE op)
{
	COLL_METHOD_ADD_HANDLER_COLL (remove_operand, op)

	return self;
}

/* call-seq:
 * c.idlist
 *
 * Gets the list of media ids that make up the collection.
 */
static VALUE
c_coll_idlist_get (VALUE self)
{
	int i;
	VALUE ary = rb_ary_new ();
	uint32_t *ret = NULL;

	COLL_METHOD_ADD_HANDLER_RET (get_idlist)

	for (i = 0; ret[i]; i++)
		rb_ary_push (ary, UINT2NUM (ret[i]));

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
	unsigned int *ary = NULL;
	struct RArray *rb_ary = NULL;

	Check_Type (ids, T_ARRAY);
	COLL_METHOD_HANDLER_HEADER

	rb_ary = RARRAY (ids);
	ary = malloc (sizeof (unsigned int *) * (rb_ary->len + 1));

	for (i = 0; i < rb_ary->len; i++)
		ary[i] = NUM2UINT (rb_ary->ptr[i]);

	ary[i] = 0;

	xmmsc_coll_set_idlist (coll->real, ary);

	return self;
}

/* call-seq:
 * c |(v)
 *
 * Returns a new collection that is the logical OR of _c_ and _v_.
 */
static VALUE
c_coll_union (VALUE self, VALUE op)
{
	COLL_METHOD_ADD_HANDLER_OPERATOR (op, UNION)
}

/* call-seq:
 * c &(v)
 *
 * Returns a new collection that is the logical AND of _c_ and _v_.
 */
static VALUE
c_coll_intersect (VALUE self, VALUE op)
{
	COLL_METHOD_ADD_HANDLER_OPERATOR (op, INTERSECTION)
}

/* call-seq:
 * ~c
 *
 * Returns a new collection that is the logical complement of _c_.
 */
static VALUE
c_coll_complement (VALUE self)
{
	xmmsc_coll_t *ucoll = NULL;
	COLL_METHOD_HANDLER_HEADER

	ucoll = xmmsc_coll_new (XMMS_COLLECTION_TYPE_COMPLEMENT);
	xmmsc_coll_add_operand (ucoll, coll->real);

	return TO_XMMS_CLIENT_COLLECTION (ucoll);
}

/* call-seq:
 * c.attributes
 *
 * Gets a hash of the attributes that make up the collection.
 */
static VALUE
c_coll_attributes (VALUE self)
{
	VALUE hash = rb_hash_new ();
	const char *key, *value = NULL;
	COLL_METHOD_HANDLER_HEADER

	for (xmmsc_coll_attribute_list_first (coll->real);
	     xmmsc_coll_attribute_list_valid (coll->real);
	     xmmsc_coll_attribute_list_next (coll->real))
	{
		xmmsc_coll_attribute_list_entry (coll->real, &key, &value);
		rb_hash_aset (hash, ID2SYM (rb_intern (key)),
		              ID2SYM (rb_intern (value)));
	}

	return hash;
}

void
Init_Collection (VALUE mXmms)
{
	cColl = rb_define_class_under (mXmms, "Collection", rb_cObject);

	rb_define_alloc_func (cColl, c_alloc);

	rb_define_singleton_method (cColl, "universe", c_coll_universe, 0);

	rb_define_method (cColl, "initialize", c_coll_init, 1);

	/* type methods */
	rb_define_method (cColl, "type", c_coll_type_get, 0);
	rb_define_method (cColl, "type=", c_coll_type_set, 1);

	/* idlist methods */
	rb_define_method (cColl, "idlist", c_coll_idlist_get, 0);
	rb_define_method (cColl, "idlist=", c_coll_idlist_set, 1);

	/* operand methods */
	rb_define_method (cColl, "operands", c_coll_operands, 0);
	rb_define_method (cColl, "operand_add", c_coll_operand_add, 1);
	rb_define_method (cColl, "operand_remove", c_coll_operand_remove, 1);

	/* attribute methods */
	rb_define_method (cColl, "attributes", c_coll_attributes, 0);

	/* operator methods */
	rb_define_method (cColl, "union", c_coll_union, 1);
	rb_define_method (cColl, "intersect", c_coll_intersect, 1);
	rb_define_method (cColl, "complement", c_coll_complement, 0);

	rb_define_alias (cColl, "or", "union");
	rb_define_alias (cColl, "and", "intersect");
	rb_define_alias (cColl, "not", "complement");
	rb_define_alias (cColl, "|", "union");
	rb_define_alias (cColl, "&", "intersect");
	rb_define_alias (cColl, "~@", "complement");

	rb_define_const (cColl, "NS_ALL", rb_str_new2 (XMMS_COLLECTION_NS_ALL));
	rb_define_const (cColl, "NS_COLLECTIONS",
	                 rb_str_new2 (XMMS_COLLECTION_NS_COLLECTIONS));
	rb_define_const (cColl, "NS_PLAYLISTS",
	                 rb_str_new2 (XMMS_COLLECTION_NS_PLAYLISTS));
	DEF_CONST (cColl, XMMS_COLLECTION_, TYPE_REFERENCE)
	DEF_CONST (cColl, XMMS_COLLECTION_, TYPE_UNION)
	DEF_CONST (cColl, XMMS_COLLECTION_, TYPE_INTERSECTION)
	DEF_CONST (cColl, XMMS_COLLECTION_, TYPE_COMPLEMENT)
	DEF_CONST (cColl, XMMS_COLLECTION_, TYPE_HAS)
	DEF_CONST (cColl, XMMS_COLLECTION_, TYPE_EQUALS)
	DEF_CONST (cColl, XMMS_COLLECTION_, TYPE_MATCH)
	DEF_CONST (cColl, XMMS_COLLECTION_, TYPE_SMALLER)
	DEF_CONST (cColl, XMMS_COLLECTION_, TYPE_GREATER)
	DEF_CONST (cColl, XMMS_COLLECTION_, TYPE_IDLIST)
	DEF_CONST (cColl, XMMS_COLLECTION_, TYPE_QUEUE)
	DEF_CONST (cColl, XMMS_COLLECTION_, TYPE_PARTYSHUFFLE)

	eCollectionError = rb_define_class_under (cColl, "CollectionError",
	                                        rb_eStandardError);
	eClientError = rb_define_class_under (cColl, "ClientError",
	                                      rb_eStandardError);
	eDisconnectedError = rb_define_class_under (cColl, "DisconnectedError",
	                                            eClientError);
}
