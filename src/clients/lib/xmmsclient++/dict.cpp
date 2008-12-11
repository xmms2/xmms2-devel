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

#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/dict.h>
#include <xmmsclient/xmmsclient++/helpers.h>
#include <xmmsclient/xmmsclient++/exceptions.h>
#include <boost/variant.hpp>
#include <string>
#include <list>
#include <vector>
#include <iostream>

namespace Xmms
{
	static void getValue( Dict::Variant& val, xmmsv_t *value );

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
			
			case XMMSV_TYPE_UINT32: {

				uint32_t temp = 0;
				if( !xmmsv_get_uint( value, &temp ) ) {
					// FIXME: handle error
				}
				val = temp;
				break;

			}
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

}

