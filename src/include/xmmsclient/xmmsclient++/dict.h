#ifndef XMMSCLIENTPP_DICT_H
#define XMMSCLIENTPP_DICT_H

#include <xmmsclient/xmmsclient.h>
#include <boost/any.hpp>
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

			/** Gets the corresponding value of the key.
			 *
			 * @param key Key to look for
			 *
			 * @return boost::any containing the value.
			 *         Use boost::any_cast or .type() member function 
			 *         to figure out the actual type of the returned value.
			 *         @n The return value can be of types:
			 *         - @c std::string
			 *         - @c unsigned int
			 *         - @c int
			 *
			 * @throws no_such_key_error Occurs when key can't be found.
			 */
			virtual boost::any operator[]( const std::string& key ) const;
			
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

	};

}

#endif // XMMSCLIENTPP_DICT_H
