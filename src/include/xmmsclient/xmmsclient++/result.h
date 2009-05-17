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

#ifndef XMMSCLIENTPP_RESULT_H
#define XMMSCLIENTPP_RESULT_H

#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/dict.h>
#include <xmmsclient/xmmsclient++/list.h>
#include <xmmsclient/xmmsclient++/coll.h>
#include <xmmsclient/xmmsclient++/mainloop.h>
#include <xmmsclient/xmmsclient++/helpers.h>
#include <string>
#include <iostream>

namespace Xmms
{

	template< typename T >
	class AdapterBase
	{
		public:

			AdapterBase( xmmsc_result_t* res, MainloopInterface*& ml )
				: res_( res ), ml_( ml ), sig_( 0 )
			{
			}

			virtual ~AdapterBase()
			{
				xmmsc_result_unref( res_ );
			}

			AdapterBase( const AdapterBase& src )
				: res_( src.res_ ), ml_( src.ml_ )
			{
				xmmsc_result_ref( res_ );

				// Does not delete the signals if it's been added to the
				// SignalHolder because operator() sets the sig_ to 0.
				delete sig_;
			}

			AdapterBase&
			operator=( const AdapterBase& src )
			{
				xmmsc_result_unref( res_ );
				res_ = src.res_;
				ml_ = src.ml_;
				xmmsc_result_ref( res_ );
				return *this;
			}

			virtual void
			operator()()
			{
				if( sig_ ) {
					SignalHolder::getInstance().addSignal( sig_ );
				}
				// sig_ will be removed from the SignalHolder (and freed)
				// by freeSignal when the result is freed.
				xmmsc_result_notifier_set_full( res_,
				                                Xmms::generic_callback< T >,
				                                static_cast< void* >( sig_ ),
				                                &freeSignal );

				// NOTE: This is intentional, the SignalHolder will destroy
				// the Signal object when it's no longer needed.
				sig_ = 0;
			}

			virtual void
			operator()( typename Signal<T>::signal_t::slot_type slot )
			{
				connect( slot );
				(*this)();
			}

			virtual void
			operator()( typename Signal<T>::signal_t::slot_type slot,
			            boost::function< bool( const std::string& ) > error )
			{
				connect( slot );
				connectError( error );
				(*this)();
			}

			virtual void
			connect( typename Signal<T>::signal_t::slot_type slot )
			{
				if( !sig_ ) {
					sig_ = new Signal< T >;
				}
				sig_->signal.connect( slot );
			}

			virtual void
			connectError( boost::function< bool( const std::string& ) > error )
			{
				if( !sig_ ) {
					sig_ = new Signal< T >;
				}
				sig_->error_signal.connect( error );
			}

		protected:

			xmmsc_result_t* res_;
			MainloopInterface*& ml_;
			Signal< T >* sig_;

	};

	template< typename T, typename A, int get( const xmmsv_t*, A* ) >
	class Adapter : public AdapterBase< T >
	{

		public:

			Adapter( xmmsc_result_t* res, MainloopInterface*& ml )
				: AdapterBase<T>( res, ml ), value_( 0 )
			{
			}

			virtual ~Adapter()
			{
			}

			Adapter( const Adapter& src )
				: AdapterBase<T>( src ), value_( src.value_ )
			{
			}

			Adapter&
			operator=( const Adapter& src )
			{
				AdapterBase<T>::operator=( src );
				value_ = src.value_;
				return *this;
			}

			operator T()
			{
				check( this->ml_ );
				xmmsc_result_wait( this->res_ );
				check( this->res_ );

				xmmsv_t *v = xmmsc_result_get_value( this->res_ );
				if( !get( v, &value_ ) ) {
					// FIXME: Handle failure
				}
				return T( value_ );
			}

		private:

			A value_;

	};

	template< typename T >
	class ClassAdapter : public AdapterBase< T >
	{

		public:

			ClassAdapter( xmmsc_result_t* res, MainloopInterface*& ml )
				: AdapterBase<T>( res, ml )
			{
			}

			~ClassAdapter()
			{
			}

			ClassAdapter( const ClassAdapter& src )
				: AdapterBase<T>( src )
			{
			}

			ClassAdapter&
			operator=( const ClassAdapter& src )
			{
				AdapterBase<T>::operator=( src );
				return *this;
			}

			operator T()
			{
				check( this->ml_ );
				xmmsc_result_wait( this->res_ );
				check( this->res_ );

				xmmsv_t *v = xmmsc_result_get_value( this->res_ );
				return T( v );
			}

	};

	template< typename T >
	class SignalAdapter : public AdapterBase< T >
	{
		
		public:
			SignalAdapter( xmmsc_result_t* res, MainloopInterface*& ml )
				: AdapterBase<T>( res, ml )
			{
			}

			virtual ~SignalAdapter()
			{
			}

			SignalAdapter( const SignalAdapter& src )
				: AdapterBase<T>( src )
			{
			}

			SignalAdapter&
			operator=( const SignalAdapter& src )
			{
				AdapterBase<T>::operator=( src );
				return *this;
			}

	};

	class QuitSignal : protected SignalAdapter< int32_t >
	{
		public:

			QuitSignal( xmmsc_result_t* res, MainloopInterface*& ml )
				: SignalAdapter< int32_t >( res, ml )
			{
				xmmsc_result_notifier_set_full( res_,
				                                Xmms::generic_callback<int32_t>,
				                                static_cast< void* >( sig_ ),
				                                &freeSignal );
			}

			using SignalAdapter< int32_t >::connect;
			using SignalAdapter< int32_t >::connectError;

	};

	typedef Adapter< int32_t, int32_t, xmmsv_get_int > IntResult;
	typedef Adapter< std::string, const char*, xmmsv_get_string > StringResult;
	typedef Adapter< xmms_playback_status_t, int32_t,
	                 xmmsv_get_int > StatusResult;

	typedef ClassAdapter< Dict > DictResult;
	typedef ClassAdapter< PropDict > PropDictResult;
	typedef ClassAdapter< List< int > > IntListResult;
	typedef ClassAdapter< List< std::string > > StringListResult;
	typedef ClassAdapter< List< Dict > > DictListResult;

	typedef SignalAdapter< Dict > DictSignal;
	typedef SignalAdapter< int > IntSignal;
	typedef SignalAdapter< std::string > StringSignal;
	typedef SignalAdapter< xmms_playback_status_t > StatusSignal;
	typedef SignalAdapter< xmms_mediainfo_reader_status_t > ReaderStatusSignal;

	class VoidResult : public AdapterBase< void >
	{

		public:

			VoidResult( xmmsc_result_t* res, MainloopInterface*& ml )
				: AdapterBase<void>( res, ml )
			{
				if( !(ml && ml->isRunning()) ) {
					xmmsc_result_wait( res );
					check( res );
				}
			}

			~VoidResult()
			{
			}

			VoidResult( const VoidResult& src )
				: AdapterBase<void>( src )
			{
			}

			VoidResult&
			operator=( const VoidResult& src )
			{
				AdapterBase<void>::operator=( src );
				return *this;
			}

	};

	class BinResult : public AdapterBase< bin >
	{

		public:

			BinResult( xmmsc_result_t* res, MainloopInterface*& ml )
				: AdapterBase< bin >( res, ml )
			{
			}

			~BinResult()
			{
			}

			BinResult( const BinResult& src )
				: AdapterBase< bin >( src )
			{
			}

			BinResult&
			operator=( const BinResult& src )
			{
				AdapterBase<bin>::operator=( src );
				return *this;
			}

			operator bin()
			{
				check( this->ml_ );
				xmmsc_result_wait( this->res_ );
				check( this->res_ );

				const unsigned char* temp = 0;
				unsigned int len = 0;
				xmmsv_t *v = xmmsc_result_get_value( this->res_ );
				if( !xmmsv_get_bin( v, &temp, &len ) ) {
					// FIXME: Handle failure
				}
				return Xmms::bin( temp, len );
			}

	};

	class CollResult : public AdapterBase< Coll::Coll >
	{

		public:
			CollResult( xmmsc_result_t* res, MainloopInterface*& ml )
				: AdapterBase< Coll::Coll >( res, ml )
			{
			}

			~CollResult()
			{
			}

			CollResult( const CollResult& src )
				: AdapterBase<Coll::Coll>( src )
			{
			}

			CollResult&
			operator=( const CollResult& src )
			{
				AdapterBase<Coll::Coll>::operator=( src );
				return *this;
			}

			operator CollPtr()
			{
				check( this->ml_ );
				xmmsc_result_wait( this->res_ );
				check( this->res_ );

				xmmsv_coll_t* coll = 0;
				xmmsv_t *v = xmmsc_result_get_value( this->res_ );
				if( !xmmsv_get_coll( v, &coll ) ) {
					throw value_error( "Invalid collection in value" );
				}
				return createColl( coll );
			}

			static CollPtr
			createColl( xmmsv_coll_t* coll )
			{

				CollPtr collptr;

				switch( xmmsv_coll_get_type( coll ) ) {

					case XMMS_COLLECTION_TYPE_REFERENCE: {
						collptr.reset( new Coll::Reference( coll ) );
						break;
					}
					case XMMS_COLLECTION_TYPE_UNION: {
						collptr.reset( new Coll::Union( coll ) );
						break;
					}
					case XMMS_COLLECTION_TYPE_INTERSECTION: {
						collptr.reset( new Coll::Intersection( coll ) );
						break;
					}
					case XMMS_COLLECTION_TYPE_COMPLEMENT: {
						collptr.reset( new Coll::Complement( coll ) );
						break;
					}
					case XMMS_COLLECTION_TYPE_HAS: {
						collptr.reset( new Coll::Has( coll ) );
						break;
					}
					case XMMS_COLLECTION_TYPE_SMALLER: {
						collptr.reset( new Coll::Smaller( coll ) );
						break;
					}
					case XMMS_COLLECTION_TYPE_GREATER: {
						collptr.reset( new Coll::Greater( coll ) );
						break;
					}
					case XMMS_COLLECTION_TYPE_EQUALS: {
						collptr.reset( new Coll::Equals( coll ) );
						break;
					}
					case XMMS_COLLECTION_TYPE_MATCH: {
						collptr.reset( new Coll::Match( coll ) );
						break;
					}
					case XMMS_COLLECTION_TYPE_IDLIST: {
						collptr.reset( new Coll::Idlist( coll ) );
						break;
					}
					case XMMS_COLLECTION_TYPE_QUEUE: {
						collptr.reset( new Coll::Queue( coll ) );
						break;
					}
					case XMMS_COLLECTION_TYPE_PARTYSHUFFLE: {
						collptr.reset( new Coll::PartyShuffle( coll ) );
						break;
					}

				}

				return collptr;

			}

	};

}

#endif
