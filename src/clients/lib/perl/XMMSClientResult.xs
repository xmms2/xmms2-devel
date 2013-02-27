#include "perl_xmmsclient.h"

STATIC SV *value_to_sv (xmmsv_t *val);

STATIC int
notifyer_cb (xmmsv_t *val, void *user_data)
{
	int ret;
	PerlXMMSClientCallback *cb = (PerlXMMSClientCallback *)user_data;

	perl_xmmsclient_callback_invoke (cb, &ret, value_to_sv (val));

	return ret;
}

STATIC SV *
sv_from_value_int (xmmsv_t *val)
{
	int ret, num;

	ret = xmmsv_get_int (val, &num);

	if (ret == 0) {
		croak("could not fetch int value");
	}

	return newSViv (num);
}

STATIC SV *
sv_from_value_string (xmmsv_t *val)
{
	int ret;
	const char *str = NULL;

	ret = xmmsv_get_string (val, &str);

	if (ret == 0) {
		croak("could not fetch string value");
	}

	return newSVpv (str, 0);
}

STATIC SV *
sv_from_value_coll (xmmsv_t *val)
{
	return perl_xmmsclient_new_sv_from_ptr ((void *) xmmsv_ref (val), "Audio::XMMSClient::Collection");
}

STATIC SV *
sv_from_value_bin (xmmsv_t *val)
{
	int ret;
	const unsigned char *bin;
	unsigned int bin_len = 0;

	ret = xmmsv_get_bin (val, &bin, &bin_len);

	if (ret == 0) {
		croak("could not fetch bin value");
	}

	return newSVpv ((char *)bin, bin_len);
}

STATIC void
list_foreach_cb (xmmsv_t *value, void *user_data)
{
	AV *list = (AV *)user_data;
	av_push (list, value_to_sv (value));
}

STATIC SV *
sv_from_value_list (xmmsv_t *val)
{
	int ret;
	AV *list = newAV ();

	ret = xmmsv_list_foreach (val, list_foreach_cb, list);

	if (ret == 0) {
		croak ("could not fetch list value");
	}

	return newRV_inc ((SV *)list);
}

STATIC void
dict_foreach_cb (const char *key, xmmsv_t *value, void *user_data)
{
	HV *hash = (HV *)user_data;

	if (!hv_store (hash, key, strlen (key), value_to_sv (value), 0)) {
		croak ("failed to convert result to hash");
	}
}

STATIC SV *
sv_from_value_dict (xmmsv_t *val)
{
	int ret;
	HV *dict = newHV ();

	ret = xmmsv_dict_foreach (val, dict_foreach_cb, dict);

	if (ret == 0) {
		croak ("could not fetch dict value");
	}

	return newRV_inc ((SV *)dict);
}

STATIC void
croak_value_error (xmmsv_t *val) {
	int ret;
	const char *msg;

	ret = xmmsv_get_error (val, &msg);

	if (ret == 0) {
		croak ("could not fetch error message");
	}

	croak ("%s", msg);
}

STATIC SV *
value_to_sv (xmmsv_t *value) {
	xmmsv_type_t type;
	SV *ret;

	type = xmmsv_get_type (value);

	switch (type) {
		case XMMSV_TYPE_NONE:
			ret = &PL_sv_undef;
			break;
		case XMMSV_TYPE_ERROR:
			croak_value_error (value);
			break;
		case XMMSV_TYPE_INT32:
			ret = sv_from_value_int (value);
			break;
		case XMMSV_TYPE_STRING:
			ret = sv_from_value_string (value);
			break;
		case XMMSV_TYPE_COLL:
			ret = sv_from_value_coll (value);
			break;
		case XMMSV_TYPE_BIN:
			ret = sv_from_value_bin (value);
			break;
		case XMMSV_TYPE_LIST:
			ret = sv_from_value_list (value);
			break;
		case XMMSV_TYPE_DICT:
			ret = sv_from_value_dict (value);
			break;
		default:
			croak ("unhandled value type");
	}

	return ret;
}

MODULE = Audio::XMMSClient::Result	PACKAGE = Audio::XMMSClient::Result	PREFIX = xmmsc_result_

=head1 NAME

Audio::XMMSClient::Result - Results of Audio::XMMSClient operations

=head1 SYNOPSIS

  use Audio::XMMSClient;

  my $conn = Audio::XMMSClient->new($client_name);
  $conn->connect or die $c->get_last_error;

  my $result = $c->playback_status;
  $result->wait;
  print $result->value;

=head1 DESCRIPTION

This module provides an abstract, asyncronous interface to for retrieving results of C<Audio::XMMSClient> server operations.

=head1 METHODS

=head2 get_class

=over 4

=item Arguments: none

=item Return Value: $class

=back

  my $class = $result->get_class;

Get the class of the result. This may be one of "default", "signal" and
"broadcast".

=cut

xmmsc_result_type_t
xmmsc_result_get_class (res)
		xmmsc_result_t *res

=head2 notifier_set

=over 4

=item Arguments: \&func, $data?

=item Return Value: none

=back

  $result->notifier_set(sub { die 'got an answer!' });

Set up a callback for the result retrival. This callback will be called when
the answers arrives. It's arguments will be the result itself as well as an
optional userdata if C<$data> is passed.

=cut

void
xmmsc_result_notifier_set (res, func, data=NULL)
		xmmsc_result_t *res
		SV *func
		SV *data
	PREINIT:
		PerlXMMSClientCallback *cb = NULL;
		PerlXMMSClientCallbackParamType param_types[1];
	CODE:
		param_types[0] = PERL_XMMSCLIENT_CALLBACK_PARAM_TYPE_VALUE;

		cb = perl_xmmsclient_callback_new (func, data, ST(0), 1, param_types,
		                                   PERL_XMMSCLIENT_CALLBACK_RETURN_TYPE_INT);

		xmmsc_result_notifier_set_full (res, notifyer_cb, cb,
		                                (xmmsc_user_data_free_func_t)perl_xmmsclient_callback_destroy);

=head2 wait

=over 4

=item Arguments: none

=item Return Value: $result

=back

  my $value = $result->wait->value;

Block for the reply. In a synchronous application this can be used to wait for
the result. Will return the C<$result> this method was called when the server
replyed.

=cut

SV *
xmmsc_result_wait (res)
		SV *res
	PREINIT:
		xmmsc_result_t *c_res;
	CODE:
		c_res = (xmmsc_result_t *)perl_xmmsclient_get_ptr_from_sv (res, "Audio::XMMSClient::Result");

		xmmsc_result_wait (c_res);

		SvREFCNT_inc (res);
		RETVAL = res;
	OUTPUT:
		RETVAL

=head2 get_type

=over 4

=item Arguments: none

=item Return Value: $type

=back

  my $type = $result->get_type;

Get the type of the result. May be one of "none", "error", "uint", "int",
"string", "dict", "bin", "coll" or "list".

=cut

xmmsv_type_t
xmmsc_result_get_type (res)
		xmmsc_result_t *res
	CODE:
		RETVAL = xmmsv_get_type (xmmsc_result_get_value (res));
	OUTPUT:
		RETVAL

=head2 iserror

=over 4

=item Arguments: none

=item Return Value: 1 | 0

=back

  my $has_error = $result->iserror;

Check if the result represents an error. Returns a true value if the result is
an error, false otherwise.

=cut

int
xmmsc_result_iserror (res)
		xmmsc_result_t *res

=head2 get_error

=over 4

=item Arguments: none

=item Return Value: $message

=back

  my $message = $result->get_error;

Get an error C<$message> describing the error that occoured.

=cut

const char *
xmmsc_result_get_error (res)
		xmmsc_result_t *res
	PREINIT:
		xmmsv_t *val;
	CODE:
		RETVAL = NULL;
		val = xmmsc_result_get_value (res);
		if (val) {
			xmmsv_get_error (val, &RETVAL);
		}
	OUTPUT:
		RETVAL

=head2 value

=over 4

=item Arguments: none

=item Return Value: $scalar | \@arrayref | \%hashref | \%propdict

=back

  my $value = $result->value;

Gets the return value of the C<$result>. Depending of the results type
(L</get_type>) you might get other types of return values.

=cut

SV *
value (res)
		xmmsc_result_t *res
	CODE:
		RETVAL = value_to_sv (xmmsc_result_get_value (res));
	OUTPUT:
		RETVAL

=for later

=head2 decode_url

=over 4

=item Arguments: $string

=item Return Value: $decoded_url

=back

  my $decoded_url = $result->decode_url($url);
  my $decoded_url = Audio::XMMSClient::Result->decode_url($url);

Decode an URL-encoded C<$string>.

Some strings (currently only the url of media) has no known encoding, and must
be encoded in an UTF-8 clean way. This is done similar to the url encoding web
browsers do. This functions decodes a string encoded in that way. OBSERVE that
the decoded string HAS NO KNOWN ENCODING and you cannot display it on screen in
a 100% guaranteed correct way (a good heuristic is to try to validate the
decoded string as UTF-8, and if it validates assume that it is an UTF-8 encoded
string, and otherwise fall back to some other encoding).

Do not use this function if you don't understand the implications. The best
thing is not to try to display the url at all.

Note that the fact that the string has NO KNOWN ENCODING and CAN NOT BE
DISPLAYED does not stop you from open the file if it is a local file (if it
starts with "file://").

This method can either be called as a class or instance method.

=cut

=for later
const char *
xmmsc_result_decode_url (class, string)
		const char *string
	C_ARGS:
		NULL, string
	CLEANUP:
		free ((void *)RETVAL);
=cut

=head1 AUTHOR

Florian Ragwitz <rafl@debian.org>

=head1 SEE ALSO

L<Audio::XMMSClient>, L<Audio::XMMSClient::Result::PropDict>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2006-2008, Florian Ragwitz

This library is free software; you can redistribute it and/or modify it under
the same terms as Perl itself, either Perl version 5.8.8 or, at your option,
any later version of Perl 5 you may have available.

=cut

void
DESTROY (res)
		xmmsc_result_t *res
	CODE:
		xmmsc_result_unref (res);
