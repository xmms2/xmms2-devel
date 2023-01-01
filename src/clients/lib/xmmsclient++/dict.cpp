/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2023 XMMS2 Team
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

#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/dict.h>
#include <xmmsclient/xmmsclient++/helpers.h>
#include <xmmsclient/xmmsclient++/exceptions.h>
#include <boost/variant.hpp>
#include <string>
#include <list>
#include <vector>
#include <iostream>
#include <cstring>

namespace Xmms
{
	static void getValue( Dict::Variant& val, xmmsv_t *value );

	Dict::Dict() : value_( xmmsv_new_dict() )
	{
	}

	Dict::Dict( xmmsv_t* val ) : value_( 0 )
	{
		if( xmmsv_is_error( val ) ) {
			const char *buf;
			xmmsv_get_error( val, &buf );
			throw value_error( buf );
		}
		else if( xmmsv_get_type( val ) != XMMSV_TYPE_DICT ) {
			throw not_dict_error( "Value is not a dict" );
		}
		setValue( val );
	}

	Dict::Dict( const Dict& dict ) : value_( dict.value_ )
	{
		xmmsv_ref( value_ );
	}

	Dict& Dict::operator=( const Dict& dict )
	{
		setValue( dict.value_ );
		return *this;
	}

	Dict::~Dict()
	{
		xmmsv_unref( value_ );
	}

	void Dict::setValue( xmmsv_t *newval )
	{
		if( value_ ) {
			xmmsv_unref( value_ );
		}
		value_ = newval;
		xmmsv_ref( value_ );
	}

	bool Dict::contains( const std::string& key ) const
	{
		return !!xmmsv_dict_get( value_, key.c_str(), NULL );
	}

	Dict::const_iterator Dict::find( const std::string& key ) const
	{
		const_iterator it( value_ );

		if( xmmsv_dict_iter_find( it.it_, key.c_str() ) ) {
			return it;
		}
		else {
			return end();
		}
	}

	Dict::Variant Dict::operator[]( const std::string& key ) const
	{
		Dict::Variant value;

		xmmsv_t *elem;
		if( !xmmsv_dict_get( value_, key.c_str(), &elem ) ) {
			throw no_such_key_error( "No such key: " + key );
		}

		getValue( value, elem );

		return value;

	}

	static void
	getValue( Dict::Variant& val, xmmsv_t *value )
	{
		switch( xmmsv_get_type( value ) ) {
			
			case XMMSV_TYPE_INT32: {

				int32_t temp = 0;
				if( !xmmsv_get_int( value, &temp ) ) {
					// FIXME: handle error
				}
				val = temp;
				break;

			}
			case XMMSV_TYPE_STRING: {

				const char* temp = 0;
				if( !xmmsv_get_string( value, &temp ) ) {
					// FIXME: handle error
				}
				val = std::string( temp );
				break;

			}
			case XMMSV_TYPE_NONE: {
				break;
			}
			default: {
			}

		}
	}

	static void
	dict_foreach( const char* key, xmmsv_t *value, void* userdata )
	{
		Xmms::Dict::ForEachFunc* func =
			static_cast< Xmms::Dict::ForEachFunc* >( userdata );
		Xmms::Dict::Variant val;
		std::string keystring( static_cast< const char* >( key ) );
		getValue( val, value );
		(*func)( key, val );

	}

	void Dict::each( ForEachFunc func ) const
	{
		xmmsv_dict_foreach( value_, &dict_foreach,
		                    static_cast< void* >( &func ) );
	}


	PropDict::PropDict( xmmsv_t* val ) : Dict( val ), propdict_( val )
	{
		xmmsv_ref( propdict_ );

		// Immediately replace with a "flat" dict (default sources)
		xmmsv_t *flat = xmmsv_propdict_to_dict( propdict_, NULL );
		setValue( flat );
		// setValue refs flat, unref here to get back to refcount of 1
		xmmsv_unref ( flat );
	}

	PropDict::PropDict( const PropDict& dict )
		: Dict( dict ), propdict_( dict.propdict_ )
	{
		xmmsv_ref( propdict_ );
	}

	PropDict& PropDict::operator=( const PropDict& dict )
	{
		Dict::operator=( dict );
		if( propdict_ ) {
			xmmsv_unref( propdict_ );
		}
		propdict_ = dict.propdict_;
		xmmsv_ref( propdict_ );
		return *this;
	}

	PropDict::~PropDict()
	{
		xmmsv_unref( propdict_ );
	}

	void PropDict::setSource( const std::string& src )
	{
		std::list< std::string > sources;
		sources.push_back( src );
		setSource( sources );
	}

	void PropDict::setSource( const std::list< std::string >& src )
	{
		std::vector< const char* > prefs;
		fillCharArray( src, prefs );

		xmmsv_t *flat = xmmsv_propdict_to_dict( propdict_, &prefs[0] );
		setValue( flat );
		// setValue refs flat, unref here to get back to refcount of 1
		xmmsv_unref ( flat );
	}


	static void
	propdict_foreach_inner( const char* source, xmmsv_t *value, void* userdata )
	{
		Xmms::PropDict::ForEachData *fedata =
			static_cast< Xmms::PropDict::ForEachData* >( userdata );

		Dict::Variant val;
		getValue( val, value );

		fedata->run( source, val );
	}

	static void
	propdict_foreach( const char* key, xmmsv_t *pair, void* userdata )
	{
		Xmms::PropDict::ForEachFunc* func =
			static_cast< Xmms::PropDict::ForEachFunc* >( userdata );

		Xmms::PropDict::ForEachData fedata( key, func );

		xmmsv_dict_foreach( pair, &propdict_foreach_inner,
		                    static_cast< void* >( &fedata ) );
	}

	void PropDict::each( PropDict::ForEachFunc func ) const
	{
		xmmsv_dict_foreach( propdict_, &propdict_foreach,
		                    static_cast< void* >( &func ) );
	}

	Dict::const_iterator
	Dict::begin() const
	{
		return const_iterator( value_ );
	}

	Dict::const_iterator
	Dict::end() const
	{
		return const_iterator();
	}

	Dict::const_iterator::const_iterator()
		: dict_( 0 ), it_( 0 )
	{
	}

	Dict::const_iterator::const_iterator( xmmsv_t* dict )
		: dict_( dict ), it_( 0 )
	{
		xmmsv_get_dict_iter( dict_, &it_ );
	}

	Dict::const_iterator::const_iterator( const const_iterator& rh )
		: dict_( rh.dict_ ), it_( 0 )
	{
		if ( dict_ ) {
			copy( rh );
		}
	}

	Dict::const_iterator& Dict::const_iterator::operator=( const const_iterator& rh )
	{
		dict_ = rh.dict_;
		if( it_ ) {
			xmmsv_dict_iter_explicit_destroy( it_ );
		}

		if ( dict_ ) {
			copy( rh );
		}
		else {
			it_ = 0;
		}

		return *this;
	}

	const Dict::const_iterator::value_type&
	Dict::const_iterator::operator*() const
	{
		static value_type value;
		const char* key;
		xmmsv_t* val;

		xmmsv_dict_iter_pair( it_, &key, &val );

		Dict::Variant var;
		getValue( var, val );
		value = value_type( key, var );
		return value;
	}

	const Dict::const_iterator::value_type*
	Dict::const_iterator::operator->() const
	{
		return &( operator*() );
	}

	Dict::const_iterator&
	Dict::const_iterator::operator++()
	{
		xmmsv_dict_iter_next( it_ );
		return *this;
	}

	Dict::const_iterator
	Dict::const_iterator::operator++( int )
	{
		const_iterator tmp( *this );
		++*this;
		return tmp;
	}

	bool
	Dict::const_iterator::valid() const
	{
		return dict_ && it_ && xmmsv_dict_iter_valid( it_ );
	}

	void
	Dict::const_iterator::copy( const const_iterator& rh )
	{
		const char* key = 0;
		xmmsv_get_dict_iter( dict_, &it_ );
		xmmsv_dict_iter_pair( rh.it_, &key, NULL );
		xmmsv_dict_iter_find( it_, key );
	}

	bool Dict::const_iterator::equal( const const_iterator& rh ) const
	{
		// _equal returns false if it_'s == 0
		if ( !valid() && !rh.valid() ) {
			return true;
		}
		if ( dict_ == rh.dict_ ) {
			const char *rh_key, *key;
			xmmsv_dict_iter_pair( rh.it_, &rh_key, NULL );
			xmmsv_dict_iter_pair( it_, &key, NULL );
			return (std::strcmp( key, rh_key ) == 0);
		}
		return false;
	}
}
