#include <xmmsclient/xmmsclient++/signal.h>

namespace Xmms
{

	SignalHolder& SignalHolder::getInstance()
	{
		static SignalHolder instance;
		return instance;
	}

	void SignalHolder::addSignal( SignalInterface* sig )
	{
		signals_.push_back( sig );
	}

	void SignalHolder::removeSignal( SignalInterface* sig )
	{
		signals_.remove( sig );
		delete sig;
	}

	SignalHolder::~SignalHolder()
	{
		std::list< SignalInterface* >::iterator i;
		for( i = signals_.begin(); i != signals_.end(); ++i )
		{
			delete *i; *i = 0;
		}
	}

}
