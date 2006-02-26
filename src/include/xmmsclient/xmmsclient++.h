#ifndef __XMMSCLIENTPP_H
#define __XMMSCLIENTPP_H

#include <sigc++/sigc++.h>
#include <iostream>
#include <xmmsclient/xmmsclient.h>

using namespace std;


class _XMMSResult
{
	public:
		_XMMSResult (xmmsc_result_t*);
		_XMMSResult (const _XMMSResult &);
		~_XMMSResult ();

		uint getClass (void) { return xmmsc_result_get_class (m_res); }
		uint getType (void) { return xmmsc_result_get_type (m_res); }
		void restart (void);
		
		/* error */
		bool isError (void) { return xmmsc_result_iserror (m_res); }
		const char *getError (void) { return xmmsc_result_get_error (m_res); }

		/* wait for this resultset */
		void wait (void) const { xmmsc_result_wait (m_res); }

	protected:
		xmmsc_result_t *m_res;
};


class XMMSResultList
{
	public:
		XMMSResultList (xmmsc_result_t* res) : m_list_res(res) { }
		XMMSResultList (const XMMSResultList &src) : m_list_res(src.m_list_res) { }
		~XMMSResultList () { }

		bool listValid (void) { return xmmsc_result_list_valid (m_list_res); }
		bool listNext (void)  { return xmmsc_result_list_next (m_list_res); }
		bool listFirst (void) { return xmmsc_result_list_first (m_list_res); }

		bool isList (void) { return xmmsc_result_is_list (m_list_res); }

	private:
		xmmsc_result_t *m_list_res;
};


class _XMMSResultDict : public _XMMSResult
{
	public:
		_XMMSResultDict (xmmsc_result_t* res) : _XMMSResult(res) { }
		_XMMSResultDict (const _XMMSResultDict &src) : _XMMSResult(src) { }
		~_XMMSResultDict () { }
	
		std::list<const char*> getPropDictKeys ();
		std::list<const char*> getDictKeys ();

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


class _XMMSResultValueInt : public _XMMSResult
{
	public:
		_XMMSResultValueInt (xmmsc_result_t* res) : _XMMSResult(res) { }
		_XMMSResultValueInt (const _XMMSResultValueInt &src) : _XMMSResult(src) { }
		~_XMMSResultValueInt () { }
	
		bool getValue (int *var) { return xmmsc_result_get_int (m_res, var); }
};

class _XMMSResultValueUint : public _XMMSResult
{
	public:
		_XMMSResultValueUint (xmmsc_result_t* res) : _XMMSResult(res) { }
		_XMMSResultValueUint (const _XMMSResultValueUint &src) : _XMMSResult(src) { }
		~_XMMSResultValueUint () { }
	
		bool getValue (uint *var) { return xmmsc_result_get_uint (m_res, var); }
};

class _XMMSResultValueString : public _XMMSResult
{
	public:
		_XMMSResultValueString (xmmsc_result_t* res) : _XMMSResult(res) { }
		_XMMSResultValueString (const _XMMSResultValueString &src) : _XMMSResult(src) { }
		~_XMMSResultValueString () { }
	
		bool getValue (char **var) { return xmmsc_result_get_string (m_res, var); }
};


class _XMMSResultValueListInt : public XMMSResultList, public _XMMSResultValueInt
{
	public:
		_XMMSResultValueListInt (xmmsc_result_t* res)
		  : XMMSResultList(res), _XMMSResultValueInt(res) { }
		_XMMSResultValueListInt (const _XMMSResultValueListInt &src)
		  : XMMSResultList(src), _XMMSResultValueInt(src) { }
		~_XMMSResultValueListInt() { }
};

class _XMMSResultValueListUint : public XMMSResultList, public _XMMSResultValueUint
{
	public:
		_XMMSResultValueListUint (xmmsc_result_t* res)
		  : XMMSResultList(res), _XMMSResultValueUint(res) { }
		_XMMSResultValueListUint (const _XMMSResultValueListUint &src)
		  : XMMSResultList(src), _XMMSResultValueUint(src) { }
		~_XMMSResultValueListUint() { }
};

class _XMMSResultValueListString : public XMMSResultList, public _XMMSResultValueString
{
	public:
		_XMMSResultValueListString (xmmsc_result_t* res)
		  : XMMSResultList(res), _XMMSResultValueString(res) { }
		_XMMSResultValueListString (const _XMMSResultValueListString &src)
		  : XMMSResultList(src), _XMMSResultValueString(src) { }
		~_XMMSResultValueListString() { }
};


class _XMMSResultDictList : public XMMSResultList, public _XMMSResultDict
{
	public:
		_XMMSResultDictList (xmmsc_result_t* res)
		  : XMMSResultList(res), _XMMSResultDict(res) { }
		_XMMSResultDictList (const _XMMSResultDictList &src)
		  : XMMSResultList(src), _XMMSResultDict(src) { }
		~_XMMSResultDictList() { }
};



/* Now come the real result classes, "enriched" with an associated signal. */

template <class T>
void  generic_handler (xmmsc_result_t *res, void *userdata)
{
	T r = static_cast<T>(userdata);
	if (!r) {
		cout << "********* FATAL ERROR ***********" << endl;
		cout << "The generic handler was called without a result!" << endl;
		return;
	}
	r->emit ();
}

template <class T>
class XMMSSigRes : public T
{
	public:
		XMMSSigRes (xmmsc_result_t* res) : T(res), m_inited(false), m_signal() { }
		XMMSSigRes (const XMMSSigRes<T> &src)
		  : T(src), m_inited(src.m_inited), m_signal(src.m_signal) { }
		~XMMSSigRes () { }

		void connect (const sigc::slot<void, XMMSSigRes<T>*>& slot_)
		{
			if (!m_inited) {
				xmmsc_result_notifier_set (T::m_res, &generic_handler<XMMSSigRes<T>*>, this);
				m_inited = true;
			}
			m_signal.connect (slot_);
		}

		void emit (void) { m_signal.emit(this); }

	private:
		bool m_inited;
		sigc::signal<void, XMMSSigRes<T>*> m_signal;

};

typedef XMMSSigRes<_XMMSResult> XMMSResult;
typedef XMMSSigRes<_XMMSResultDict> XMMSResultDict;
typedef XMMSSigRes<_XMMSResultDictList> XMMSResultDictList;

typedef XMMSSigRes<_XMMSResultValueInt> XMMSResultValueInt;
typedef XMMSSigRes<_XMMSResultValueUint> XMMSResultValueUint;
typedef XMMSSigRes<_XMMSResultValueString> XMMSResultValueString;

typedef XMMSSigRes<_XMMSResultValueListInt> XMMSResultValueListInt;
typedef XMMSSigRes<_XMMSResultValueListUint> XMMSResultValueListUint;
typedef XMMSSigRes<_XMMSResultValueListString> XMMSResultValueListString;



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

