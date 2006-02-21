#ifndef __XMMSCLIENTPP_H
#define __XMMSCLIENTPP_H

#include <sigc++/sigc++.h>
#include <iostream>
#include <xmmsclient/xmmsclient.h>


class XMMSResult
{
	public:
		XMMSResult (xmmsc_result_t*);
		XMMSResult (const XMMSResult &);
		~XMMSResult ();

		uint getType (void) { return xmmsc_result_get_type (m_res); }
		void restart (void);
		
		/* error */
		bool isError (void) { return xmmsc_result_iserror (m_res); }
		const char *getError (void) { return xmmsc_result_get_error (m_res); }

		/* wait for this resultset */
		void wait (void) const { xmmsc_result_wait (m_res); }

		/* connect to signal */
		void connect (const sigc::slot<void, XMMSResult*>& slot_);

		/* don't use me */
		void emit (void);

	protected:
		xmmsc_result_t *m_res;

	private:
		bool m_inited;
		sigc::signal1<void, XMMSResult*> m_signal; // FIXME: Oops, dynamic type here?

		xmmsc_result_t *getRes (void) { return m_res; }
};


class XMMSResultList : virtual public XMMSResult {
	public:
		XMMSResultList (xmmsc_result_t* res) : XMMSResult(res) { }
		XMMSResultList (const XMMSResultList &src) : XMMSResult(src) { }
		~XMMSResultList ();

		bool listValid (void) { return xmmsc_result_list_valid (m_res); }
		bool listNext (void) { return xmmsc_result_list_next (m_res); }
		bool listFirst (void) { return xmmsc_result_list_first (m_res); }

		bool isList (void) { return xmmsc_result_is_list (m_res); }
};


class XMMSResultDict : virtual public XMMSResult {
	public:
		XMMSResultDict (xmmsc_result_t* res) : XMMSResult(res) { }
		XMMSResultDict (const XMMSResultDict &src) : XMMSResult(src) { }
		~XMMSResultDict () { }
	
		std::list<const char*> *getPropDictList ();
		std::list<const char*> *getDictList ();

		uint getDictValueType (const char *key) {
			return xmmsc_result_get_dict_entry_type (m_res, key);
		}

		int entryFormat (char *target, int len, const char *format) {
			return xmmsc_entry_format (target, len, format, m_res);
		}

		bool getValue (const char* key, int *var) {
			return xmmsc_result_get_dict_entry_int32 (m_res, key, var); 
		}
		bool getValue (const char* key, uint *var) {
			return xmmsc_result_get_dict_entry_uint32 (m_res, key, var); 
		}
		bool getValue (const char* key, char **var) {
			return xmmsc_result_get_dict_entry_str (m_res, key, var); 
		}
};



template <class T>
class XMMSResultValue : virtual public XMMSResult
{
	public:
		XMMSResultValue (xmmsc_result_t* res) : XMMSResult(res) { }
		XMMSResultValue (const XMMSResultValue<T> &src) : XMMSResult(src) { }
		~XMMSResultValue () { }
	
		bool getValue (T *var) = 0;
};

template <>
class XMMSResultValue<int> : virtual public XMMSResult
{
	public:
		XMMSResultValue (xmmsc_result_t* res) : XMMSResult(res) { }
		XMMSResultValue (const XMMSResultValue<int> &src) : XMMSResult(src) { }
		~XMMSResultValue () { }
	
		bool getValue (int *var) { return xmmsc_result_get_int (m_res, var); }
};

template <>
class XMMSResultValue<uint> : virtual public XMMSResult
{
	public:
		XMMSResultValue (xmmsc_result_t* res) : XMMSResult(res) { }
		XMMSResultValue (const XMMSResultValue<uint> &src) : XMMSResult(src) { }
		~XMMSResultValue () { }
	
		bool getValue (uint *var) { return xmmsc_result_get_uint (m_res, var); }
};

template <>
class XMMSResultValue<char*> : virtual public XMMSResult
{
	public:
		XMMSResultValue (xmmsc_result_t* res) : XMMSResult(res) { }
		XMMSResultValue (const XMMSResultValue<char*> &src) : XMMSResult(src) { }
		~XMMSResultValue () { }
	
		bool getValue (char **var) { return xmmsc_result_get_string (m_res, var); }
};



template <class T>
class XMMSResultValueList : public XMMSResultList, public XMMSResultValue<T>
{
	public:
		XMMSResultValueList (xmmsc_result_t* res) : XMMSResultValue<T>(res) { }
		XMMSResultValueList (const XMMSResultValueList<T> &src) : XMMSResultValue<T>(src) { }
		~XMMSResultValueList() { }
};

class XMMSResultDictList : public XMMSResultList, public XMMSResultDict
{
	public:
		// FIXME: Is this actually working, or do we mess with signals?
		XMMSResultDictList (xmmsc_result_t* res) : XMMSResult(res), XMMSResultList(res), XMMSResultDict(res) { }
		XMMSResultDictList (const XMMSResultDictList &src) : XMMSResult(src), XMMSResultList(src), XMMSResultDict(src) { }
		~XMMSResultDictList() { }
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

