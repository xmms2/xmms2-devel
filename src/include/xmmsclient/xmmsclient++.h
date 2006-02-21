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

		/* connect to signal */
		void connect (const sigc::slot<void, XMMSResult*>& slot_);

		/* wait for this resultset */
		void wait (void) const { xmmsc_result_wait (m_res); }

		/* don't use me */
		void emit (void);

	protected:
		xmmsc_result_t *m_res;

	private:
		bool m_inited;
		sigc::signal1<void, XMMSResult*> m_signal;

		xmmsc_result_t *getRes (void) { return m_res; }
};


class XMMSResultList : public XMMSResult {
	public:
		XMMSResultList (xmmsc_result_t* res) : XMMSResult(res) { }
		XMMSResultList (const XMMSResultList &src) : XMMSResult(src) { }
		~XMMSResultList ();

		bool listValid (void) { return xmmsc_result_list_valid (m_res); }
		bool listNext (void) { return xmmsc_result_list_next (m_res); }
		bool listFirst (void) { return xmmsc_result_list_first (m_res); }

		bool isList (void) { return xmmsc_result_is_list (m_res); }
};


class XMMSDict : public XMMSResult {
	public:
		XMMSDict (xmmsc_result_t* res) : XMMSResult(res) { }
		XMMSDict (const XMMSDict &src) : XMMSResult(src) { }
		~XMMSDict () { }
	
		std::list<const char*> *getPropDictList ();
		std::list<const char*> *getDictList ();

		uint getDictValueType (const char *key) {
			return xmmsc_result_get_dict_entry_type (m_res, key);
		}

		int entryFormat (char *target, int len, const char *format) {
			return xmmsc_entry_format (target, len, format, m_res);
		}

};



template <class T>
class XMMSResultValue : public XMMSResult
{
	public:
		XMMSResultValue (xmmsc_result_t* res) : XMMSResult(res) { }
		XMMSResultValue (const XMMSResultValue<T> &src) : XMMSResult(src) { }
		~XMMSResultValue () { }
	
		bool getValue (T *var) = 0;
};

template <>
class XMMSResultValue<int> : public XMMSResult
{
	public:
		XMMSResultValue (xmmsc_result_t* res) : XMMSResult(res) { }
		XMMSResultValue (const XMMSResultValue<int> &src) : XMMSResult(src) { }
		~XMMSResultValue () { }
	
		bool getValue (int *var) { return xmmsc_result_get_int (m_res, var); }
};

template <>
class XMMSResultValue<uint> : public XMMSResult
{
	public:
		XMMSResultValue (xmmsc_result_t* res) : XMMSResult(res) { }
		XMMSResultValue (const XMMSResultValue<uint> &src) : XMMSResult(src) { }
		~XMMSResultValue () { }
	
		bool getValue (uint *var) { return xmmsc_result_get_uint (m_res, var); }
};

template <>
class XMMSResultValue<char*> : public XMMSResult
{
	public:
		XMMSResultValue (xmmsc_result_t* res) : XMMSResult(res) { }
		XMMSResultValue (const XMMSResultValue<char*> &src) : XMMSResult(src) { }
		~XMMSResultValue () { }
	
		bool getValue (char **var) { return xmmsc_result_get_string (m_res, var); }
};



template <class T>
class XMMSResultDict : public XMMSDict
{
	public:
		XMMSResultDict (xmmsc_result_t* res) : XMMSDict(res) { }
		XMMSResultDict (const XMMSResultDict<T> &src) : XMMSDict(src) { }
		~XMMSResultDict () { }

		bool getValue (const char* key, T *var) = 0;
};

template <>
class XMMSResultDict<int> : public XMMSDict
{
	public:
		XMMSResultDict (xmmsc_result_t* res) : XMMSDict(res) { }
		XMMSResultDict (const XMMSResultDict<int> &src) : XMMSDict(src) { }
		~XMMSResultDict () { }
	
		bool getValue (const char* key, int *var) {
			return xmmsc_result_get_dict_entry_int32 (m_res, key, var); 
		}
};

template <>
class XMMSResultDict<uint> : public XMMSDict
{
	public:
		XMMSResultDict (xmmsc_result_t* res) : XMMSDict(res) { }
		XMMSResultDict (const XMMSResultDict<uint> &src) : XMMSDict(src) { }
		~XMMSResultDict () { }
	
		bool getValue (const char* key, uint *var) {
			return xmmsc_result_get_dict_entry_uint32 (m_res, key, var); 
		}
};

template <>
class XMMSResultDict<char*> : public XMMSDict
{
	public:
		XMMSResultDict (xmmsc_result_t* res) : XMMSDict(res) { }
		XMMSResultDict (const XMMSResultDict<char*> &src) : XMMSDict(src) { }
		~XMMSResultDict () { }
	
		bool getValue (const char* key, char **var) {
			return xmmsc_result_get_dict_entry_str (m_res, key, var); 
		}
};


template <class T>
class XMMSResultValueList : public XMMSResultList, public XMMSResultValue<T>
{
	public:
		XMMSResultValueList (xmmsc_result_t* res) : XMMSResultValue<T>(res) { }
		XMMSResultValueList (const XMMSResultValueList<T> &src) : XMMSResultValue<T>(src) { }
		~XMMSResultValueList() { }
};

template <class T>
class XMMSResultDictList : public XMMSResultList, public XMMSResultDict<T>
{
	public:
		XMMSResultDictList (xmmsc_result_t* res) : XMMSResultDict<T>(res) { }
		XMMSResultDictList (const XMMSResultDictList<T> &src) : XMMSResultDict<T>(src) { }
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

