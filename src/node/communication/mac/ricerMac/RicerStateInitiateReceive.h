#ifndef _RICERSTATEINITIATERECEIVE_H_
#define _RICERSTATEINITIATERECEIVE_H_

#include "RicerState.h"

class RicerStateInitiateReceive : public RicerState
{
	public:
		std::string getCurrentStateName();
		void start(RicerStateContextInterface *context, RicerMacInterface *moduleInterface);
		void fromRadioLayer(RicerStateContextInterface *context, RicerMacInterface *moduleInterface, RicerMacPacket *packet);
		void packetFromNetLayerHasBeenBuffered(RicerStateContextInterface *context, RicerMacInterface *moduleInterface);
		void timerFired(RicerStateContextInterface *context, RicerMacInterface *moduleInterface, RicerMacTimer timer);
	
	private:
		void checkCca(RicerStateContextInterface *context, RicerMacInterface *moduleInterface);
};

#endif //_RICERSTATEINITIATERECEIVE_H_