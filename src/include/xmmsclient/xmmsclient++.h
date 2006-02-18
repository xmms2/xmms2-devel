#ifndef __XMMSCLIENTPP_H
#define __XMMSCLIENTPP_H

#include <iostream>
#include <xmmsclient/xmmsclient.h>
#include <sigc++/signal.h>


class XMMSResult
{
	public:
		XMMSResult (xmmsc_result_t*);
		~XMMSResult ();

		void restart (void);

		/* Simple value retrieval */
		bool getValue (uint *i) { return xmmsc_result_get_uint (m_res, i); }
		bool getValue (int *i) { return xmmsc_result_get_int (m_res, i); }
		bool getValue (char **s) { return xmmsc_result_get_string (m_res, s); }

		uint getType (void) { return xmmsc_result_get_type (m_res); }

		/* Dict retrieval */
		bool getDictValue (const char *key, uint *i) 
		{ 
			return xmmsc_result_get_dict_entry_uint32 (m_res, key, i); 
		}
		bool getDictValue (const char *key, int *i) 
		{ 
			return xmmsc_result_get_dict_entry_int32 (m_res, key, i); 
		}
		bool getDictValue (const char *key, char **s) 
		{ 
			return xmmsc_result_get_dict_entry_str (m_res, key, s); 
		}

		std::list<const char*> *getPropDictList ();
		std::list<const char*> *getDictList ();
		uint getDictValueType (const char *key) { return xmmsc_result_get_dict_entry_type (m_res, key); }

		/* List manipulation */
		bool listValid (void) { return xmmsc_result_list_valid (m_res); }
		bool listNext (void) { return xmmsc_result_list_next (m_res); }
		bool listFirst (void) { return xmmsc_result_list_first (m_res); }
		bool isList (void) { return xmmsc_result_is_list (m_res); }
		
		/* error */
		bool isError (void) { return xmmsc_result_iserror (m_res); }
		const char *getError (void) { return xmmsc_result_get_error (m_res); }

		/* connect to signal */
		void connect (const sigc::slot<void, XMMSResult*>& slot_);

		/* wait for this resultset */
		void wait (void) const { xmmsc_result_wait (m_res); }

		/* don't use me */
		void emit (void);

	private:
		bool m_inited;
		xmmsc_result_t *m_res;
		sigc::signal1<void, XMMSResult*> *m_signal;

		xmmsc_result_t *getRes (void) { return m_res; }
};


class XMMSClient
{
	public:
		XMMSClient (const char *name);
		~XMMSClient ();

		bool connect (const char *path);

		xmmsc_connection_t *getConn(void) { return m_xmmsc; }

#include <xmmsclient/xmmsclient++_methods.h>

	private:
		xmmsc_connection_t *m_xmmsc;
};

#endif

