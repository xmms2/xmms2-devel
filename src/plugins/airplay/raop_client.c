#include <string.h>
#include <unistd.h>

#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/bio.h>
#include <openssl/engine.h>
#include <openssl/bn.h>
#include <openssl/aes.h>

#include "raop_client.h"
#include "net_utils.h"
#include "rtsp.h"

#define RAOP_RTSP_USER_AGENT "iTunes/4.6 (Macintosh; U; PPC Mac OS X 10.3)"
#define RAOP_DEFAULT_VOLUME -30

#define RAOP_IO_UNDEFINED    0x00
#define RAOP_IO_RTSP_READ    0x01
#define RAOP_IO_RTSP_WRITE   0x02
#define RAOP_IO_STREAM_READ  0x04
#define RAOP_IO_STREAM_WRITE 0x08

#define RAOP_RTSP_DISCONNECTED  0x00
#define RAOP_RTSP_CONNECTING    0x01
#define RAOP_RTSP_ANNOUNCE      0x02
#define RAOP_RTSP_SETUP         0x04
#define RAOP_RTSP_RECORD        0x08
#define RAOP_RTSP_SETPARAMS     0x10
#define RAOP_RTSP_FLUSH         0x20
#define RAOP_RTSP_CONNECTED     0x40
#define RAOP_RTSP_DONE          0x80

typedef enum audio_jack_status {
	AUDIO_JACK_CONNECTED,
	AUDIO_JACK_DISCONNECTED
} audio_jack_status_t;

typedef enum audio_jack_type {
	AUDIO_JACK_ANALOG,
	AUDIO_JACK_DIGITAL
} audio_jack_type_t;

struct raop_client_struct {
	/* endpoint addresses */
	gchar *apex_host;
	gushort rtsp_port;
	gushort stream_port;
	gchar *cli_host;

	/* rtsp handler */
	RTSPConnection *rtsp_conn;
	gchar *rtsp_url;
	guint32 rtsp_state;

	/* data stream */
	gint stream_fd;
	raop_client_stream_cb_t stream_cb;

	/* RTSP and stream channel state */
	gint io_state;

	/* RTSP session stuff */
	gchar session_id[10 + 1]; /* 4-byte base-10 */
	gchar client_id[16 + 1]; /* 8-byte base-16 */

	/* audio settings */
	audio_jack_status_t jack_status;
	audio_jack_type_t jack_type;
	gdouble volume;

	/* crypto */
	guint8 aes_iv[16];
	guint8 aes_key_str[16];
	guint8 challenge[16];
	AES_KEY *aes_key;

	/* audio sample */
	guint8 sbuf[RAOP_ALAC_FRAME_SIZE * 2  * 2 + 19]; /* 16-bit stereo */
	guint32 sbuf_size;
	guint32 sbuf_offset;
};

static gint raop_client_stream_sample (raop_client_t *rc, guint16 *buf, guint32 len);

/* Helper Functions */

/* write upto 8 bits at-a-time into a buffer */
static void
write_bits (guint8 *buf, guint8 val, gint nbits, guint32 *offset)
{
	int bit_offset = *offset % 8;
	int byte_offset = *offset / 8;
	int left = 8 - bit_offset;

	*offset += nbits;
	if (nbits >= left) {
		buf[byte_offset] |= (val >> (nbits - left));
		val = (val << left) >> left;
		nbits -= left;
		left = 8;
		byte_offset++;
	}
	if (nbits && nbits < left) {
		buf[byte_offset] |= (val << (left - nbits));
	}
}

static gint
raop_rsa_encrypt (guchar *text, gint len, guchar *res)
{
	RSA *rsa;
	size_t size;
	static const guchar mod[] = {
		0xe7,0xd7,0x44,0xf2,0xa2,0xe2,0x78,0x8b,0x6c,0x1f,0x55,0xa0,
		0x8e,0xb7,0x5,0x44,0xa8,0xfa,0x79,0x45,0xaa,0x8b,0xe6,0xc6,
		0x2c,0xe5,0xf5,0x1c,0xbd,0xd4,0xdc,0x68,0x42,0xfe,0x3d,0x10,
		0x83,0xdd,0x2e,0xde,0xc1,0xbf,0xd4,0x25,0x2d,0xc0,0x2e,0x6f,
		0x39,0x8b,0xdf,0xe,0x61,0x48,0xea,0x84,0x85,0x5e,0x2e,0x44,
		0x2d,0xa6,0xd6,0x26,0x64,0xf6,0x74,0xa1,0xf3,0x4,0x92,0x9a,
		0xde,0x4f,0x68,0x93,0xef,0x2d,0xf6,0xe7,0x11,0xa8,0xc7,0x7a,
		0xd,0x91,0xc9,0xd9,0x80,0x82,0x2e,0x50,0xd1,0x29,0x22,0xaf,
		0xea,0x40,0xea,0x9f,0xe,0x14,0xc0,0xf7,0x69,0x38,0xc5,0xf3,
		0x88,0x2f,0xc0,0x32,0x3d,0xd9,0xfe,0x55,0x15,0x5f,0x51,0xbb,
		0x59,0x21,0xc2,0x1,0x62,0x9f,0xd7,0x33,0x52,0xd5,0xe2,0xef,
		0xaa,0xbf,0x9b,0xa0,0x48,0xd7,0xb8,0x13,0xa2,0xb6,0x76,0x7f,
		0x6c,0x3c,0xcf,0x1e,0xb4,0xce,0x67,0x3d,0x3,0x7b,0xd,0x2e,
		0xa3,0xc,0x5f,0xff,0xeb,0x6,0xf8,0xd0,0x8a,0xdd,0xe4,0x9,
		0x57,0x1a,0x9c,0x68,0x9f,0xef,0x10,0x72,0x88,0x55,0xdd,0x8c,
		0xfb,0x9a,0x8b,0xef,0x5c,0x89,0x43,0xef,0x3b,0x5f,0xaa,0x15,
		0xdd,0xe6,0x98,0xbe,0xdd,0xf3,0x59,0x96,0x3,0xeb,0x3e,0x6f,
		0x61,0x37,0x2b,0xb6,0x28,0xf6,0x55,0x9f,0x59,0x9a,0x78,0xbf,
		0x50,0x6,0x87,0xaa,0x7f,0x49,0x76,0xc0,0x56,0x2d,0x41,0x29,
		0x56,0xf8,0x98,0x9e,0x18,0xa6,0x35,0x5b,0xd8,0x15,0x97,0x82,
		0x5e,0xf,0xc8,0x75,0x34,0x3e,0xc7,0x82,0x11,0x76,0x25,0xcd
		,0xbf,0x98,0x44,0x7b};
	static const guchar exp[] = {0x01, 0x00, 0x01};

	rsa = RSA_new ();
	rsa->n = BN_bin2bn (mod, 256, NULL);
	rsa->e = BN_bin2bn (exp, 3, NULL);

	size = RSA_public_encrypt (len, text, res, rsa, RSA_PKCS1_OAEP_PADDING);

	RSA_free (rsa);
	return size;
}

static void
raop_send_sample (raop_client_t *rc)
{
	gint nwritten;
	gint nleft = rc->sbuf_size - rc->sbuf_offset;

	if (nleft == 0) {
		guchar buf[RAOP_ALAC_FRAME_SIZE * 2 * 2];
		gint ret;

		ret = rc->stream_cb.func (rc->stream_cb.data, buf, sizeof (buf));
		if (ret > 0)
			raop_client_stream_sample (rc, (guint16 *)buf, ret);

		nleft = rc->sbuf_size - rc->sbuf_offset;
	}
	/* XXX: check for error */
	nwritten = tcp_write (rc->stream_fd, (char *) rc->sbuf + rc->sbuf_offset,
	                      nleft);
	rc->sbuf_offset += nwritten;
}

/* RTSP glue */

static gint
raop_rtsp_get_reply (raop_client_t *rc)
{
	RTSPMessage response = { 0 };
	RTSPResult res;
	gchar *ajstatus;
	gchar **params;

	res = rtsp_connection_receive (rc->rtsp_conn, &response);
	if (res != RTSP_OK)
		return RAOP_EFAIL;

	res = rtsp_message_get_header (&response, RTSP_HDR_AUDIO_JACK_STATUS,
	                               &ajstatus);
	if (res == RTSP_OK) {
		params = g_strsplit (ajstatus, "; ", -1);
		if (!g_ascii_strncasecmp (params[0], "connected", strlen ("connected"))) {
			rc->jack_status = AUDIO_JACK_CONNECTED;
		} else {
			rc->jack_status = AUDIO_JACK_DISCONNECTED;
		}
		if (!g_ascii_strncasecmp (params[1], "type=analog", strlen ("type=analog"))) {
			rc->jack_type = AUDIO_JACK_ANALOG;
		} else {
			rc->jack_type = AUDIO_JACK_DIGITAL;
		}
		g_strfreev (params);
	}

	if (rc->rtsp_state == RAOP_RTSP_SETUP) {
		gchar *transport;
		gchar *port_str;
		res = rtsp_message_get_header (&response, RTSP_HDR_TRANSPORT,
		                               &transport);
		if (res != RTSP_OK)
			return RAOP_EFAIL;

		port_str = g_strrstr (transport, "server_port=");
		rc->stream_port = strtol (port_str + strlen ("server_port="), NULL, 10);
	}

	return RAOP_EOK;
}

static gint
b64_encode_alloc (const guchar *data, int size, char **out)
{
	BIO *mb,*b64b,*bio;
	char *p;

	mb = BIO_new (BIO_s_mem ());
	b64b = BIO_new (BIO_f_base64 ());
	BIO_set_flags (b64b, BIO_FLAGS_BASE64_NO_NL);
	bio = BIO_push (b64b, mb);

	BIO_write (bio,data,size);

	(void) BIO_flush (bio);
	size = BIO_ctrl (mb, BIO_CTRL_INFO, 0, (char *)&p);
	*out = g_malloc (size+1);
	memcpy (*out, p, size);
	(*out)[size] = 0;
	BIO_free_all (bio);
	return size;
}

static gint
raop_rtsp_announce (raop_client_t *rc)
{
	RTSPMessage request = { 0 };
	RTSPResult res;
	guchar enc_aes_key[512];
	gchar *key;
	gchar *iv;
	gint size;
	gchar *sdp_buf;
	gchar *ac;
	gint ret = RAOP_EOK;

	size = raop_rsa_encrypt (rc->aes_key_str, 16, enc_aes_key);

	size = b64_encode_alloc (enc_aes_key, size, &key);
	g_strdelimit (key, "=", '\0');
	size = b64_encode_alloc (rc->aes_iv, 16, &iv);
	g_strdelimit (iv, "=", '\0');
	size = b64_encode_alloc (rc->challenge, 16, &ac);
	g_strdelimit (ac, "=", '\0');

	res = rtsp_message_init_request (RTSP_ANNOUNCE, rc->rtsp_url, &request);
	rtsp_message_add_header (&request, RTSP_HDR_USER_AGENT, RAOP_RTSP_USER_AGENT);
	rtsp_message_add_header (&request, RTSP_HDR_CLIENT_INSTANCE, rc->client_id);
	rtsp_message_add_header (&request, RTSP_HDR_APPLE_CHALLENGE, ac);
	rtsp_message_add_header (&request, RTSP_HDR_CONTENT_TYPE, "application/sdp");

	sdp_buf = g_strdup_printf ("v=0\r\n"
	                           "o=iTunes %s 0 IN IP4 %s\r\n"
	                           "s=iTunes\r\n"
	                           "c=IN IP4 %s\r\n"
	                           "t=0 0\r\n"
	                           "m=audio 0 RTP/AVP 96\r\n"
	                           "a=rtpmap:96 AppleLossless\r\n"
	                           "a=fmtp:96 4096 0 16 40 10 14 2 255 0 0 44100\r\n"
	                           "a=rsaaeskey:%s\r\n"
	                           "a=aesiv:%s\r\n",
	                           rc->session_id, rc->cli_host,
	                           rc->apex_host, key, iv);
	rtsp_message_set_body (&request, (guint8 *) sdp_buf, strlen (sdp_buf));
	res = rtsp_connection_send (rc->rtsp_conn, &request);
	if (res != RTSP_OK)
		ret = RAOP_EFAIL;

	g_free (key);
	g_free (iv);
	g_free (ac);
	g_free (sdp_buf);
	return ret;
}

static gint
raop_rtsp_setup (raop_client_t *rc)
{
	RTSPMessage request = { 0 };
	RTSPResult res;

	res = rtsp_message_init_request (RTSP_SETUP, rc->rtsp_url, &request);
	rtsp_message_add_header (&request, RTSP_HDR_CLIENT_INSTANCE, rc->client_id);
	rtsp_message_add_header (&request, RTSP_HDR_USER_AGENT, RAOP_RTSP_USER_AGENT);
	rtsp_message_add_header (&request, RTSP_HDR_TRANSPORT, "RTP/AVP/TCP;unicast;interleaved=0-1;mode=record");/* ;control_port=0;timing_port=0"; */

	res = rtsp_connection_send (rc->rtsp_conn, &request);
	return (res == RTSP_OK) ? RAOP_EOK : RAOP_EFAIL;
}

static gint
raop_rtsp_record (raop_client_t *rc)
{
	RTSPMessage request = { 0 };
	RTSPResult res;

	res = rtsp_message_init_request (RTSP_RECORD, rc->rtsp_url, &request);
	rtsp_message_add_header (&request, RTSP_HDR_CLIENT_INSTANCE, rc->client_id);
	rtsp_message_add_header (&request, RTSP_HDR_USER_AGENT, RAOP_RTSP_USER_AGENT);
	rtsp_message_add_header (&request, RTSP_HDR_RANGE, "npt=0-");
	rtsp_message_add_header (&request, RTSP_HDR_RTP_INFO, "seq=0;rtptime=0");
	res = rtsp_connection_send (rc->rtsp_conn, &request);
	return (res == RTSP_OK) ? RAOP_EOK : RAOP_EFAIL;
}

/*
 * XXX: take seq and rtptime as args
 */
static gint
raop_rtsp_flush (raop_client_t *rc)
{
	RTSPMessage request = { 0 };
	RTSPResult res;

	res = rtsp_message_init_request (RTSP_FLUSH, rc->rtsp_url, &request);
	rtsp_message_add_header (&request, RTSP_HDR_CLIENT_INSTANCE, rc->client_id);
	rtsp_message_add_header (&request, RTSP_HDR_USER_AGENT, RAOP_RTSP_USER_AGENT);
	rtsp_message_add_header (&request, RTSP_HDR_RANGE, "npt=0-");
	rtsp_message_add_header (&request, RTSP_HDR_RTP_INFO, "seq=0;rtptime=0");
	res = rtsp_connection_send (rc->rtsp_conn, &request);
	return (res == RTSP_OK) ? RAOP_EOK : RAOP_EFAIL;
}

static gint
raop_rtsp_set_params (raop_client_t *rc)
{
	RTSPMessage request = { 0 };
	RTSPResult res;
	gchar *volume;

	res = rtsp_message_init_request (RTSP_SET_PARAMETER, rc->rtsp_url, &request);
	rtsp_message_add_header (&request, RTSP_HDR_CLIENT_INSTANCE, rc->client_id);
	rtsp_message_add_header (&request, RTSP_HDR_USER_AGENT, RAOP_RTSP_USER_AGENT);

	rtsp_message_add_header (&request, RTSP_HDR_CONTENT_TYPE, "text/parameters");
	volume = g_strdup_printf ("volume: %f\r\n", rc->volume);
	rtsp_message_set_body (&request, (guint8 *) volume, strlen (volume));
	res = rtsp_connection_send (rc->rtsp_conn, &request);

	g_free (volume);

	return (res == RTSP_OK) ? RAOP_EOK : RAOP_EFAIL;
}

static gint
raop_rtsp_teardown (raop_client_t *rc)
{
	RTSPMessage request = { 0 };
	RTSPResult res;

	res = rtsp_message_init_request (RTSP_TEARDOWN, rc->rtsp_url, &request);
	rtsp_message_add_header (&request, RTSP_HDR_CLIENT_INSTANCE, rc->client_id);
	rtsp_message_add_header (&request, RTSP_HDR_USER_AGENT, RAOP_RTSP_USER_AGENT);
	res = rtsp_connection_send (rc->rtsp_conn, &request);
	return (res == RTSP_OK) ? RAOP_EOK : RAOP_EFAIL;
}


/* Public API */

gint
raop_client_init (raop_client_t **client)
{
	raop_client_t *rc;
	guchar rand_buf[8 + 16];
	int ret;

	*client = (raop_client_t *) g_malloc (sizeof (raop_client_t));
	if (!*client)
		return RAOP_ENOMEM;
	rc = *client;

	/* XXX: use a better seed */
	RAND_seed (rc, sizeof (raop_client_t));
	memset ((void *) rc, 0, sizeof (raop_client_t));

	rc->stream_fd = -1;
	rc->io_state = RAOP_IO_UNDEFINED;
	rc->jack_status = AUDIO_JACK_DISCONNECTED;
	rc->jack_type = AUDIO_JACK_ANALOG;
	rc->volume = RAOP_DEFAULT_VOLUME;

	/* setup client_id, aes_key */
	ret = RAND_bytes (rand_buf, sizeof (rand_buf));
	g_snprintf (rc->client_id, 17, "%08X%08X", *((guint *) rand_buf),
	            *((guint *) (rand_buf + 4)));

	ret = RAND_bytes (rc->aes_key_str, sizeof (rc->aes_key_str));
	rc->aes_key = (AES_KEY *) g_malloc (sizeof (AES_KEY));
	AES_set_encrypt_key (rc->aes_key_str, 128, rc->aes_key);

	return RAOP_EOK;
}

gint
raop_client_connect (raop_client_t *rc, const gchar *host, gushort port)
{
	guchar rand_buf[4];
	int ret;
	int rtsp_fd;

	rc->apex_host = g_strdup (host);
	rc->rtsp_port = port;
	rc->sbuf_size = 0;
	rc->sbuf_offset = 0;

	RAND_bytes (rand_buf, sizeof (rand_buf));
	g_snprintf (rc->session_id, 11, "%u", *((guint *) rand_buf));
	RAND_bytes (rc->aes_iv, sizeof (rc->aes_iv));
	RAND_bytes (rc->challenge, sizeof (rc->challenge));

	rtsp_fd = tcp_open ();
	if (rtsp_fd == -1)
		return RAOP_ESYS;
	ret = set_sock_nonblock (rtsp_fd);
	if (ret == -1)
		return RAOP_ESYS;
	ret = tcp_connect (rtsp_fd, rc->apex_host, rc->rtsp_port);
	if (ret == -1 && errno != EINPROGRESS)
		return RAOP_ESYS;

	/* XXX: do this after successful connect? */
	rc->cli_host = g_strdup (get_local_addr (rtsp_fd));
	rc->rtsp_url = g_strdup_printf ("rtsp://%s/%s", rc->cli_host,
	                                rc->session_id);
	rtsp_connection_create (rtsp_fd, &rc->rtsp_conn);

	rc->rtsp_state = RAOP_RTSP_CONNECTING;
	rc->io_state |= RAOP_IO_RTSP_WRITE;
	return RAOP_EOK;
}


gint
raop_client_handle_io (raop_client_t *rc, int fd, GIOCondition cond)
{
	gint rtsp_fd = rc->rtsp_conn->fd;
	gint ret = RAOP_EOK;

	if (fd < 0)
		return RAOP_EINVAL;

	if (cond == G_IO_OUT ) {
		/* send RTSP requests */
		if (fd == rtsp_fd) {
			/* if pending answers return */
			if (rc->io_state & RAOP_IO_RTSP_READ)
				return RAOP_EPROTO;
			/* XXX: options, challenge/response */
			if (rc->rtsp_state & RAOP_RTSP_CONNECTING) {
				ret = raop_rtsp_announce (rc);
				if (ret != RAOP_EOK)
					return ret;
				rc->rtsp_state = RAOP_RTSP_ANNOUNCE;
			} else if (rc->rtsp_state & RAOP_RTSP_ANNOUNCE) {
				ret = raop_rtsp_setup (rc);
				if (ret != RAOP_EOK)
					return ret;
				rc->rtsp_state = RAOP_RTSP_SETUP;
			} else if (rc->rtsp_state & RAOP_RTSP_SETUP) {
				ret = raop_rtsp_record (rc);
				if (ret != RAOP_EOK)
					return ret;
				rc->rtsp_state = RAOP_RTSP_RECORD;
			} else if (rc->rtsp_state & RAOP_RTSP_RECORD) {
				ret = raop_rtsp_set_params (rc);
				if (ret != RAOP_EOK)
					return ret;
				rc->rtsp_state = RAOP_RTSP_DONE;
			} else if (rc->rtsp_state & RAOP_RTSP_SETPARAMS) {
				ret = raop_rtsp_set_params (rc);
				if (ret != RAOP_EOK)
					return ret;
				rc->rtsp_state ^= RAOP_RTSP_SETPARAMS;
			} else if (rc->rtsp_state & RAOP_RTSP_FLUSH) {
				ret = raop_rtsp_flush (rc);
				if (ret != RAOP_EOK)
					return ret;
				rc->rtsp_state ^= RAOP_RTSP_FLUSH;
			}
			rc->io_state ^= RAOP_IO_RTSP_WRITE;
			rc->io_state |= RAOP_IO_RTSP_READ;
		} else if (fd == rc->stream_fd) { /* stream data */
			raop_send_sample (rc);
		}
	} else if (cond == G_IO_IN) {
		/* get RTSP replies */
		if (fd == rtsp_fd) {
			/* shouldn't happen */
			if (rc->io_state & RAOP_IO_RTSP_WRITE)
				return RAOP_EPROTO;
			ret = raop_rtsp_get_reply (rc);
			if (ret != RAOP_EOK)
				return ret;
			rc->io_state ^= RAOP_IO_RTSP_READ;
			if (rc->rtsp_state == RAOP_RTSP_DONE) {
				rc->stream_fd = tcp_open ();
				if (rc->stream_fd == -1)
					return RAOP_ESYS;
				ret = set_sock_nonblock (rc->stream_fd);
				if (ret == -1)
					return RAOP_ESYS;
				ret = tcp_connect (rc->stream_fd, rc->apex_host,
				                   rc->stream_port);
				if (ret == -1 && errno != EINPROGRESS)
					return RAOP_ESYS;
				rc->io_state |= RAOP_IO_STREAM_WRITE;
				rc->io_state |= RAOP_IO_STREAM_READ;
				rc->rtsp_state = RAOP_RTSP_CONNECTED;
			} else if (rc->rtsp_state != RAOP_RTSP_CONNECTED) {
				rc->io_state |= RAOP_IO_RTSP_WRITE;
			}
		} else if (fd == rc->stream_fd) {
			char buf[56];
			/* read data sent by ApEx we don't know what
			 * it is, just read it for now, and doesn't
			 * even care about returnval */
			read (rc->stream_fd, buf, 56);
		}
	} else if (cond == G_IO_ERR) {
		/* XXX */
	}

	return RAOP_EOK;
}

static gint
raop_client_stream_sample (raop_client_t *rc, guint16 *buf, guint32 len)
{
	guint8 hdr[] = {0x24, 0x00, 0x00, 0x00,
	                0xF0, 0xFF, 0x00, 0x00,
	                0x00, 0x00, 0x00, 0x00,
	                0x00, 0x00, 0x00, 0x00};
	int i;
	short cnt;
	guint8 *pbuf;
	guint8 iv[16];
	guint32 offset = 0;

	cnt = len + 3 + 12;
	cnt = GUINT16_TO_BE (cnt);
	memcpy (hdr + 2, (void *) &cnt, sizeof (cnt));

	memset (rc->sbuf, 0, sizeof (rc->sbuf));
	memcpy (rc->sbuf, hdr, sizeof (hdr));
	pbuf = rc->sbuf + sizeof (hdr);

	/* ALAC frame header */
	write_bits (pbuf, 1, 3, &offset); /* # of channels */
	write_bits (pbuf, 0, 4, &offset); /* output waiting? */
	write_bits (pbuf, 0, 4, &offset); /* unknown */
	write_bits (pbuf, 0, 8, &offset); /* unknown */
	write_bits (pbuf, 0, 1, &offset); /* sample size follows the header */
	write_bits (pbuf, 0, 2, &offset); /* unknown */
	write_bits (pbuf, 1, 1, &offset); /* uncompressed */

	for (i = 0; i < len/2; i++) {
		write_bits (pbuf, (buf[i]) >> 8, 8, &offset);
		write_bits (pbuf, (buf[i]) & 0xff, 8, &offset);
	}
	memcpy (iv, rc->aes_iv, sizeof (iv));
	AES_cbc_encrypt (pbuf, pbuf,
	                 (len + 3) / 16 * 16,
	                 rc->aes_key, iv, TRUE);

	rc->sbuf_size = len + 3 + sizeof (hdr);
	rc->sbuf_offset = 0;

	return RAOP_EOK;
}

gint
raop_client_flush (raop_client_t *rc)
{
	if (rc->rtsp_state & RAOP_RTSP_CONNECTED) {
		memset (rc->sbuf, 0, sizeof (rc->sbuf));
		rc->rtsp_state |= RAOP_RTSP_FLUSH;
		rc->io_state |= RAOP_IO_RTSP_WRITE;
	}

	return RAOP_EOK;
}

gint
raop_client_set_stream_cb (raop_client_t *rc, raop_client_stream_cb_func_t func, void *data)
{
	rc->stream_cb.func = func;
	rc->stream_cb.data = data;
	return RAOP_EOK;
}

gint
raop_client_set_volume (raop_client_t *rc, gdouble volume)
{
	rc->volume = volume;
	if (rc->rtsp_state & RAOP_RTSP_CONNECTED) {
		rc->rtsp_state |= RAOP_RTSP_SETPARAMS;
		rc->io_state |= RAOP_IO_RTSP_WRITE;
	}
	return RAOP_EOK;
}

gint
raop_client_get_volume (raop_client_t *rc, gdouble *volume)
{
	*volume = rc->volume;
	return RAOP_EOK;
}

gboolean
raop_client_can_read (raop_client_t *rc, gint fd)
{
	gint rtsp_fd = rc->rtsp_conn->fd;
	if (fd == rtsp_fd) {
		return rc->io_state & RAOP_IO_RTSP_READ;
	} else if (fd == rc->stream_fd) {
		return rc->io_state & RAOP_IO_STREAM_READ;
	} else {
		return FALSE;
	}
}

gboolean
raop_client_can_write (raop_client_t *rc, gint fd)
{
	gint rtsp_fd = rc->rtsp_conn->fd;
	if (fd == rtsp_fd) {
		return rc->io_state & RAOP_IO_RTSP_WRITE;
	} else if (fd == rc->stream_fd) {
		return rc->io_state & RAOP_IO_STREAM_WRITE;
	} else {
		return FALSE;
	}
}

gint
raop_client_rtsp_sock (raop_client_t *rc)
{
	return rc->rtsp_conn->fd;
}

gint
raop_client_stream_sock (raop_client_t *rc)
{
	return rc->stream_fd;
}

gint
raop_client_disconnect (raop_client_t *rc)
{
	if (!rc)
		return RAOP_EINVAL;

	raop_rtsp_teardown (rc);
	close (rc->rtsp_conn->fd);
	close (rc->stream_fd);
	rc->rtsp_conn->fd = rc->stream_fd = -1;
	rtsp_connection_free (rc->rtsp_conn);
	rc->io_state = RAOP_IO_UNDEFINED;
	rc->rtsp_state = RAOP_RTSP_DISCONNECTED;
	g_free (rc->rtsp_url);
	return RAOP_EOK;
}

gint
raop_client_destroy (raop_client_t *rc)
{
	if (!rc)
		return RAOP_EINVAL;

	g_free (rc->aes_key);
	g_free (rc->apex_host);
	g_free (rc->cli_host);
	g_free (rc);
	return RAOP_EOK;
}
