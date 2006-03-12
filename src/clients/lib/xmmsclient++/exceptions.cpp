#include <xmmsclient/xmmsclient++/exceptions.h>

#include <stdexcept>
using std::string;

#include <string>
using std::string;

namespace Xmms
{

	connection_error::connection_error( const string& what_arg ) :
		runtime_error( what_arg )
	{
	}

}
