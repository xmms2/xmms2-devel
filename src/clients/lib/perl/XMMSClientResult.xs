#include "perl_xmmsclient.h"

void
perl_xmmsclient_xmmsc_result_notifyer_cb(xmmsc_result_t* res, void* user_data) {
	PerlXMMSClientCallback* cb = (PerlXMMSClientCallback*)user_data;

	perl_xmmsclient_callback_invoke(cb, res);
}

SV*
perl_xmmsclient_xmmsc_result_get_uint(xmmsc_result_t* res) {
	int ret;
	unsigned int val;

	ret = xmmsc_result_get_uint(res, &val);

	if (ret == 0)
		croak("Could not fetch uint value");

	return newSVuv(val);
}

SV*
perl_xmmsclient_xmmsc_result_get_int(xmmsc_result_t* res) {
	int ret, val;

	ret = xmmsc_result_get_int(res, &val);

	if (ret == 0)
		croak("Could not fetch int value");

	return newSViv(val);
}

SV*
perl_xmmsclient_xmmsc_result_get_string(xmmsc_result_t* res) {
	int ret;
	char* val = NULL;

	ret = xmmsc_result_get_string(res, &val);

	if (ret == 0)
		croak("Could not fetch string value");

	return newSVpv(val, 0);
}

SV*
perl_xmmsclient_xmmsc_result_get_coll(xmmsc_result_t* res) {
	int ret;
	xmmsc_coll_t* coll = NULL;

	ret = xmmsc_result_get_collection(res, &coll);

	if (ret == 0)
		croak("Could not fetch collection value");

	return perl_xmmsclient_new_sv_from_ptr((void *)coll, "Audio::XMMSClient::Collection");
}

SV*
perl_xmmsclient_xmmsc_result_get_bin(xmmsc_result_t* res) {
	int ret;
	unsigned char* bin;
	unsigned int bin_len = 0;

	ret = xmmsc_result_get_bin(res, &bin, &bin_len);

	if (ret == 0)
		croak("Could not fetch bin value");

	return newSVpv((char*)bin, bin_len);
}

SV*
perl_xmmsclient_xmms_result_cast_value(xmmsc_result_value_type_t type, const void* value) {
	SV* perl_value;

	switch (type) {
		case XMMSC_RESULT_VALUE_TYPE_STRING:
			perl_value = newSVpv((const char*)value, 0);
			break;
		case XMMSC_RESULT_VALUE_TYPE_INT32:
			perl_value = newSViv((int)value);
			break;
		case XMMSC_RESULT_VALUE_TYPE_UINT32:
			perl_value = newSVuv((unsigned int)value);
			break;
		default:
			perl_value = &PL_sv_undef;
	}

	return perl_value;
}

void
perl_xmmsclient_xmmsc_result_dict_foreach_cb(const void* key, xmmsc_result_value_type_t type, const void* value, void* user_data) {
	HV* hash = (HV*)user_data;
	
	hv_store(hash, (const char*)key, strlen((const char*)key), perl_xmmsclient_xmms_result_cast_value(type, value), 0);
}

SV*
perl_xmmsclient_xmmsc_result_get_dict(xmmsc_result_t* res) {
	int ret;
	HV* val = newHV();

	ret = xmmsc_result_dict_foreach(res, perl_xmmsclient_xmmsc_result_dict_foreach_cb, val);

	if (ret == 0)
		croak("Could not fetch dict value");

	return newRV_inc((SV*)val);
}

SV*
perl_xmmsclient_xmmsc_result_get_propdict(xmmsc_result_t* res) {
	SV* hash;
	SV* tie;
	
	xmmsc_result_ref(res);

	tie = perl_xmmsclient_new_sv_from_ptr(res, "Audio::XMMSClient::Result::PropDict::Tie");
	hash = perl_xmmsclient_new_sv_from_ptr(res, "Audio::XMMSClient::Result::PropDict");
	hv_magic((HV*)SvRV(hash), (GV*)tie, PERL_MAGIC_tied);

	return hash;
}

SV*
perl_xmmsclient_result_get_value(xmmsc_result_t* res) {
	SV* ret;

	switch (xmmsc_result_get_type(res)) {
		case XMMSC_RESULT_VALUE_TYPE_UINT32:
			ret = perl_xmmsclient_xmmsc_result_get_uint(res);
			break;
		case XMMSC_RESULT_VALUE_TYPE_INT32:
			ret = perl_xmmsclient_xmmsc_result_get_int(res);
			break;
		case XMMSC_RESULT_VALUE_TYPE_STRING:
			ret = perl_xmmsclient_xmmsc_result_get_string(res);
			break;
		case XMMSC_RESULT_VALUE_TYPE_COLL:
			ret = perl_xmmsclient_xmmsc_result_get_coll(res);
			break;
		case XMMSC_RESULT_VALUE_TYPE_BIN:
			ret = perl_xmmsclient_xmmsc_result_get_bin(res);
			break;
		case XMMS_OBJECT_CMD_ARG_DICT:
			ret = perl_xmmsclient_xmmsc_result_get_dict(res);
			break;
		case XMMS_OBJECT_CMD_ARG_PROPDICT:
			ret = perl_xmmsclient_xmmsc_result_get_propdict(res);
			break;
		default:
			ret = &PL_sv_undef;
	}

	return ret;
}

SV*
perl_xmmsclient_result_get_list(xmmsc_result_t* res) {
	AV* list = newAV();

	while (xmmsc_result_list_valid(res)) {
		av_push(list, perl_xmmsclient_result_get_value(res));
		xmmsc_result_list_next(res);
	}

	return newRV_inc((SV*)list);
}

MODULE = Audio::XMMSClient::Result	PACKAGE = Audio::XMMSClient::Result	PREFIX = xmmsc_result_

xmmsc_result_type_t
xmmsc_result_get_class(res)
		xmmsc_result_t* res

void
xmmsc_result_disconnect(res)
		xmmsc_result_t* res

void
xmmsc_result_restart(sv)
		SV* sv
	PREINIT:
		MAGIC* mg;
		xmmsc_result_t *res, *res2;
	CODE:
		res = perl_xmmsclient_get_ptr_from_sv(sv, "Audio::XMMSClient::Result");

		res2 = xmmsc_result_restart(res);
		xmmsc_result_unref(res);

		mg = perl_xmmsclient_get_magic_from_sv(sv, "Audio::XMMSClient::Result");
		mg->mg_ptr = (char*)res2;

#private api?
#uint32_t
#xmmsc_result_cookie_get(res)
#		xmmsc_result_t* res

void
xmmsc_result_notifier_set(res, func, data=NULL)
		SV* res
		SV* func
		SV* data
	PREINIT:
		PerlXMMSClientCallback* cb = NULL;
		PerlXMMSClientCallbackParamType param_types[1];
		xmmsc_result_t* c_res;
	CODE:
		/*
		 * TODO: free the PerlXMMSClientCallback if it is already set. The
		 * current xmmsclient API doesn't allow that, though.
		 */

		c_res = (xmmsc_result_t*)perl_xmmsclient_get_ptr_from_sv(res, "Audio::XMMSClient::Result");
		param_types[0] = PERL_XMMSCLIENT_CALLBACK_PARAM_TYPE_RESULT;
		
		cb = perl_xmmsclient_callback_new(func, data, res, 1, param_types);

		xmmsc_result_notifier_set(c_res, perl_xmmsclient_xmmsc_result_notifyer_cb, cb);

SV*
xmmsc_result_wait(res)
		SV* res
	PREINIT:
		xmmsc_result_t* c_res;
	CODE:
		c_res = (xmmsc_result_t*)perl_xmmsclient_get_ptr_from_sv(res, "Audio::XMMSClient::Result");

		xmmsc_result_wait(c_res);
		RETVAL = res;
	OUTPUT:
		RETVAL

void
xmmsc_result_source_preference_set(res, ...)
		xmmsc_result_t* res
	PREINIT:
		char** preference = NULL;
		int i;
	CODE:
		preference = (char**)malloc(sizeof(char*) * items - 1);

		for (i = 0; i < items - 1; i++) {
			preference[i] = SvPV_nolen(ST(i+1));
		}

		preference[items - 1] = NULL;

		xmmsc_result_source_preference_set(res, (const char**)preference);

		free(preference);

xmms_object_cmd_arg_type_t
xmmsc_result_get_type(res)
		xmmsc_result_t* res

int
xmmsc_result_iserror(res)
		xmmsc_result_t* res

const char*
xmmsc_result_get_error(res)
		xmmsc_result_t* res

SV*
value(res)
		xmmsc_result_t* res
	CODE:
		if (xmmsc_result_iserror(res)) {
			RETVAL = &PL_sv_undef;
		}

		if (xmmsc_result_is_list(res)) {
			RETVAL = perl_xmmsclient_result_get_list(res);
		} else {
			RETVAL = perl_xmmsclient_result_get_value(res);
		}
	OUTPUT:
		RETVAL

const char*
xmmsc_result_decode_url(res, string)
		xmmsc_result_t* res
		const char* string

void
DESTROY(res)
		xmmsc_result_t* res
	CODE:
		xmmsc_result_unref(res);
