#include "perl_xmmsclient.h"

void
perl_xmmsclient_xmmsc_result_propdict_foreach_cb(const void* key, xmmsc_result_value_type_t type, const void* value, const char* source, void* user_data) {
	HV* hash = (HV*)user_data;
	HV* subhash;

	if (hv_exists(hash, source, strlen(source))) {
		SV** sv = hv_fetch(hash, source, strlen(source), 0);

		if (!*sv || !SvOK(*sv) || !SvROK(*sv) || !(SvTYPE(SvRV(*sv)) == SVt_PVHV))
			croak("Hash element is not an array reference");

		subhash = (HV*)SvRV(*sv);
	} else {
		subhash = newHV();

		hv_store(hash, source, strlen(source), newRV_inc((SV*)subhash), 0);
	}

	hv_store(subhash, (const char*)key, strlen((const char*)key), perl_xmmsclient_xmms_result_cast_value(type, value), 0);
}

MODULE = Audio::XMMSClient::Result::PropDict	PACKAGE = Audio::XMMSClient::Result::PropDict

void
set_source_preference(sv, ...)
		SV* sv
	PREINIT:
		int i;
		MAGIC* mg = NULL;
		xmmsc_result_t* res;
		char** prefs;
	CODE:
		if (!(mg = perl_xmmsclient_get_magic_from_sv(sv, "Audio::XMMSClient::Result::PropDict"))) {
			croak("bug");
		}
		res = (xmmsc_result_t*)mg->mg_ptr;

		prefs = (char**)malloc( sizeof(char*) * items );
		for (i = 1; i < items; i++) {
			prefs[i] = SvPV_nolen(ST(i));
		}

		prefs[items] = NULL;
		xmmsc_result_source_preference_set(res, (const char**)prefs);

SV*
source_hash(sv)
		SV* sv
	PREINIT:
		int ret;
		HV* val;
		MAGIC* mg = NULL;
		xmmsc_result_t* res;
	CODE:
		if (!(mg = perl_xmmsclient_get_magic_from_sv(sv, "Audio::XMMSClient::Result::PropDict"))) {
			croak("bug");
		}
		res = (xmmsc_result_t*)mg->mg_ptr;

		val = newHV();
		ret = xmmsc_result_propdict_foreach(res, perl_xmmsclient_xmmsc_result_propdict_foreach_cb, val);

		if (ret == 0)
			croak("Could not fetch dict value");

		RETVAL = newRV_noinc((SV*)val);
	OUTPUT:
		RETVAL
