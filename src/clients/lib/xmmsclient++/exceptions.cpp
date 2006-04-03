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

	not_list_error::not_list_error( const string& what_arg ) :
		runtime_error( what_arg )
	{
	}

	result_error::result_error( const string& what_arg ) :
		runtime_error( what_arg )
	{
	}

	no_result_type_error::no_result_type_error( const string& what_arg ) :
		runtime_error( what_arg )
	{
	}

	not_dict_error::not_dict_error( const string& what_arg ) :
		runtime_error( what_arg )
	{
	}

	no_such_key_error::no_such_key_error( const string& what_arg ) :
		runtime_error( what_arg )
	{
	}

	mainloop_running_error::mainloop_running_error( const string& what_arg ) :
		logic_error( what_arg )
	{
	}

}
