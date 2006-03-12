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

}

#endif // XMMSCLIENTPP_EXCEPTIONS_H
