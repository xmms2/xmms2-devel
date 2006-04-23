#ifndef XMMSCLIENTPP_TYPEDEFS_H
#define XMMSCLIENTPP_TYPEDEFS_H

#include <vector>
#include <map>
#include <string>
#include <boost/any.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/signal.hpp>
#include <xmmsclient/xmmsclient++/list.h>
#include <xmmsclient/xmmsclient++/dict.h>
#include <xmmsclient/xmmsclient++/signal.h>
#include <xmmsclient/xmmsclient.h>

namespace Xmms 
{

	template< typename T >
	class List;

	typedef List< Dict > DictList;

	typedef boost::shared_ptr< Dict > DictPtr;
	typedef boost::shared_ptr< DictList > DictListPtr;

	typedef Signal< void >::signal_t::slot_type VoidSlot;
	typedef Signal< int >::signal_t::slot_type IntSlot;
	typedef Signal< unsigned int >::signal_t::slot_type UintSlot;
	typedef Signal< std::string >::signal_t::slot_type StringSlot;

	typedef Signal< Dict >::signal_t::slot_type DictSlot;
	typedef Signal< PropDict >::signal_t::slot_type PropDictSlot;
	typedef Signal< xmms_playback_status_t >::signal_t::slot_type StatusSlot;

	typedef Signal< List< int > >::signal_t::slot_type IntListSlot;
	typedef Signal< List< unsigned int > >::signal_t::slot_type UintListSlot;
	typedef Signal< List< std::string > >::signal_t::slot_type StringListSlot;
	typedef Signal< List< Dict > >::signal_t::slot_type DictListSlot;

	typedef Xmms::error_sig::slot_type ErrorSlot;

}

#endif // XMMSCLIENTPP_TYPEDEFS_H
