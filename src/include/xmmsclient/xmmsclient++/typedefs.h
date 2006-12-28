#ifndef XMMSCLIENTPP_TYPEDEFS_H
#define XMMSCLIENTPP_TYPEDEFS_H

#include <xmmsclient/xmmsclient.h>
#include <boost/signal.hpp>
#include <xmmsclient/xmmsclient++/signal.h>
#include <xmmsclient/xmmsclient++/list.h>
#include <xmmsclient/xmmsclient++/dict.h>

#include <string>

namespace Xmms 
{

	template< typename T >
	class List;

	typedef List< Dict > DictList;

	typedef std::basic_string< unsigned char > bin;

	/** Used for function pointers to functions with signature
	 *  bool();
	 */
	typedef Signal< void >::signal_t::slot_type VoidSlot;

	/** Used for function pointers to functions with signature
	 *  bool( const int& );
	 */
	typedef Signal< int >::signal_t::slot_type IntSlot;

	/** Used for function pointers to functions with signature
	 *  bool( const unsigned int& );
	 */
	typedef Signal< unsigned int >::signal_t::slot_type UintSlot;

	/** Used for function pointers to functions with signature
	 *  bool( const std::string& );
	 */
	typedef Signal< std::string >::signal_t::slot_type StringSlot;

	/** Used for function pointers to functions with signature
	 *  bool( const Xmms::bin& );
	 */
	typedef Signal< Xmms::bin >::signal_t::slot_type BinSlot;

	/** Used for function pointers to functions with signature
	 *  bool( const Xmms::Dict& );
	 */
	typedef Signal< Dict >::signal_t::slot_type DictSlot;

	/** Used for function pointers to functions with signature
	 *  bool( const Xmms::PropDict& );
	 */
	typedef Signal< PropDict >::signal_t::slot_type PropDictSlot;

	/** Used for function pointers to functions with signature
	 *  bool( const Xmms::Playback::Status& );
	 */
	typedef Signal< xmms_playback_status_t >::signal_t::slot_type StatusSlot;

	/** Used for function pointers to functions with signature
	 *  bool( const Xmms::Stats::ReaderStatus& );
	 */
	typedef Signal< xmms_mediainfo_reader_status_t >::signal_t::slot_type
	                                                        ReaderStatusSlot;


	/** Used for function pointers to functions with signature
	 *  bool( const Xmms::List< int >& );
	 */
	typedef Signal< List< int > >::signal_t::slot_type IntListSlot;

	/** Used for function pointers to functions with signature
	 *  bool( const Xmms::List< unsigned int >& );
	 */
	typedef Signal< List< unsigned int > >::signal_t::slot_type UintListSlot;

	/** Used for function pointers to functions with signature
	 *  bool( const Xmms::List< std::string >& );
	 */
	typedef Signal< List< std::string > >::signal_t::slot_type StringListSlot;

	/** Used for function pointers to functions with signature
	 *  bool( const Xmms::List< Xmms::Dict >& );
	 */
	typedef Signal< List< Dict > >::signal_t::slot_type DictListSlot;

	namespace Coll {
		class Coll;
	}
	typedef Signal< Coll::Coll >::signal_t::slot_type CollPtrSlot;


	/** Used for function pointers to functions with signature
	 *  bool( const std::string& );
	 */
	typedef Xmms::error_sig::slot_type ErrorSlot;

}

#endif // XMMSCLIENTPP_TYPEDEFS_H
