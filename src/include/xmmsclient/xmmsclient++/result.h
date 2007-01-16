#ifndef XMMSCLIENTPP_RESULT_H
#define XMMSCLIENTPP_RESULT_H

#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/dict.h>
#include <xmmsclient/xmmsclient++/list.h>
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
				xmmsc_result_notifier_set( res_, Xmms::generic_callback< T >,
				                           static_cast< void* >( sig_ ) );

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

	template< typename T, typename A, int get( xmmsc_result_t*, A* ) >
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

				if( !get( this->res_, &value_ ) ) {
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
				return T( this->res_ );
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

			~SignalAdapter()
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

	typedef Adapter< int32_t, int32_t, xmmsc_result_get_int > IntResult;
	typedef Adapter< uint32_t, uint32_t, xmmsc_result_get_uint > UintResult;
	typedef Adapter< std::string, char*, xmmsc_result_get_string > StringResult;
	typedef Adapter< xmms_playback_status_t, uint32_t,
	                 xmmsc_result_get_uint > StatusResult;

	typedef ClassAdapter< Dict > DictResult;
	typedef ClassAdapter< PropDict > PropDictResult;
	typedef ClassAdapter< List< int > > IntListResult;
	typedef ClassAdapter< List< unsigned int > > UintListResult;
	typedef ClassAdapter< List< std::string > > StringListResult;
	typedef ClassAdapter< List< Dict > > DictListResult;

	typedef SignalAdapter< Dict > DictSignal;
	typedef SignalAdapter< unsigned int > UintSignal;
	typedef SignalAdapter< std::string > StringSignal;

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

				unsigned char* temp = 0;
				unsigned int len = 0;
				if( !xmmsc_result_get_bin( this->res_, &temp, &len ) ) {
					// FIXME: Handle failure
				}
				return Xmms::bin( temp, len );
			}

	};

}

#endif
