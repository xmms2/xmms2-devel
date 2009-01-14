/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2009 XMMS2 Team
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

#ifndef XMMSCLIENTPP_LIST_H
#define XMMSCLIENTPP_LIST_H

#include <xmmsclient/xmmsclient.h>
#include <boost/any.hpp>
#include <boost/shared_ptr.hpp>

#include <xmmsclient/xmmsclient++/dict.h>
#include <xmmsclient/xmmsclient++/typedefs.h>
#include <xmmsclient/xmmsclient++/exceptions.h>
#include <string>

#include <iostream>

namespace Xmms
{

	/** @class SuperList list.h "xmmsclient/xmmsclient++/list.h"
	 *  @brief Superclass for List classes.
	 */
	class SuperList
	{

		public:

			/** Constructor.
			 *
			 *  @param value xmmsv_t* extracted from the result, must
			 *               be a list.
			 *
			 *  @throw value_error If the value was in error state.
			 *  @throw not_list_error If the value is not a list.
			 *
			 *  @note You must unref the value you feed to this class.
			 */
			SuperList( xmmsv_t* value );

			/** Copy-constructor.
			 */
			SuperList( const SuperList& list );

			/** Copy assignment operator.
			 */
			SuperList& operator=( const SuperList& list );

			/** Destructor.
			 */
			virtual ~SuperList();

			/** Return to first entry in list.
			 *
			 *  @todo Should probably throw on error?
			 */
			virtual void first() const;

			/** Skip to next entry in list.
			 *
			 *  Advances to next list entry. May advance outside of list,
			 *  so isValid should be used to determine if end of list was
			 *  reached.
			 *
			 *  @todo Throw on error?
			 */
			virtual void operator++() const;

			/** Check if current listnode is inside list boundary.
			 */
			virtual bool isValid() const;

		protected:
			xmmsv_t* getElement() const;

		private:
			xmmsv_t* value_;
			xmmsv_list_iter_t *iter_;
	};

	/** @class List list.h "xmmsclient/xmmsclient++/list.h"
	 *  @brief This class acts as a wrapper for list type values.
	 *  This is actually a virtual class and is specialized with T being
	 *  - std::string
	 *  - int
	 *  - unsigned int
	 *  - Dict
	 *
	 *  If any other type is used, a compile-time error should occur.
	 */
	template< typename T >
	class List : public SuperList
	{

		public:

			/** Constructor
			 *  @see SuperList#SuperList.
			 */
			List( xmmsv_t* value ) :
				SuperList( value )
			{
			}

			/** Copy-constructor.
			 */
			List( const List<T>& list ) :
				SuperList( list )
			{
			}

			/** Copy assignment operator.
			 */
			List<T>& operator=( const List<T>& list )
			{
				SuperList::operator=( list );
				return *this;
			}

			/** Destructor.
			 */
			virtual ~List()
			{
			}

			/** Operator *.
			 *  Used to get the underlying value from the list.
			 */
			const T operator*() const
			{
				return constructContents();
			}

			/** Operator ->.
			 *  Used to call a function of the underlying class
			 *  (only applicable for std::string and Dict).
			 *  Same as (*list).function();
			 */
			const T operator->() const
			{
				return constructContents();
			}

		/** @cond */
		private:

			virtual T constructContents() const = 0;
		/** @endcond */

	};

	/** @cond */
	template<>
	class List< int > : public SuperList
	{

		public:
			List( xmmsv_t* value ) :
				SuperList( value )
			{
				// The typing of elements is checked when an element is fetched
			}

			List( const List<int>& list ) :
				SuperList( list )
			{
			}

			List<int>& operator=( const List<int>& list )
			{
				SuperList::operator=( list );
				return *this;
			}

			virtual ~List()
			{
			}

			int operator*() const
			{
				return constructContents();
			}

		private:

			virtual int constructContents() const
			{

				if( !isValid() ) {
					throw out_of_range( "List out of range or empty list" );
				}

				int temp = 0;
				xmmsv_t *elem = getElement();
				if( !xmmsv_get_int( elem, &temp ) ) {
					throw wrong_type_error( "Failed to retrieve an int "
					                        "from the list" );
				}
				return temp;

			}

	};

	template<>
	class List< unsigned int > : public SuperList
	{

		public:
			List( xmmsv_t* value ) :
				SuperList( value )
			{
				// The typing of elements is checked when an element is fetched
			}

			List( const List<unsigned int>& list ) :
				SuperList( list )
			{
			}

			List<unsigned int>& operator=( const List<unsigned int>& list )
			{
				SuperList::operator=( list );
				return *this;
			}

			virtual ~List()
			{
			}

			unsigned int operator*() const
			{
				return constructContents();
			}

		private:

			virtual unsigned int constructContents() const
			{

				if( !isValid() ) {
					throw out_of_range( "List out of range or empty list" );
				}

				unsigned int temp = 0;
				xmmsv_t *elem = getElement();
				if( !xmmsv_get_uint( elem, &temp ) ) {
					throw wrong_type_error( "Failed to retrieve an unsigned int "
					                        "from the list" );
				}
				return temp;

			}

	};

	template<>
	class List< std::string > : public SuperList
	{

		public:
			List( xmmsv_t* value ) :
				SuperList( value )
			{
				// The typing of elements is checked when an element is fetched
			}

			List( const List<std::string>& list ) :
				SuperList( list )
			{
			}

			List<std::string>& operator=( const List<std::string>& list )
			{
				SuperList::operator=( list );
				return *this;
			}

			virtual ~List()
			{
			}

			const std::string& operator*() const
			{
				constructContents();
				return str_;
			}

			const std::string* operator->() const
			{
				constructContents();
				return &str_;
			}

		private:

			mutable std::string str_;

			virtual void constructContents() const
			{

				if( !isValid() ) {
					throw out_of_range( "List out of range or empty list" );
				}

				const char* temp = 0;
				xmmsv_t *elem = getElement();
				if( !xmmsv_get_string( elem, &temp ) ) {
					throw wrong_type_error( "Failed to retrieve a string "
					                        "from the list" );
				}
				str_ = std::string( temp );

			}

	};

	template<>
	class List< Dict > : public SuperList
	{

		public:
			List( xmmsv_t* value ) try :
				SuperList( value )
			{
				// checking the type here is a bit useless since
				// Dict constructor checks it but we must catch it and
				// unref the value which SuperList refs or we leak.
			}
			catch( Xmms::not_list_error& e )
			{
				if( xmmsv_get_type( value ) != XMMSV_TYPE_NONE ) {

					// FIXME: Do we? It's not been ref'd by SuperList, so
					// it should be okay
//					xmmsv_unref( value );
					throw;

				}
			}

			List( const List<Dict>& list ) :
				SuperList( list ), dict_( list.dict_ )
			{
			}

			List<Dict>& operator=( const List<Dict>& list )
			{
				SuperList::operator=( list );
				return *this;
			}

			virtual ~List()
			{
			}

			const Dict& operator*() const
			{
				constructContents();
				return *dict_;
			}

			const Dict* operator->() const
			{
				constructContents();
				return dict_.get();
			}

		private:

			mutable boost::shared_ptr< Dict > dict_;

			virtual void constructContents() const
			{

				if( !isValid() ) {
					throw out_of_range( "List out of range or empty list" );
				}

				xmmsv_t *elem = getElement();
				dict_ = boost::shared_ptr< Dict >( new Dict( elem ) );

			}

	};
	/** @endcond */
}

#endif // XMMSCLIENTPP_LIST_H
