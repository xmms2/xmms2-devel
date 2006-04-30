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

	/** @class Config config.h "xmmsclient/xmmsclient++/config.h"
	 *  @brief This class controls the configuration values in the server.
	 */
	class Config
	{

		public:

			/** Destructor.
			 */
			virtual ~Config();

			/** Registers a config value in the server.
			 *  
			 *  @param name should be @<clientname@>.myval
			 *              like cli.path or something like that.
			 *  @param defval The default value for this config value.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			void valueRegister( const std::string& name,
			                    const std::string& defval ) const;

			/** Sets a config value in the server.
			 *
			 *  @param key Key of the config value to set.
			 *  @param value Value for the config key.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			void valueSet( const std::string& key,
			               const std::string& value ) const;

			/** Gets the value of a config key.
			 *  
			 *  @param key Key of the config value to get.
			 *
			 *  @return String holding the value of the key.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			const std::string valueGet( const std::string& key ) const;

			/** Gets a key<->value list of config values from the server.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 *  
			 *  @return Dict containing all configuration keys and their values.
			 */
			Dict valueList() const;

			/** Registers a config value in the server.
			 *  
			 *  @param name should be @<clientname@>.myval
			 *              like cli.path or something like that.
			 *  @param defval The default value for this config value.
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			valueRegister( const std::string& name, const std::string& defval,
			               const VoidSlot& slot,
			               const ErrorSlot& error = &Xmms::dummy_error
			             ) const;

			/**
			 * @overload
			 * @note It takes a list of slots instead of just one slot.
			 */
			void
			valueRegister( const std::string& name, const std::string& defval,
			               const std::list< VoidSlot >& slots,
			               const ErrorSlot& error = &Xmms::dummy_error
			             ) const;

			/** Sets a config value in the server.
			 *
			 *  @param key Key of the config value to set.
			 *  @param value Value for the config key.
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			valueSet( const std::string& key, const std::string& value,
			          const VoidSlot& slot,
			          const ErrorSlot& error = &Xmms::dummy_error
			        ) const;

			/**
			 * @overload
			 * @note It takes a list of slots instead of just one slot.
			 */
			void
			valueSet( const std::string& key, const std::string& value,
			          const std::list< VoidSlot >& slots,
			          const ErrorSlot& error = &Xmms::dummy_error
			        ) const;

			/** Gets the value of a config key.
			 *  
			 *  @param key Key of the config value to get.
			 *  @param slot Function pointer to a function taking
			 *              const std::string& and returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			valueGet( const std::string& key,
			          const StringSlot& slot,
			          const ErrorSlot& error = &Xmms::dummy_error
			        ) const;

			/**
			 * @overload
			 * @note It takes a list of slots instead of just one slot.
			 */
			void
			valueGet( const std::string& key,
			          const std::list< StringSlot >& slots,
			          const ErrorSlot& error = &Xmms::dummy_error
			        ) const;

			/** Gets a key<->value list of config values from the server.
			 *  
			 *  @param slot Function pointer to a function taking
			 *              const Dict& and returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			valueList( const DictSlot& slot,
			           const ErrorSlot& error = &Xmms::dummy_error
			         ) const;

			/**
			 * @overload
			 * @note It takes a list of slots instead of just one slot.
			 */
			void
			valueList( const std::list< DictSlot >& slots,
			           const ErrorSlot& error = &Xmms::dummy_error
			         ) const;


			/** Requests the <i>config value changed</i> broadcast.
			 *
			 *  @param slot Function pointer to a function taking
			 *              const Dict& and returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			broadcastValueChanged( const DictSlot& slot,
			                       const ErrorSlot& error = &Xmms::dummy_error
			                     ) const;

			/**
			 * @overload
			 * @note It takes a list of slots instead of just one slot.
			 */
			void
			broadcastValueChanged( const std::list< DictSlot >& slots,
			                       const ErrorSlot& error = &Xmms::dummy_error
			                     ) const;

		/** @cond */
		private:

			// Constructor, only to be called by Xmms::Client
			friend class Client;
			Config( xmmsc_connection_t*& conn, bool& connected,
				    MainloopInterface*& ml );

			// Copy-constructor / operator=
			Config( const Config& src );
			Config operator=( const Config& src ) const;

			xmmsc_connection_t*& conn_;
			bool& connected_;
			MainloopInterface*& ml_;
		/** @endcond */

	};

}

#endif // XMMSCLIENTPP_CONFIG_H
