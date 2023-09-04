#ifndef _RICERSTATESEND_H_
#define _RICERSTATESEND_H_

#include "RicerState.h"

class RicerStateSend : public RicerState
{
	private:
		void sendFinishedGoToWaitToSend(RicerStateContextInterface *context, RicerMacInterface *moduleInterface);
		void backoffEndedCheckCca(RicerStateContextInterface *context, RicerMacInterface *moduleInterface);
		//void stopSendingAndGoToSleep(RicerStateContextInterface *context, RicerMacInterface *moduleInterface);

	public:
		std::string getCurrentStateName();
		void start(RicerStateContextInterface *context, RicerMacInterface *moduleInterface);
		void fromRadioLayer(RicerStateContextInterface *context, RicerMacInterface *moduleInterface, RicerMacPacket *packet);
		void packetFromNetLayerHasBeenBuffered(RicerStateContextInterface *context, RicerMacInterface *moduleInterface);
		void timerFired(RicerStateContextInterface *context, RicerMacInterface *moduleInterface, RicerMacTimer timer);
};


#endif //_RICERSTATESEND_H_