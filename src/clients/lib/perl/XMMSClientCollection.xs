#include "perl_xmmsclient.h"

MODULE = Audio::XMMSClient::Collection	PACKAGE = Audio::XMMSClient::Collection	PREFIX = xmmsc_coll_

xmmsc_coll_t *
xmmsc_coll_new (class, type)
		xmmsc_coll_type_t type
	C_ARGS:
		type

void
DESTROY (coll)
		xmmsc_coll_t *coll
	CODE:
		xmmsc_coll_unref (coll);

void
xmmsc_coll_set_type (coll, type)
		xmmsc_coll_t *coll
		xmmsc_coll_type_t type

void
xmmsc_coll_set_idlist (coll, ...)
		xmmsc_coll_t *coll
	PREINIT:
		int i;
		unsigned int *ids;
	CODE:
		ids = (unsigned int *)malloc (sizeof(unsigned int) * items);

		for (i = 0; i < items - 1; i++) {
			ids[i] = SvUV(ST(i + 1));
		}

		ids[items - 1] = 0;

		xmmsc_coll_set_idlist (coll, ids);

void
xmmsc_coll_add_operand (coll, op)
		xmmsc_coll_t *coll
		xmmsc_coll_t *op

void
xmmsc_coll_remove_operand (coll, op)
		xmmsc_coll_t *coll
		xmmsc_coll_t *op

int
xmmsc_coll_idlist_append (coll, id)
		xmmsc_coll_t *coll
		unsigned int id

int
xmmsc_coll_idlist_insert (coll, id, index)
		xmmsc_coll_t *coll
		unsigned int id
		unsigned int index

int
xmmsc_coll_idlist_move (coll, from, to)
		xmmsc_coll_t *coll
		unsigned int from
		unsigned int to

int
xmmsc_coll_idlist_clear (coll)
		xmmsc_coll_t *coll

SV *
xmmsc_coll_idlist_get_index (coll, index)
		xmmsc_coll_t *coll
		unsigned int index
	PREINIT:
		uint32_t val;
	CODE:
		if (xmmsc_coll_idlist_get_index (coll, index, &val) != 0) {
			RETVAL = &PL_sv_undef;
		}
		else {
			RETVAL = sv_2mortal (newSVuv (val));
		}
	OUTPUT:
		RETVAL

int
xmmsc_coll_idlist_set_index (coll, index, val)
		xmmsc_coll_t *coll
		unsigned int index
		uint32_t val

size_t
xmmsc_coll_idlist_get_size (coll)
		xmmsc_coll_t *coll

xmmsc_coll_type_t
xmmsc_coll_get_type (coll)
		xmmsc_coll_t *coll

void
xmmsc_coll_get_idlist (coll)
		xmmsc_coll_t *coll
	PREINIT:
		uint32_t *ret;
		size_t size;
		unsigned int i = 0;
	PPCODE:
		ret = xmmsc_coll_get_idlist (coll);

		if (ret == NULL)
			XSRETURN_UNDEF;

		size = xmmsc_coll_idlist_get_size (coll);
		EXTEND (sp, size);

		while (ret[i] != 0) {
			PUSHs (sv_2mortal (newSVuv (ret[i])));
			++i;
		}

int
xmmsc_coll_operand_list_first (coll)
		xmmsc_coll_t *coll

int
xmmsc_coll_operand_list_valid (coll)
		xmmsc_coll_t *coll

xmmsc_coll_t *
xmmsc_coll_operand_list_entry (coll)
		xmmsc_coll_t *coll
	CODE:
		if (xmmsc_coll_operand_list_entry (coll, &RETVAL) != 0)
			XSRETURN_UNDEF;
	OUTPUT:
		RETVAL

int
xmmsc_coll_operand_list_next (coll)
		xmmsc_coll_t *coll

int
xmmsc_coll_operand_list_save (coll)
		xmmsc_coll_t *coll

int
xmmsc_coll_operand_list_restore (coll)
		xmmsc_coll_t *coll

void
xmmsc_coll_attribute_set (coll, key, value)
		xmmsc_coll_t *coll
		const char *key
		const char *value

int
xmmsc_coll_attribute_remove (coll, key)
		xmmsc_coll_t *coll
		const char *key

char *
xmmsc_coll_attribute_get (coll, key)
		xmmsc_coll_t *coll
		const char *key
	CODE:
		if (xmmsc_coll_attribute_get (coll, key, &RETVAL) != 0)
			XSRETURN_UNDEF;
	OUTPUT:
		RETVAL
