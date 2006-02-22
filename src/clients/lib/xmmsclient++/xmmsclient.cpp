#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++.h>
#include <sigc++/sigc++.h>

using namespace std;

void 
generic_handler (xmmsc_result_t *res, void *userdata) 
{
	XMMSResult *r = static_cast<XMMSResult*>(userdata);
	if (!r) {
		cout << "********* FATAL ERROR ***********" << endl;
		cout << "The generic handler was called without a result!" << endl;
		return;
	}
	r->emit ();
}

XMMSClient::XMMSClient (const char *name)
{
	m_xmmsc = xmmsc_init (name);
}

bool
XMMSClient::connect (const char *path)
{
	if (!xmmsc_connect (m_xmmsc, path)) {
		return false;
	}

	return true;
}

XMMSClient::~XMMSClient ()
{
	xmmsc_unref (m_xmmsc);
}

_XMMSResult::_XMMSResult (xmmsc_result_t *res)
  : m_res(res)
{
	cout << "result created" << endl;
}

_XMMSResult::_XMMSResult (const _XMMSResult &src)
  : m_res(src.m_res)
{
}

void
_XMMSResult::restart (void)
{
	xmmsc_result_t *nres;
	nres = xmmsc_result_restart (m_res);
	xmmsc_result_unref (m_res);
	xmmsc_result_unref (nres);
	m_res = nres;
}

static void
dict_foreach (const void *key, 
			  xmmsc_result_value_type_t type, 
			  const void *value, 
			  void *udata)
{
	list<const char *> *i (static_cast<list<const char*>*>(udata));
	i->push_front (static_cast<const char*>(key));
}

list<const char *>
_XMMSResultDict::getDictKeys (void)
{
	list<const char *> i;

	xmmsc_result_dict_foreach (m_res, dict_foreach, static_cast<void*>(&i));

	return i;
}

static void
propdict_foreach (const void *key, 
			  xmmsc_result_value_type_t type, 
			  const void *value, 
			  const char *source,
			  void *udata)
{
	list<const char *> *i (static_cast<list<const char*>*>(udata));
	i->push_front (static_cast<const char*>(key));
}

list<const char *>
_XMMSResultDict::getPropDictKeys (void)
{
	list<const char *> i;

	xmmsc_result_propdict_foreach (m_res, propdict_foreach, static_cast<void*>(&i));

	return i;
}

_XMMSResult::~_XMMSResult ()
{
	cout << "result destroy" << endl;
	xmmsc_result_unref (m_res);
}
