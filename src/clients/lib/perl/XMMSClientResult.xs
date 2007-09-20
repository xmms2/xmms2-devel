#include "perl_xmmsclient.h"

void
perl_xmmsclient_xmmsc_result_notifyer_cb (xmmsc_result_t *res, void *user_data)
{
	PerlXMMSClientCallback *cb = (PerlXMMSClientCallback *)user_data;

	perl_xmmsclient_callback_invoke (cb, res);
}

SV *
perl_xmmsclient_xmmsc_result_get_uint (xmmsc_result_t *res)
{
	int ret;
	unsigned int val;

	ret = xmmsc_result_get_uint (res, &val);

	if (ret == 0) {
		croak("Could not fetch uint value");
	}

	return newSVuv (val);
}

SV *
perl_xmmsclient_xmmsc_result_get_int (xmmsc_result_t *res)
{
	int ret, val;

	ret = xmmsc_result_get_int (res, &val);

	if (ret == 0) {
		croak("Could not fetch int value");
	}

	return newSViv (val);
}

SV *
perl_xmmsclient_xmmsc_result_get_string (xmmsc_result_t *res)
{
	int ret;
	char *val = NULL;

	ret = xmmsc_result_get_string (res, &val);

	if (ret == 0) {
		croak("Could not fetch string value");
	}

	return newSVpv (val, 0);
}

SV *
perl_xmmsclient_xmmsc_result_get_coll (xmmsc_result_t *res)
{
	int ret;
	xmmsc_coll_t *coll = NULL;

	ret = xmmsc_result_get_collection (res, &coll);

	if (ret == 0) {
		croak("Could not fetch collection value");
	}

	return perl_xmmsclient_new_sv_from_ptr ((void *)coll, "Audio::XMMSClient::Collection");
}

SV *
perl_xmmsclient_xmmsc_result_get_bin (xmmsc_result_t *res)
{
	int ret;
	unsigned char *bin;
	unsigned int bin_len = 0;

	ret = xmmsc_result_get_bin (res, &bin, &bin_len);

	if (ret == 0) {
		croak("Could not fetch bin value");
	}

	return newSVpv ((char *)bin, bin_len);
}

SV *
perl_xmmsclient_xmms_result_cast_value (xmmsc_result_value_type_t type, const void *value)
{
	SV *perl_value;

	switch (type) {
		case XMMSC_RESULT_VALUE_TYPE_STRING:
			perl_value = newSVpv ((const char *)value, 0);
			break;
		case XMMSC_RESULT_VALUE_TYPE_INT32:
			perl_value = newSViv ((IV)value);
			break;
		case XMMSC_RESULT_VALUE_TYPE_UINT32:
			perl_value = newSVuv ((UV)value);
			break;
		default:
			perl_value = &PL_sv_undef;
	}

	return perl_value;
}

void
perl_xmmsclient_xmmsc_result_dict_foreach_cb (const void *key, xmmsc_result_value_type_t type, const void *value, void *user_data)
{
	HV *hash = (HV *)user_data;

	hv_store(hash, (const char *)key, strlen((const char *)key), perl_xmmsclient_xmms_result_cast_value (type, value), 0);
}

SV *
perl_xmmsclient_xmmsc_result_get_dict(xmmsc_result_t *res)
{
	int ret;
	HV *val = newHV ();

	ret = xmmsc_result_dict_foreach (res, perl_xmmsclient_xmmsc_result_dict_foreach_cb, val);

	if (ret == 0) {
		croak("Could not fetch dict value");
	}

	return newRV_inc ((SV *)val);
}

SV *
perl_xmmsclient_xmmsc_result_get_propdict (xmmsc_result_t *res)
{
	SV *hash;
	SV *tie;

	xmmsc_result_ref (res);

	tie = perl_xmmsclient_new_sv_from_ptr (res, "Audio::XMMSClient::Result::PropDict::Tie");
	hash = perl_xmmsclient_new_sv_from_ptr (res, "Audio::XMMSClient::Result::PropDict");
	hv_magic ((HV *)SvRV (hash), (GV *)tie, PERL_MAGIC_tied);

	return hash;
}

SV *
perl_xmmsclient_result_get_value (xmmsc_result_t *res)
{
	SV *ret;

	switch (xmmsc_result_get_type (res)) {
		case XMMS_OBJECT_CMD_ARG_UINT32:
			ret = perl_xmmsclient_xmmsc_result_get_uint (res);
			break;
		case XMMS_OBJECT_CMD_ARG_INT32:
			ret = perl_xmmsclient_xmmsc_result_get_int (res);
			break;
		case XMMS_OBJECT_CMD_ARG_STRING:
			ret = perl_xmmsclient_xmmsc_result_get_string (res);
			break;
		case XMMS_OBJECT_CMD_ARG_COLL:
			ret = perl_xmmsclient_xmmsc_result_get_coll (res);
			break;
		case XMMS_OBJECT_CMD_ARG_BIN:
			ret = perl_xmmsclient_xmmsc_result_get_bin (res);
			break;
		case XMMS_OBJECT_CMD_ARG_DICT:
			ret = perl_xmmsclient_xmmsc_result_get_dict (res);
			break;
		case XMMS_OBJECT_CMD_ARG_PROPDICT:
			ret = perl_xmmsclient_xmmsc_result_get_propdict (res);
			break;
		default:
			ret = &PL_sv_undef;
	}

	return ret;
}

SV *
perl_xmmsclient_result_get_list (xmmsc_result_t *res)
{
	AV *list = newAV ();

	while (xmmsc_result_list_valid (res)) {
		av_push (list, perl_xmmsclient_result_get_value (res));
		xmmsc_result_list_next (res);
	}

	return newRV_inc ((SV *)list);
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

=head2 disconnect

=over 4

=item Arguments: none

=item Return Value: none

=back

  $result->disconnect;

Disconnect a signal or a broadcast.

=cut

void
xmmsc_result_disconnect (res)
		xmmsc_result_t *res
	PREINIT:
		xmmsc_result_type_t type;
	INIT:
		type = xmmsc_result_get_class (res);
		if (type != XMMSC_RESULT_CLASS_SIGNAL && type != XMMSC_RESULT_CLASS_BROADCAST) {
			croak ("calling disconnect on a result that's neither a signal nor a broadcast");
		}

=head2 restart

=over 4

=item Arguments: none

=item Return Value: none

=back

  $result->restart;

Restart a signal.

A lot of signals you would like to get notified about when they change, instead
of polling the server all the time. These results are "restartable".

  sub handler {
	  my ($result) = @_;

	  if ($result->iserror) {
		  warn "error: ", $result->get_error;
		  return;
	  }

	  my $id = $result->value;
	  $result->restart;

	  print "current id is: ${id}\n";
  }

  # connect, blah, blah, ...
  my $res = $conn->signal_playback_playtime;
  $res->notifier_set(\&handler);

In the above example the handler would be called when the playtime is updated.
Only signals are restatable. Broadcasts will automaticly restart.

=cut

void
xmmsc_result_restart (res)
		xmmsc_result_t *res
	PREINIT:
		MAGIC *mg;
		xmmsc_result_t *res2;
	INIT:
		if (xmmsc_result_get_class (res) != XMMSC_RESULT_CLASS_SIGNAL) {
			croak ("trying to restart a result that's not a signal");
		}
	CODE:
		res2 = xmmsc_result_restart (res);
		xmmsc_result_unref (res);

		mg = perl_xmmsclient_get_magic_from_sv (ST(0), "Audio::XMMSClient::Result");
		mg->mg_ptr = (char *)res2;

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
		SV *res
		SV *func
		SV *data
	PREINIT:
		PerlXMMSClientCallback *cb = NULL;
		PerlXMMSClientCallbackParamType param_types[1];
		xmmsc_result_t *c_res;
	CODE:
		c_res = (xmmsc_result_t *)perl_xmmsclient_get_ptr_from_sv (res, "Audio::XMMSClient::Result");
		param_types[0] = PERL_XMMSCLIENT_CALLBACK_PARAM_TYPE_RESULT;

		cb = perl_xmmsclient_callback_new (func, data, res, 1, param_types);

		xmmsc_result_notifier_set_full (c_res, perl_xmmsclient_xmmsc_result_notifyer_cb,
		                                cb, (xmmsc_user_data_free_func_t)perl_xmmsclient_callback_destroy);

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

		SvREFCNT_inc (res); /* TODO: Only do so in non-void context? */
		RETVAL = res;
	OUTPUT:
		RETVAL

=head2 source_preference_set

=over 4

=item Arguments: @preferences

=item Return Value: none

=back

  $result->source_preference_set(qw{plugin/* plugin/id3v2 client/* server});

Set source C<@preferences> to be used when fetching stuff from a propdict.

=cut

void
xmmsc_result_source_preference_set (res, ...)
		xmmsc_result_t *res
	PREINIT:
		const char **preference = NULL;
		int i;
	INIT:
		preference = (const char **)malloc (sizeof (char *) * items);

		for (i = 1; i < items; i++) {
			preference[i] = SvPV_nolen (ST (i));
		}

		preference[items - 1] = NULL;
	C_ARGS:
		res, preference
	CLEANUP:
		free (preference);

=head2 get_type

=over 4

=item Arguments: none

=item Return Value: $type

=back

  my $type = $result->get_type;

Get the type of the result. May be one of "none", "uint", "int", "dict", "bin",
"coll", "list" or "propdict".

=cut

xmms_object_cmd_arg_type_t
xmmsc_result_get_type (res)
		xmmsc_result_t *res

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
		if (xmmsc_result_iserror (res)) {
			RETVAL = &PL_sv_undef;
		}

		if (xmmsc_result_is_list (res)) {
			RETVAL = perl_xmmsclient_result_get_list (res);
		} else {
			RETVAL = perl_xmmsclient_result_get_value (res);
		}
	OUTPUT:
		RETVAL

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

const char *
xmmsc_result_decode_url (class, string)
		const char *string
	C_ARGS:
		NULL, string
	CLEANUP:
		free ((void *)RETVAL);

=head1 AUTHOR

Florian Ragwitz <rafl@debian.org>

=head1 SEE ALSO

L<Audio::XMMSClient>, L<Audio::XMMSClient::Result::PropDict>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2006-2007, Florian Ragwitz

This library is free software; you can redistribute it and/or modify it under
the same terms as Perl itself, either Perl version 5.8.8 or, at your option,
any later version of Perl 5 you may have available.

=cut

void
DESTROY (res)
		xmmsc_result_t *res
	CODE:
		xmmsc_result_unref (res);

BOOT:
	PERL_UNUSED_VAR (items);
