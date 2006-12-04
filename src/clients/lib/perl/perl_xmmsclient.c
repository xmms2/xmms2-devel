#include "perl_xmmsclient.h"

SV*
perl_xmmsclient_new_sv_from_ptr(void* ptr, const char* class) {
	SV* obj;
	SV* sv;
	HV* stash;

	obj = (SV*)newHV();
	sv_magic(obj, 0, PERL_MAGIC_ext, (const char*)ptr, 0);
	sv = newRV_noinc(obj);
	stash = gv_stashpv(class, 0);
	sv_bless(sv, stash);

	return sv;
}

void*
perl_xmmsclient_get_ptr_from_sv(SV* sv, const char* class) {
	MAGIC* mg;

	if (!(mg = perl_xmmsclient_get_magic_from_sv(sv, class)))
		return NULL;

	return (void*)mg->mg_ptr;
}

MAGIC*
perl_xmmsclient_get_magic_from_sv(SV* sv, const char* class) {
	MAGIC* mg;

	if (!sv || !SvOK(sv) || !SvROK(sv) || !sv_derived_from(sv, class) || !(mg = mg_find(SvRV(sv), PERL_MAGIC_ext)))
		return NULL;

	return mg;
}

void
_perl_xmmsclient_call_xs(pTHX_ void (*subaddr) (pTHX_ CV*), CV* cv, SV** mark) {
	dSP;
	PUSHMARK(mark);
	(*subaddr) (aTHX_ cv);
	PUTBACK;
}

PerlXMMSClientCallback*
perl_xmmsclient_callback_new(SV* func, SV* data, SV* wrapper, int n_params, PerlXMMSClientCallbackParamType param_types[]) {
	PerlXMMSClientCallback* cb;

	cb = (PerlXMMSClientCallback*)malloc(sizeof(PerlXMMSClientCallback));

	cb->func = newSVsv(func);

	if (data)
		cb->data = newSVsv(data);

	if (wrapper)
		cb->wrapper = newSVsv(wrapper);

	cb->n_params = n_params;

	if (cb->n_params) {
		if (!param_types)
			croak("n_params is %d but param_types is NULL in perl_xmmsclient_callback_new", n_params);

		cb->param_types = (PerlXMMSClientCallbackParamType*)malloc(sizeof(PerlXMMSClientCallbackParamType) * n_params);
		memcpy(cb->param_types, param_types, n_params * sizeof(PerlXMMSClientCallbackParamType));
	}

#ifdef PERL_IMPLICIT_CONTEXT
	cb->priv = aTHX;
#endif

	return cb;
}

void
perl_xmmsclient_callback_destroy(PerlXMMSClientCallback* cb) {
	if (cb) {
		if (cb->func) {
			SvREFCNT_dec(cb->func);
			cb->func = NULL;
		}

		if (cb->data) {
			SvREFCNT_dec(cb->data);
			cb->data = NULL;
		}

		if (cb->param_types) {
			free(cb->param_types);
			cb->n_params = 0;
			cb->param_types = NULL;
		}

		free(cb);
	}
}

void
perl_xmmsclient_callback_invoke(PerlXMMSClientCallback* cb, ...) {
	va_list va_args;
	dPERL_XMMS_CLIENT_CALLBACK_MARSHAL_SP;

	if (cb == NULL)
		croak("cb == NULL in perl_xmmsclient_callback_invoke");

	PERL_XMMS_CLIENT_MARSHAL_INIT(cb);

	ENTER;
	SAVETMPS;

	PUSHMARK(sp);

	va_start(va_args, cb);

	if (cb->n_params > 0) {
		int i;

		for (i = 0; i < cb->n_params; i++) {
			SV* sv;
			switch (cb->param_types[i]) {
				case PERL_XMMSCLIENT_CALLBACK_PARAM_TYPE_CONNECTION:
				case PERL_XMMSCLIENT_CALLBACK_PARAM_TYPE_RESULT:
					if (!cb->wrapper)
						croak("wrapper == NULL in perl_xmmsclient_callback_invoke");

					sv = cb->wrapper;
					break;
				case PERL_XMMSCLIENT_CALLBACK_PARAM_TYPE_FLAG:
					sv = newSViv(va_arg(va_args, int));
					break;
				default:
					PUTBACK;
					croak("Unknown PerlXMMSClientCallbackParamType in perl_xmmsclient_callback_invoke");
			}

			if (!sv) {
				PUTBACK;
				croak("failed to convert value to sv");
			}

			//dump_sv(sv);
			XPUSHs(sv);
		}
	}

	va_end(va_args);

	if (cb->data)
		XPUSHs(cb->data);

	PUTBACK;

	call_sv(cb->func, G_DISCARD);

	FREETMPS;
	LEAVE;
}

void
dump_sv(SV* sv) {
	int lim = 4;
	const STRLEN pv_lim = 0;
	const I32 save_dumpindent = PL_dumpindent;
	PL_dumpindent = 2;
	do_sv_dump(0, Perl_debug_log, sv, 0, lim, 0, pv_lim);
	PL_dumpindent = save_dumpindent;
}
