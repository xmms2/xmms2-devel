#ifndef XMMSCLIENTPP_TYPEDEFS_H
#define XMMSCLIENTPP_TYPEDEFS_H

#include <vector>
#include <map>
#include <string>
#include <boost/any.hpp>
#include <boost/shared_ptr.hpp>

namespace Xmms 
{

	typedef std::map< std::string, boost::any > Dict;
	typedef std::vector< Dict > DictList;

	typedef boost::shared_ptr< Dict > DictPtr;
	typedef boost::shared_ptr< DictList > DictListPtr;

}

#endif // XMMSCLIENTPP_TYPEDEFS_H
