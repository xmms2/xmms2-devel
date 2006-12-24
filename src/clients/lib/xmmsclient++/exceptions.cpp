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
		logic_error( what_arg )
	{
	}

	result_error::result_error( const string& what_arg ) :
		runtime_error( what_arg )
	{
	}

	not_dict_error::not_dict_error( const string& what_arg ) :
		logic_error( what_arg )
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

	wrong_type_error::wrong_type_error( const std::string& what_arg ) :
		logic_error( what_arg )
	{
	}

	missing_operand_error::missing_operand_error( const string& what_arg ) :
		runtime_error( what_arg )
	{
	}

	out_of_range::out_of_range( const std::string& what_arg ) :
		std::out_of_range( what_arg )
	{
	}

	collection_type_error::collection_type_error( const string& what_arg ) :
		runtime_error( what_arg )
	{
	}

	collection_operation_error::collection_operation_error( const string& what_arg ) :
		runtime_error( what_arg )
	{
	}

}
