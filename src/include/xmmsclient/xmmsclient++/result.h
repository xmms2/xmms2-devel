#ifndef XMMSCLIENTPP_FOO_H
#define XMMSCLIENTPP_FOO_H

#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/dict.h>
#include <xmmsclient/xmmsclient++/mainloop.h>
#include <xmmsclient/xmmsclient++/helpers.h>
#include <string>
#include <iostream>

namespace Xmms
{

	template< typename T, typename A, int get( xmmsc_result_t*, A* ) >
	class Adapter
	{

		public:

			Adapter( xmmsc_result_t* res, MainloopInterface*& ml )
				: res_( res ), value_( 0 ), ml_( ml ), sig_( 0 )
			{
			}

			virtual ~Adapter()
			{
				xmmsc_result_unref( res_ );
			}

			operator T()
			{
				check( ml_ );
				xmmsc_result_wait( res_ );
				check( res_ );

				if( !get( res_, &value_ ) ) {
					// FIXME: Handle failure
				}
				return T( value_ );
			}

			void operator()()
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

			void operator()( boost::function< bool( const T& ) > slot )
			{
				connect( slot );
				(*this)();
			}

			void operator()( boost::function< bool( const T& ) > slot,
			                 boost::function< bool( const std::string& ) > error )
			{
				connect( slot );
				connectError( error );
				(*this)();
			}

			void connect( boost::function< bool( const T& ) > slot )
			{
				if( !sig_ ) {
					sig_ = new Signal< T >;
				}
				sig_->signal.connect( slot );
			}

			void connectError( boost::function< bool( const std::string& ) > error )
			{
				if( !sig_ ) {
					sig_ = new Signal< T >;
				}
				sig_->error_signal.connect( error );
			}

		private:

			xmmsc_result_t* res_;
			A value_;
			MainloopInterface*& ml_;
			Signal< T >* sig_;

	};

	template< typename T >
	class ClassAdapter
	{

		public:

			ClassAdapter( xmmsc_result_t* res, MainloopInterface*& ml )
				: res_( res ), ml_( ml )
			{
			}

			virtual ~ClassAdapter()
			{
				xmmsc_result_unref( res_ );
			}

			operator T()
			{
				check( ml_ );
				xmmsc_result_wait( res_ );
				return T( res_ );
			}

		private:

			xmmsc_result_t* res_;
			MainloopInterface*& ml_;

	};


	typedef Adapter< int32_t, int32_t, xmmsc_result_get_int > IntResult;
	typedef Adapter< uint32_t, uint32_t, xmmsc_result_get_uint > UintResult;
	typedef Adapter< std::string, char*, xmmsc_result_get_string > StringResult;
	typedef Adapter< xmms_playback_status_t, uint32_t,
	                 xmmsc_result_get_uint > StatusResult;
	typedef ClassAdapter< Dict > DictResult;
	typedef ClassAdapter< PropDict > PropDictResult;

	class VoidResult
	{

		public:

			VoidResult( xmmsc_result_t* res, MainloopInterface*& ml )
				: res_( res )
			{
				if( !(ml && ml->isRunning()) ) {
					xmmsc_result_wait( res );
					check( res );
				}
			}

			~VoidResult()
			{
				xmmsc_result_unref( res_ );
			}

			void operator()()
			{
				xmmsc_result_notifier_set( res_,
				                           Xmms::generic_callback< void >,
				                           0 );
			}

		private:

			xmmsc_result_t* res_;

	};

}

#endif
