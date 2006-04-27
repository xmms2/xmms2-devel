#ifndef XMMSCLIENTPP_CONFIG_H
#define XMMSCLIENTPP_CONFIG_H

#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/mainloop.h>
#include <xmmsclient/xmmsclient++/signal.h>
#include <xmmsclient/xmmsclient++/helpers.h>
#include <xmmsclient/xmmsclient++/typedefs.h>

#include <list>
#include <string>

namespace Xmms 
{

	class Client;
	class Config
	{

		public:

			// Destructor
			virtual ~Config();

			// Commands
			void valueRegister( const std::string& name,
			                    const std::string& defval ) const;

			void valueSet( const std::string& key,
			               const std::string& value ) const;

			const std::string valueGet( const std::string& key ) const;

			Dict valueList() const;


			void
			valueRegister( const std::string& name, const std::string& defval,
			               const VoidSlot& slot,
			               const ErrorSlot& error = &Xmms::dummy_error
			             ) const;

			void
			valueRegister( const std::string& name, const std::string& defval,
			               const std::list< VoidSlot >& slots,
			               const ErrorSlot& error = &Xmms::dummy_error
			             ) const;

			void
			valueSet( const std::string& key, const std::string& value,
			          const VoidSlot& slot,
			          const ErrorSlot& error = &Xmms::dummy_error
			        ) const;

			void
			valueSet( const std::string& key, const std::string& value,
			          const std::list< VoidSlot >& slots,
			          const ErrorSlot& error = &Xmms::dummy_error
			        ) const;

			void
			valueGet( const std::string& key,
			          const StringSlot& slot,
			          const ErrorSlot& error = &Xmms::dummy_error
			        ) const;

			void
			valueGet( const std::string& key,
			          const std::list< StringSlot >& slots,
			          const ErrorSlot& error = &Xmms::dummy_error
			        ) const;

			void
			valueList( const DictSlot& slot,
			           const ErrorSlot& error = &Xmms::dummy_error
			         ) const;

			void
			valueList( const std::list< DictSlot >& slots,
			           const ErrorSlot& error = &Xmms::dummy_error
			         ) const;


			void
			broadcastValueChanged( const DictSlot& slot,
			                       const ErrorSlot& error = &Xmms::dummy_error
			                     ) const;

			void
			broadcastValueChanged( const std::list< DictSlot >& slots,
			                       const ErrorSlot& error = &Xmms::dummy_error
			                     ) const;

		private:

			// Constructor, only to be called by Xmms::Client
			friend class Client;
			Config( xmmsc_connection_t*& conn, bool& connected,
				    MainLoop*& ml );

			// Copy-constructor / operator=
			Config( const Config& src );
			Config operator=( const Config& src ) const;

			xmmsc_connection_t*& conn_;
			bool& connected_;
			MainLoop*& ml_;

	};

}

#endif // XMMSCLIENTPP_CONFIG_H
