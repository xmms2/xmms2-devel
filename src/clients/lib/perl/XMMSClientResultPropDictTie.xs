#include "perl_xmmsclient.h"

#define xmmsc_result_propdict_t xmmsc_result_t

void
perl_xmmsclient_extract_keys_from_propdict (const void *key,
                                            xmmsc_result_value_type_t type,
                                            const void *value,
                                            const char *source,
                                            void *user_data)
{
	HV *keys = (HV *)user_data;

	hv_store (keys, key, strlen (key), &PL_sv_undef, 0);
}

HV *
perl_xmmsclient_get_keys_if_needed (SV *sv)
{
	HV *keys;
	SV **hash_elem;
	int ret = 0;
	MAGIC *mg = NULL;
	xmmsc_result_propdict_t *res = NULL;

	hash_elem = hv_fetch ((HV *)SvRV (sv), "keys", 4, 0);

	if (hash_elem == NULL || *hash_elem == NULL) {
		if (!(mg = perl_xmmsclient_get_magic_from_sv (sv, "Audio::XMMSClient::Result::PropDict::Tie"))) {
			croak ("This is a bug!");
		}

		res = (xmmsc_result_propdict_t *)mg->mg_ptr;

		keys = newHV ();
		ret = xmmsc_result_propdict_foreach (res, perl_xmmsclient_extract_keys_from_propdict, keys);

		hash_elem = hv_store ((HV *)SvRV (sv), "keys", 4, newRV_noinc ((SV *)keys), 0);
	}

	keys = (HV *)SvRV (*hash_elem);
	return keys;
}

MODULE = Audio::XMMSClient::Result::PropDict::Tie	PACKAGE = Audio::XMMSClient::Result::PropDict::Tie

SV *
FIRSTKEY (sv)
		SV *sv
	PREINIT:
		HV *keys;
		HE *iter;
		char *key;
		I32 key_len;
	CODE:
		keys = perl_xmmsclient_get_keys_if_needed (sv);

		hv_iterinit (keys);
		iter = hv_iternext (keys);
		key  = hv_iterkey (iter, &key_len);

		RETVAL = newSVpv (key, key_len);
	OUTPUT:
		RETVAL

SV *
NEXTKEY (sv, lastkey)
		SV *sv
	PREINIT:
		HV *keys;
		HE *iter;
		char *key;
		I32 key_len;
	CODE:
		keys = perl_xmmsclient_get_keys_if_needed (sv);

		iter = hv_iternext (keys);

		if (iter == NULL) {
			RETVAL = &PL_sv_undef;
		}
		else {
			key  = hv_iterkey (iter, &key_len);
			RETVAL = newSVpv (key, key_len);
		}
	OUTPUT:
		RETVAL

SV *
FETCH (res, key)
		xmmsc_result_propdict_t *res
		char *key
	PREINIT:
		int ret = 0;
		uint32_t uint32_val;
		int32_t int32_val;
		char *string_val;
	CODE:
		switch (xmmsc_result_get_dict_entry_type (res, key)) {
			case XMMS_OBJECT_CMD_ARG_UINT32:
				ret = xmmsc_result_get_dict_entry_uint (res, key, &uint32_val);

				RETVAL = newSVuv (uint32_val);
				break;
			case XMMS_OBJECT_CMD_ARG_INT32:
				ret = xmmsc_result_get_dict_entry_int (res, key, &int32_val);
				RETVAL = newSViv (int32_val);
				break;
			case XMMS_OBJECT_CMD_ARG_STRING:
				ret = xmmsc_result_get_dict_entry_string (res, key, &string_val);
				RETVAL = newSVpv (string_val, 0);
				break;
			default:
				RETVAL = &PL_sv_undef;
		}

		if (ret != 1) {
			RETVAL = &PL_sv_undef;
		}
	OUTPUT:
		RETVAL

BOOT:
	PERL_UNUSED_VAR (items);
