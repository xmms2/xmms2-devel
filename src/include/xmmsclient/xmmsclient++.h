#ifndef __XMMSCLIENTPP_H
#define __XMMSCLIENTPP_H

#include <sigc++/sigc++.h>
#include <iostream>
#include <xmmsclient/xmmsclient.h>

using namespace std;


class XMMSResult
{
	public:
		XMMSResult (xmmsc_result_t*);
		XMMSResult (const XMMSResult &);
		virtual ~XMMSResult ();

		uint getClass (void) { return xmmsc_result_get_class (m_res); }
		uint getType (void) { return xmmsc_result_get_type (m_res); }
		uint getCID (void) { return xmmsc_result_cid (m_res); }
		void restart (void);
		
		/* error */
		bool isError (void) { return xmmsc_result_iserror (m_res); }
		const char *getError (void) { return xmmsc_result_get_error (m_res); }

		/* wait for this resultset */
		void wait (void) const { xmmsc_result_wait (m_res); }

		void connect (const sigc::slot<void, XMMSResult*>& slot_);
		void emit (void) { m_signal.emit(this); }

	protected:
		xmmsc_result_t *m_res;

		bool m_inited;
		sigc::signal<void, XMMSResult*> m_signal;
};


class XMMSResultList
{
	public:
		XMMSResultList (xmmsc_result_t* res) : m_list_res(res) { }
		XMMSResultList (const XMMSResultList &src) : m_list_res(src.m_list_res) { }
		virtual ~XMMSResultList () { }

		bool listValid (void) { return xmmsc_result_list_valid (m_list_res); }
		bool listNext (void)  { return xmmsc_result_list_next (m_list_res); }
		bool listFirst (void) { return xmmsc_result_list_first (m_list_res); }

		bool isList (void) { return xmmsc_result_is_list (m_list_res); }

	private:
		xmmsc_result_t *m_list_res;
};


class XMMSResultDict : public XMMSResult
{
	public:
		XMMSResultDict (xmmsc_result_t* res) : XMMSResult(res) { }
		XMMSResultDict (const XMMSResultDict &src) : XMMSResult(src) { }
		virtual ~XMMSResultDict () { }
	
		void connect (const sigc::slot<void, XMMSResultDict*>& slot_) {
			XMMSResult::connect(sigc::retype(slot_));
		}

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


template <class T>
class XMMSResultValue : public XMMSResult
{
	public:
		XMMSResultValue (xmmsc_result_t* res) : XMMSResult(res) { }
		XMMSResultValue (const XMMSResultValue<T> &src) : XMMSResult(src) { }
		virtual ~XMMSResultValue () { }

		void connect (const sigc::slot<void, XMMSResultValue<T>*>& slot_) = 0;
		bool getValue (T *var) = 0;
};

template <>
class XMMSResultValue<int> : public XMMSResult
{
	public:
		XMMSResultValue (xmmsc_result_t* res) : XMMSResult(res) { }
		XMMSResultValue (const XMMSResultValue<int> &src) : XMMSResult(src) { }
		virtual ~XMMSResultValue () { }

		void connect (const sigc::slot<void, XMMSResultValue<int>*>& slot_) {
			XMMSResult::connect(sigc::retype(slot_));
		}

		bool getValue (int *var) { return xmmsc_result_get_int (m_res, var); }
};

template <>
class XMMSResultValue<uint> : public XMMSResult
{
	public:
		XMMSResultValue (xmmsc_result_t* res) : XMMSResult(res) { }
		XMMSResultValue (const XMMSResultValue<uint> &src) : XMMSResult(src) { }
		virtual ~XMMSResultValue () { }

		void connect (const sigc::slot<void, XMMSResultValue<uint>*>& slot_) {
			XMMSResult::connect(sigc::retype(slot_));
		}

		bool getValue (uint *var) { return xmmsc_result_get_uint (m_res, var); }
};

template <>
class XMMSResultValue<char*> : public XMMSResult
{
	public:
		XMMSResultValue (xmmsc_result_t* res) : XMMSResult(res) { }
		XMMSResultValue (const XMMSResultValue<char*> &src) : XMMSResult(src) { }
		virtual ~XMMSResultValue () { }

		void connect (const sigc::slot<void, XMMSResultValue<char*>*>& slot_) {
			XMMSResult::connect(sigc::retype(slot_));
		}

		bool getValue (char **var) { return xmmsc_result_get_string (m_res, var); }
};


template <class T>
class XMMSResultValueList : public XMMSResultList, public XMMSResultValue<T>
{
	public:
		XMMSResultValueList (xmmsc_result_t* res)
		  : XMMSResultList(res), XMMSResultValue<T>(res) { }
		XMMSResultValueList (const XMMSResultValueList<T> &src)
		  : XMMSResultList(src), XMMSResultValue<T>(src) { }
		virtual ~XMMSResultValueList() { }

		void connect (const sigc::slot<void, XMMSResultValueList<T>*>& slot_) {
			XMMSResult::connect(sigc::retype(slot_));
		}
};


class XMMSResultDictList : public XMMSResultList, public XMMSResultDict
{
	public:
		XMMSResultDictList (xmmsc_result_t* res)
		  : XMMSResultList(res), XMMSResultDict(res) { }
		XMMSResultDictList (const XMMSResultDictList &src)
		  : XMMSResultList(src), XMMSResultDict(src) { }
		virtual ~XMMSResultDictList() { }

		void connect (const sigc::slot<void, XMMSResultDictList*>& slot_) {
			XMMSResult::connect(sigc::retype(slot_));
		}
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

