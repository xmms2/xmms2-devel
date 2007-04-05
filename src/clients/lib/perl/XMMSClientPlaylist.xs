#include "perl_xmmsclient.h"

MODULE = Audio::XMMSClient::Playlist	PACKAGE = Audio::XMMSClient::Playlist	PREFIX = xmmsc_playlist_

xmmsc_result_t *
xmmsc_playlist_list_entries (p)
		perl_xmmsclient_playlist_t *p
	C_ARGS:
		p->conn, p->name

xmmsc_result_t *
xmmsc_playlist_create (p)
		perl_xmmsclient_playlist_t *p
	C_ARGS:
		p->conn, p->name

xmmsc_result_t *
xmmsc_playlist_current_pos (p)
		perl_xmmsclient_playlist_t *p
	C_ARGS:
		p->conn, p->name

xmmsc_result_t *
xmmsc_playlist_shuffle (p)
		perl_xmmsclient_playlist_t *p
	C_ARGS:
		p->conn, p->name

xmmsc_result_t *
xmmsc_playlist_sort (p, properties)
		perl_xmmsclient_playlist_t *p
		const char **properties = ($type)perl_xmmsclient_unpack_char_ptr_ptr ($arg);
	C_ARGS:
		p->conn, p->name, properties
	CLEANUP:
		free (properties);

xmmsc_result_t *
xmmsc_playlist_clear (p)
		perl_xmmsclient_playlist_t *p
	C_ARGS:
		p->conn, p->name

xmmsc_result_t *
xmmsc_playlist_insert_id (p, pos, id)
		perl_xmmsclient_playlist_t *p
		int pos
		unsigned int id
	C_ARGS:
		p->conn, p->name, pos, id

xmmsc_result_t *
xmmsc_playlist_insert_args (p, pos, url, ...)
		perl_xmmsclient_playlist_t *p
		int pos
		const char *url
	PREINIT:
		int i, nargs;
		const char **args = NULL;
	INIT:
		nargs = items - 2;
		args = (const char **)malloc (sizeof (char *) * nargs);

		for (i = 0; i < nargs; i++) {
			args[i] = SvPV_nolen (ST (i+2));
		}
	C_ARGS:
		p->conn, p->name, pos, url, nargs, args
	CLEANUP:
		free (args);

xmmsc_result_t *
xmmsc_playlist_insert_url (p, pos, url)
		perl_xmmsclient_playlist_t *p
		int pos
		const char *url
	C_ARGS:
		p->conn, p->name, pos, url

xmmsc_result_t *
xmmsc_playlist_insert_encoded (p, pos, url)
		perl_xmmsclient_playlist_t *p
		int pos
		const char *url
	C_ARGS:
		p->conn, p->name, pos, url

xmmsc_result_t *
xmmsc_playlist_insert_collection (p, pos, collection, order)
		perl_xmmsclient_playlist_t *p
		int pos
		xmmsc_coll_t *collection
		const char **order = ($type)perl_xmmsclient_unpack_char_ptr_ptr ($arg);
	C_ARGS:
		p->conn, p->name, pos, collection, order
	CLEANUP:
		free (order);

xmmsc_result_t *
xmmsc_playlist_add_id (p, id)
		perl_xmmsclient_playlist_t *p
		unsigned int id
	C_ARGS:
		p->conn, p->name, id

xmmsc_result_t *
xmmsc_playlist_add_args (p, url, ...)
		perl_xmmsclient_playlist_t *p
		const char *url
	PREINIT:
		int i, nargs;
		const char **args = NULL;
	INIT:
		nargs = items - 1;
		args = (const char **)malloc (sizeof (char *) * nargs);

		for (i = 0; i < nargs; i++) {
			args[i] = SvPV_nolen (ST (i+1));
		}
	C_ARGS:
		p->conn, p->name, url, nargs, args
	CLEANUP:
		free (args);

xmmsc_result_t *
xmmsc_playlist_add_url (p, url)
		perl_xmmsclient_playlist_t *p
		const char *url
	C_ARGS:
		p->conn, p->name, url

xmmsc_result_t *
xmmsc_playlist_add_encoded (p, url)
		perl_xmmsclient_playlist_t *p
		const char *url
	C_ARGS:
		p->conn, p->name, url

xmmsc_result_t *
xmmsc_playlist_add_collection (p, collection, order)
		perl_xmmsclient_playlist_t *p
		xmmsc_coll_t *collection
		const char **order = ($type)perl_xmmsclient_unpack_char_ptr_ptr ($arg);
	C_ARGS:
		p->conn, p->name, collection, order
	CLEANUP:
		free (order);

xmmsc_result_t *
xmmsc_playlist_move_entry (p, cur_pos, new_pos)
		perl_xmmsclient_playlist_t *p
		uint32_t cur_pos
		uint32_t new_pos
	C_ARGS:
		p->conn, p->name, cur_pos, new_pos

xmmsc_result_t *
xmmsc_playlist_remove_entry (p, pos)
		perl_xmmsclient_playlist_t *p
		unsigned int pos
	C_ARGS:
		p->conn, p->name, pos

xmmsc_result_t *
xmmsc_playlist_remove (p)
		perl_xmmsclient_playlist_t *p
	C_ARGS:
		p->conn, p->name

xmmsc_result_t *
xmmsc_playlist_load (p)
		perl_xmmsclient_playlist_t *p
	C_ARGS:
		p->conn, p->name

xmmsc_result_t *
xmmsc_playlist_radd (p, url)
		perl_xmmsclient_playlist_t *p
		const char *url
	C_ARGS:
		p->conn, p->name, url

xmmsc_result_t *
xmmsc_playlist_radd_encoded (p, url)
		perl_xmmsclient_playlist_t *p
		const char *url
	C_ARGS:
		p->conn, p->name, url

void
DESTROY (p)
		perl_xmmsclient_playlist_t *p
	CODE:
		perl_xmmsclient_playlist_destroy (p);
