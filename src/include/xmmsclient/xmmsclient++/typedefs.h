#ifndef XMMSCLIENTPP_TYPEDEFS_H
#define XMMSCLIENTPP_TYPEDEFS_H

#include <vector>
#include <map>
#include <string>
#include <boost/any.hpp>
#include <boost/shared_ptr.hpp>
#include <xmmsclient/xmmsclient++/list.h>
#include <xmmsclient/xmmsclient++/dict.h>

namespace Xmms 
{

	template< typename T >
	class List;

	typedef List< Dict > DictList;

	typedef boost::shared_ptr< Dict > DictPtr;
	typedef boost::shared_ptr< DictList > DictListPtr;

}

#endif // XMMSCLIENTPP_TYPEDEFS_H
