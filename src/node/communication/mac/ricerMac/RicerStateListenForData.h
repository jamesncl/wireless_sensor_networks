#ifndef _RICERSTATELISTENFORDATA_H_
#define _RICERSTATELISTENFORDATA_H_

#include "RicerState.h"

class RicerStateListenForData : public RicerState
{
	public:
		std::string getCurrentStateName();
		void start(RicerStateContextInterface *context, RicerMacInterface *moduleInterface);
		void fromRadioLayer(RicerStateContextInterface *context, RicerMacInterface *moduleInterface, RicerMacPacket *packet);
		void packetFromNetLayerHasBeenBuffered(RicerStateContextInterface *context, RicerMacInterface *moduleInterface);
		void timerFired(RicerStateContextInterface *context, RicerMacInterface *moduleInterface, RicerMacTimer timer);

	private:
		void exitStateToWaitToSend(RicerStateContextInterface *context, RicerMacInterface *moduleInterface);
};


#endif //_RICERSTATELISTENFORDATA_H_