#ifndef _RICERSTATEWAITTOSEND_H_
#define _RICERSTATEWAITTOSEND_H_

#include "RicerState.h"

class RicerStateWaitToSend : public RicerState
{
	private:
		// Note: states should not normally hold state information. However making an exception for this
		// variable: only used when in wait-to-send state - set on entering state, used and unset on exiting
		double waitToSendStartedAt;

		void stopSendingAndGoToSleep(RicerStateContextInterface *context, RicerMacInterface *moduleInterface);
		void receivedBeaconFrom(RicerStateContextInterface *context, RicerMacInterface *moduleInterface, int beaconFromNode);
		void recordWaitingTime(RicerStateContextInterface *context, RicerMacInterface *moduleInterface);

	public:
		std::string getCurrentStateName();
		void start(RicerStateContextInterface *context, RicerMacInterface *moduleInterface);
		void fromRadioLayer(RicerStateContextInterface *context, RicerMacInterface *moduleInterface, RicerMacPacket *packet);
		void packetFromNetLayerHasBeenBuffered(RicerStateContextInterface *context, RicerMacInterface *moduleInterface);
		void timerFired(RicerStateContextInterface *context, RicerMacInterface *moduleInterface, RicerMacTimer timer);
};


#endif //_RICERSTATEWAITTOSEND_H_