#ifndef _RICERSTATESLEEP_H_
#define _RICERSTATESLEEP_H_

#include "RicerState.h"

class RicerStateSleep : public RicerState
{
	private:
		// Note: states should not normally hold state information. However making an exception for this
		// variable: only used when in sleep state - set on entering sleep state, used and unset on exiting
		double sleepStartedAt;

		void recordSleepTime(RicerStateContextInterface *context, RicerMacInterface *moduleInterface);

	public:
		std::string getCurrentStateName();
		void start(RicerStateContextInterface *context, RicerMacInterface *moduleInterface);
		void fromRadioLayer(RicerStateContextInterface *context, RicerMacInterface *moduleInterface, RicerMacPacket *packet);
		void packetFromNetLayerHasBeenBuffered(RicerStateContextInterface *context, RicerMacInterface *moduleInterface);
		void timerFired(RicerStateContextInterface *context, RicerMacInterface *moduleInterface, RicerMacTimer timer);
};

#endif //_RICERSTATESLEEP_H_