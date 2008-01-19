/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2008 XMMS2 Team
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

#ifndef XMMSCLIENTPP_DICT_H
#define XMMSCLIENTPP_DICT_H

#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/exceptions.h>
#include <boost/variant.hpp>
#include <boost/function.hpp>
#include <string>
#include <list>

namespace Xmms
{
	
	/** @class Dict dict.h "xmmsclient/xmmsclient++/dict.h"
	 * @brief This class acts as a wrapper for dict type results.
	 */
	class Dict
	{

		public:

			typedef boost::variant< int32_t, uint32_t, std::string > Variant;

			/** Constructs Dict and references the result.
			 *  User must unref the result, the class does not take care of
			 *  it
			 *
			 * @param res Result to wrap around
			 *
			 * @throw not_dict_error Occurs if the result is not Dict
			 * @throw result_error Occurs if the result is in error state
			 */
			Dict( xmmsc_result_t* res );

			/** Constructs a copy of an existing Dict.
			 *  Adds to the reference counter.
			 *
			 * @param dict Dict to be copied
			 */
			Dict( const Dict& dict );

			/** Assigns an existing Dict to this Dict.
			 *  Adds to the reference counter.
			 *  
			 *  @param dict source Dict to be assigned to this Dict
			 */
			Dict& operator=( const Dict& dict );

			/** Destructs this Dict and unrefs the result.
			 */
			virtual ~Dict();

			/** Checks if the dict has a value for the key.
			 *  
			 *  @param key Key to look for
			 *
			 *  @return true if key exists, false if not
			 */
			virtual bool contains( const std::string& key ) const;

			/** Gets the corresponding value of the key.
			 *  This is basically the same as 
			 *  @link operator[]() operator[]@endlink but it does the 
			 *  conversion before returning.
			 *
			 *  @param key Key to look for
			 *
			 *  @return Value requested
			 *
			 *  @throw wrong_type_error If supplied type is of wrong type.
			 *  @throw no_such_key_error Occurs when key can't be found.
			 */
			template< typename T >
			T get( const std::string& key ) const
			{
				try {
					return boost::get< T >( this->operator[]( key ) );
				}
				catch( boost::bad_get& e ) {
					std::string error( "Failed to get value for " + key );
					throw wrong_type_error( error );
				}
			}

			/** Gets the corresponding value of the key.
			 *
			 * @param key Key to look for
			 *
			 * @return Xmms::Dict::Variant containing the value.
			 *         Use boost::get or type() member function 
			 *         to figure out the actual type of the returned value.
			 *         @n The return value can be of types:
			 *         - @c std::string
			 *         - @c unsigned int
			 *         - @c int
			 *
			 * @throws no_such_key_error Occurs when key can't be found.
			 */
			virtual Variant operator[]( const std::string& key ) const;

			typedef boost::function< void( const std::string&,
			                               const Variant& ) > ForEachFunc;

			virtual void each( const ForEachFunc& func ) const;
			
		/** @cond */
		protected:
			xmmsc_result_t* result_;
		/** @endcond */

	};

	
	/** @class PropDict dict.h "xmmsclient/xmmsclient++/dict.h"
	 * @brief This class acts as a wrapper for propdict type results.
	 */
	class PropDict : public Dict
	{

		public:

			/** Constructs PropDict and references the result.
			 *  User must unref the result, the class does not take care of
			 *  it
			 *
			 * @param res Result to wrap around
			 *
			 * @throw not_dict_error Occurs if the result is not PropDict
			 * @throw result_error Occurs if the result is in error state
			 */
			PropDict( xmmsc_result_t* res );

			/** Constructs a copy of an existing PropDict.
			 *  Adds to the reference counter.
			 *
			 * @param dict PropDict to be copied
			 */
			PropDict( const PropDict& dict );

			/** Assigns an existing PropDict to this PropDict.
			 *  Adds to the reference counter.
			 *  
			 *  @param dict source PropDict to be assigned to this PropDict
			 */
			PropDict& operator=( const PropDict& dict );

			/** Destructs this PropDict and unrefs the result.
			 */
			virtual ~PropDict();

			/** Set the source to use when retrieving data from the PropDict.
			 *
			 * @param src Name of the source to use
			 */
			virtual void setSource( const std::string& src ) const;

			/** Set the sources to use when retrieving data from the PropDict.
			 *
			 * @param src List of sources to use
			 */
			virtual void setSource( const std::list< std::string >& src ) const;

			typedef boost::function< void( const std::string&,
			                               const Dict::Variant&,
			                               const std::string& ) > ForEachFunc;

			virtual void each( const ForEachFunc& func ) const;

	};

}

#endif // XMMSCLIENTPP_DICT_H
