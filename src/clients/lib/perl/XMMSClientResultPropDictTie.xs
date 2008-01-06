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
			XSRETURN_UNDEF;
		}

		key  = hv_iterkey (iter, &key_len);
		RETVAL = newSVpv (key, key_len);
	OUTPUT:
		RETVAL

SV *
FETCH (sv, key)
		SV *sv
		char *key
	PREINIT:
		int ret = 0;
		MAGIC *mg;
		xmmsc_result_propdict_t *res = NULL;
		SV **overrides;
	INIT:
		RETVAL = NULL;
	CODE:
		overrides = hv_fetch ((HV *)SvRV (sv), "overrides", 9, 0);

		if (overrides && *overrides) {
			SV **val = hv_fetch ((HV *)SvRV (*overrides), key, strlen (key), 0);

			if (val) {
				RETVAL = *val;
			}
		}

		if (!RETVAL) {
			if (!(mg = perl_xmmsclient_get_magic_from_sv (sv, "Audio::XMMSClient::Result::PropDict::Tie"))) {
				croak ("This is a bug!");
			}

			res = (xmmsc_result_propdict_t *)mg->mg_ptr;

			switch (xmmsc_result_get_dict_entry_type (res, key)) {
				case XMMS_OBJECT_CMD_ARG_UINT32:
					{
						uint32_t val;
						ret = xmmsc_result_get_dict_entry_uint (res, key, &val);
						RETVAL = newSVuv (val);
						break;
					}
				case XMMS_OBJECT_CMD_ARG_INT32:
					{
						int32_t val;
						ret = xmmsc_result_get_dict_entry_int (res, key, &val);
						RETVAL = newSViv (val);
						break;
					}
				case XMMS_OBJECT_CMD_ARG_STRING:
					{
						const char *val;
						ret = xmmsc_result_get_dict_entry_string (res, key, &val);
						RETVAL = newSVpv (val, 0);
						break;
					}
				default:
					RETVAL = &PL_sv_undef;
			}

			if (ret != 1) {
				RETVAL = &PL_sv_undef;
			}
		}
	OUTPUT:
		RETVAL

void
STORE (sv, key, value)
		SV *sv
		SV *key
		SV *value
	PREINIT:
		SV **he;
		HV *overrides;
	CODE:
		he = hv_fetch ((HV *)SvRV (sv), "overrides", 9, 0);

		if (!he || !*he) {
			overrides = newHV ();
			hv_store ((HV *)SvRV (sv), "overrides", 9, newRV_inc ((SV *)overrides), 0);
		}
		else {
			overrides = (HV *)SvRV (*he);
		}

		hv_store_ent (overrides, key, newSVsv (value), 0);

BOOT:
	PERL_UNUSED_VAR (items);
