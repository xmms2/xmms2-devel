#ifndef XMMSCLIENTPP_EXCEPTIONS_H
#define XMMSCLIENTPP_EXCEPTIONS_H

#include <stdexcept>
#include <string>

namespace Xmms
{

	// These should probably be inherited from something like Xmms::error
	// to provide more methods like getFunction(), getLine() or something.

	// Someone kick doxygen for me please!, this line just can't be cut
	/** @class connection_error exceptions.h "xmmsclient/xmmsclient++/exceptions.h"
	 *  @brief Thrown on connection error when connecting or when calling a
	 *         function and not connected.
	 */
	class connection_error : public std::runtime_error
	{
		public:
			explicit connection_error( const std::string& what_arg );

	};

	/** @class not_list_error exceptions.h "xmmsclient/xmmsclient++/exceptions.h"
	 *  @brief Thrown when trying to create a list from non-list resultset.
	 *  @note Logic error, should <b>not</b> be caught, fix the code instead.
	 */
	class not_list_error : public std::logic_error
	{
		public:
			explicit not_list_error( const std::string& what_arg );

	};

	/** @class result_error exceptions.h "xmmsclient/xmmsclient++/exceptions.h"
	 *
	 *  @brief Thrown if the returned resultset was in error state.
	 */
	class result_error : public std::runtime_error
	{
		public:
			explicit result_error( const std::string& what_arg );

	};

	/** @class not_dict_error exceptions.h "xmmsclient/xmmsclient++/exceptions.h"
	 *  @brief Thrown when trying to create a dict from non-dict resultset.
	 *  @note Logic error, should <b>not</b> be caught, fix the code instead.
	 */
	class not_dict_error : public std::logic_error
	{
		public:
			explicit not_dict_error( const std::string& what_arg );

	};

	/** @class no_such_key_error exceptions.h "xmmsclient/xmmsclient/exceptions.h"
	 *  @brief Thrown if trying to access a non-existant key in a Dict.
	 */
	class no_such_key_error : public std::runtime_error
	{
		public:
			explicit no_such_key_error( const std::string& what_arg );

	};

	/** @class mainloop_running_error exceptions.h "xmmsclient/xmmsclient++/exceptions.h"
	 *  @brief Thrown when calling a synchronous function and the mainloop is
	 *         running.
	 *  @note Logic error, should <b>not</b> be caught, fix the code instead.
	 */
	class mainloop_running_error : public std::logic_error
	{
		public:
			explicit mainloop_running_error( const std::string& what_arg );

	};

	/** @class wrong_type_error exceptions.h "xmmsclient/xmmsclient++/exceptions.h"
	 *  @brief Thrown from Dict::get if the type provided was wrong.
	 *  @note Logic error, should <b>not</b> be caught, fix the code instead.
	 */
	class wrong_type_error : public std::logic_error
	{
		public:
			explicit wrong_type_error( const std::string& what_arg );

	};

	/** @class out_of_range exceptions.h "xmmsclient/xmmsclient++/exceptions.h"
	 *
	 *  @brief Thrown if accessing a List which is at the end or over.
	 *  @note It's better to check for List validity
	 *        than have this caught/thrown.
	 */
	class out_of_range : public std::out_of_range
	{
		public:
			explicit out_of_range( const std::string& what_arg );

	};

}

#endif // XMMSCLIENTPP_EXCEPTIONS_H
