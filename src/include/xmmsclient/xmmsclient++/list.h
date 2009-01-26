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
#include <iterator>

namespace Xmms
{
	template< typename T >
	struct type_traits;

	template<>
	struct type_traits< int32_t >
	{
		typedef int32_t type;
		static int (*get_func)( const xmmsv_t*, int32_t* );
	};

	template<>
	struct type_traits< uint32_t >
	{
		typedef uint32_t type;
		static int (*get_func)( const xmmsv_t*, uint32_t* );
	};

	template<>
	struct type_traits< std::string >
	{
		typedef const char* type;
		static int (*get_func)( const xmmsv_t*, const char** );
	};

	template<>
	struct type_traits< Dict >
	{
	};

	namespace {
		template< typename T >
		T construct( xmmsv_t* elem )
		{
			typename type_traits< T >::type temp = 0;
			if( !type_traits< T >::get_func( elem, &temp ) ) {
				throw wrong_type_error( "Failed to retrieve a value"
				                        " from the list" );
			}
			return T( temp );
		}
		template<>
		Dict construct( xmmsv_t* elem )
		{
			return Dict( elem );
		}
	}

	template< typename T >
	class List;

	template< typename T >
	class List_const_iterator_
	{
		private:
			List_const_iterator_( xmmsv_t* );
			friend class List< T >;
		public:
			typedef ptrdiff_t difference_type;
			typedef std::forward_iterator_tag iterator_category;
			typedef T value_type;
			typedef value_type& reference;
			typedef value_type* pointer;

			List_const_iterator_();
			List_const_iterator_( const List_const_iterator_& );
			List_const_iterator_& operator=( const List_const_iterator_& );
			~List_const_iterator_();
			const value_type& operator*() const;
			const value_type* operator->() const;
			List_const_iterator_& operator++();
			List_const_iterator_ operator++( int );
			List_const_iterator_& operator--();
			List_const_iterator_ operator--( int );
			bool equal( const List_const_iterator_& rh ) const;

		private:
			xmmsv_t* getElement() const
			{
				xmmsv_t *elem = NULL;
				xmmsv_list_iter_entry( it_, &elem );
				return elem;
			}
			bool valid() const;

			xmmsv_t* list_;
			xmmsv_list_iter_t* it_;
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
	class List
	{

		public:

			typedef List_const_iterator_< T > const_iterator;

			/** Constructor
			 *  @see SuperList#SuperList.
			 */
			List( xmmsv_t* value ) :
				value_( 0 )
			{
				if( xmmsv_is_error( value ) ) {
					const char *buf;
					xmmsv_get_error( value, &buf );
					throw value_error( buf );
				}
				if( !xmmsv_is_type( value, XMMSV_TYPE_LIST ) ) {
					throw not_list_error( "Provided value is not a list" );
				}

				value_ = value;
				xmmsv_ref( value_ );
			}

			/** Copy-constructor.
			 */
			List( const List<T>& list ) :
				value_( list.value_ )
			{
				xmmsv_ref( value_ );
			}

			/** Copy assignment operator.
			 */
			List<T>& operator=( const List<T>& list )
			{
				value_ = list.value_;
				xmmsv_ref( value_ );
				return *this;
			}

			/** Destructor.
			 */
			~List()
			{
				xmmsv_unref( value_ );
			}

			const_iterator begin() const
			{
				return const_iterator( value_ );
			}
			const_iterator end() const
			{
				return const_iterator();
			}

		/** @cond */
		private:
			xmmsv_t* value_;
		/** @endcond */

	};

	template< typename T >
	inline
	bool operator==( const List_const_iterator_< T >& lh, const List_const_iterator_< T >& rh )
	{
		return lh.equal( rh );
	}

	template< typename T >
	inline
	bool operator!=( const List_const_iterator_< T >& lh, const List_const_iterator_< T >& rh )
	{
		return !( lh == rh );
	}

	template< typename T >
	List_const_iterator_< T >::List_const_iterator_( xmmsv_t* list )
		: list_( list ), it_( 0 )
	{
		xmmsv_get_list_iter( list_, &it_ );
	}

	template< typename T >
	List_const_iterator_< T >::List_const_iterator_()
		: list_( 0 ), it_( 0 )
	{
	}

	template< typename T >
	List_const_iterator_< T >::List_const_iterator_( const List_const_iterator_& rh )
		: list_( rh.list_ ), it_( 0 )
	{
		if( list_ ) {
			xmmsv_t* rh_parent( xmmsv_list_iter_get_parent( rh.it_ ) );
			xmmsv_get_list_iter( rh_parent, &it_ );
			xmmsv_list_iter_seek( it_, xmmsv_list_iter_tell( rh.it_ ) );
		}
	}

	template< typename T >
	List_const_iterator_< T >&
	List_const_iterator_< T >::operator=( const List_const_iterator_& rh )
	{
		list_ = rh.list_;
		if ( it_ ) {
			xmmsv_list_iter_explicit_destroy( it_ );
		}

		if( list_ ) {
			xmmsv_t* rh_parent( xmmsv_list_iter_get_parent( rh.it_ ) );
			xmmsv_get_list_iter( rh_parent, &it_ );
			xmmsv_list_iter_seek( it_, xmmsv_list_iter_tell( rh.it_ ) );
		}
		else {
			it_ = 0;
		}

		return *this;
	}

	template< typename T >
	List_const_iterator_< T >::~List_const_iterator_()
	{
		if ( it_ ) {
			xmmsv_list_iter_explicit_destroy( it_ );
		}
	}

	template< typename T >
	const typename List_const_iterator_< T >::value_type&
	List_const_iterator_<T>::operator*() const
	{
		static value_type value;
		value = construct< T >( getElement() );
		return value;
	}

	template< typename T >
	const typename List_const_iterator_< T >::value_type*
	List_const_iterator_<T>::operator->() const
	{
		return &( operator*() );
	}

	template< typename T >
	List_const_iterator_< T >&
	List_const_iterator_< T >::operator++()
	{
		xmmsv_list_iter_next( it_ );
		return *this;
	}

	template< typename T >
	List_const_iterator_< T >
	List_const_iterator_< T >::operator++( int )
	{
		List_const_iterator_ tmp( *this );
		++*this;
		return tmp;
	}

	template< typename T >
	List_const_iterator_< T >&
	List_const_iterator_< T >::operator--()
	{
		unsigned int pos( xmmsv_list_iter_tell( it_ ) );
		if ( pos != 0 ) {
			xmmsv_list_iter_seek( it_, pos - 1 );
		}
		return *this;
	}

	template< typename T >
	List_const_iterator_< T >
	List_const_iterator_< T >::operator--( int )
	{
		List_const_iterator_ tmp( *this );
		--*this;
		return tmp;
	}

	template< typename T >
	bool List_const_iterator_< T >::equal( const List_const_iterator_& rh ) const
	{
		if( !valid() && !rh.valid() ) {
			return true;
		}
		if( list_ == rh.list_ ) {
			return xmmsv_list_iter_tell( it_ ) == xmmsv_list_iter_tell( rh.it_ );
		}
		return false;
	}

	template< typename T >
	bool List_const_iterator_< T >::valid() const
	{
		return list_ && it_ && xmmsv_list_iter_valid( it_ );
	}
}

#endif // XMMSCLIENTPP_LIST_H
