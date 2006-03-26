#ifndef XMMSCLIENTPP_EXCEPTIONS_H
#define XMMSCLIENTPP_EXCEPTIONS_H

#include <stdexcept>
#include <string>

namespace Xmms
{

	class connection_error : public std::runtime_error
	{
		public:
			explicit connection_error( const std::string& what_arg );

	};

	class not_list_error : public std::runtime_error
	{
		public:
			explicit not_list_error( const std::string& what_arg );

	};

	class result_error : public std::runtime_error
	{
		public:
			explicit result_error( const std::string& what_arg );

	};

	class no_result_type_error : public std::runtime_error
	{
		public:
			explicit no_result_type_error( const std::string& what_arg );

	};

	class not_dict_error : public std::runtime_error
	{
		public:
			explicit not_dict_error( const std::string& what_arg );

	};

	class no_such_key_error : public std::runtime_error
	{
		public:
			explicit no_such_key_error( const std::string& what_arg );

	};

}

#endif // XMMSCLIENTPP_EXCEPTIONS_H
