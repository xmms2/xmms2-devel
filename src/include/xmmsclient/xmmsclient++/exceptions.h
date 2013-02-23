/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2013 XMMS2 Team
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */

#ifndef XMMSCLIENTPP_EXCEPTIONS_H
#define XMMSCLIENTPP_EXCEPTIONS_H

#include <stdexcept>
#include <string>

namespace Xmms
{

	// This should probably provide more methods like getFunction(), getLine() or something.
	/** @class exception exceptions.h "xmmsclient/xmmsclient++/exceptions.h"
	 *  @brief Base class for all Xmms:: exceptions.
	 *
	 *  @note Derive your exceptions from it if you want them to
	 *  be handled by library in asynchronous situations.
	 */
	class exception
	{
		public:
			virtual ~exception() throw() {}
	};

	// Someone kick doxygen for me please!, this line just can't be cut
	/** @class connection_error exceptions.h "xmmsclient/xmmsclient++/exceptions.h"
	 *  @brief Thrown on connection error when connecting or when calling a
	 *         function and not connected.
	 */
	class connection_error : public exception, public std::runtime_error
	{
		public:
			explicit connection_error( const std::string& what_arg );

	};

	/** @class not_list_error exceptions.h "xmmsclient/xmmsclient++/exceptions.h"
	 *  @brief Thrown when trying to create a list from non-list resultset.
	 *  @note Logic error, should <b>not</b> be caught, fix the code instead.
	 */
	class not_list_error : public exception, public std::logic_error
	{
		public:
			explicit not_list_error( const std::string& what_arg );

	};

	/** @class result_error exceptions.h "xmmsclient/xmmsclient++/exceptions.h"
	 *
	 *  @brief Thrown if the returned resultset was in error state.
	 */
	class result_error : public exception, public std::runtime_error
	{
		public:
			explicit result_error( const std::string& what_arg );

	};

	/** @class value_error exceptions.h "xmmsclient/xmmsclient++/exceptions.h"
	 *
	 *  @brief Thrown if the returned value is an error.
	 */
	class value_error : public exception, public std::runtime_error
	{
		public:
			explicit value_error( const std::string& what_arg );

	};

	/** @class argument_error exceptions.h "xmmsclient/xmmsclient++/exceptions.h"
	 *
	 *  @brief Thrown if the arguments passed to a method are invalid.
	 */
	class argument_error : public exception, public std::runtime_error
	{
		public:
			explicit argument_error( const std::string& what_arg );

	};

	/** @class not_dict_error exceptions.h "xmmsclient/xmmsclient++/exceptions.h"
	 *  @brief Thrown when trying to create a dict from non-dict resultset.
	 *  @note Logic error, should <b>not</b> be caught, fix the code instead.
	 */
	class not_dict_error : public exception, public std::logic_error
	{
		public:
			explicit not_dict_error( const std::string& what_arg );

	};

	/** @class no_such_key_error exceptions.h "xmmsclient/xmmsclient/exceptions.h"
	 *  @brief Thrown if trying to access a non-existant key in a Dict.
	 */
	class no_such_key_error : public exception, public std::runtime_error
	{
		public:
			explicit no_such_key_error( const std::string& what_arg );

	};

	/** @class mainloop_running_error exceptions.h "xmmsclient/xmmsclient++/exceptions.h"
	 *  @brief Thrown when calling a synchronous function and the mainloop is
	 *         running.
	 *  @note Logic error, should <b>not</b> be caught, fix the code instead.
	 */
	class mainloop_running_error : public exception, public std::logic_error
	{
		public:
			explicit mainloop_running_error( const std::string& what_arg );

	};

	/** @class wrong_type_error exceptions.h "xmmsclient/xmmsclient++/exceptions.h"
	 *  @brief Thrown from Dict::get if the type provided was wrong.
	 *  @note Logic error, should <b>not</b> be caught, fix the code instead.
	 */
	class wrong_type_error : public exception, public std::logic_error
	{
		public:
			explicit wrong_type_error( const std::string& what_arg );

	};

	/** @class missing_operand_error exceptions.h "xmmsclient/xmmsclient++/exceptions.h"
	 *
	 *  @brief Thrown if trying to retrieve a non-existing operand of a coll operator.
	 */
	class missing_operand_error : public exception, public std::runtime_error
	{
		public:
			explicit missing_operand_error( const std::string& what_arg );

	};

	/** @class out_of_range exceptions.h "xmmsclient/xmmsclient++/exceptions.h"
	 *
	 *  @brief Thrown if accessing a List which is at the end or over.
	 *  @note It's better to check for List validity
	 *        than have this caught/thrown.
	 */
	class out_of_range : public exception, public std::out_of_range
	{
		public:
			explicit out_of_range( const std::string& what_arg );

	};

	/** @class collection_type_error exceptions.h "xmmsclient/xmmsclient++/exceptions.h"
	 *
	 *  @brief Thrown if trying to perform an operation forbidden by
	 *         the type of the collection operator.
	 */
	class collection_type_error : public exception, public std::runtime_error
	{
		public:
			explicit collection_type_error( const std::string& what_arg );

	};

	/** @class collection_operation_error exceptions.h "xmmsclient/xmmsclient++/exceptions.h"
	 *
	 *  @brief Thrown if a collection operation failed.
	 */
	class collection_operation_error : public exception, public std::runtime_error
	{
		public:
			explicit collection_operation_error( const std::string& what_arg );

	};

	/** @class collection_parsing_error exceptions.h "xmmsclient/xmmsclient++/exceptions.h"
	 *
	 *  @brief Thrown if the parsing of a collection pattern failed.
	 */
	class collection_parsing_error : public exception, public std::runtime_error
	{
		public:
			explicit collection_parsing_error( const std::string& what_arg );

	};

	/** @class invalid_url exceptions.h "xmmsclient/xmmsclient++/exceptions.h"
	 *
	 *  @brief Thrown if an invalid url is used.
	 */
	class invalid_url : public exception, public std::runtime_error
	{
		public:
			explicit invalid_url( const std::string& what_arg );

	};

}

#endif // XMMSCLIENTPP_EXCEPTIONS_H
