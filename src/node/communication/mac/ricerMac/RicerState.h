#ifndef _RICERSTATE_H_
#define _RICERSTATE_H_

#include <string>
#include <cstring>
#include <stdexcept>
#include "Radio.h"
#include "RadioControlMessage_m.h"
#include "RicerMacTimers.h"
#include "RicerStateContextInterface.h"
#include "RicerMacPacket_m.h"
#include "RicerMacInterface.h"

class RicerState // ABSTRACT base class for concrete state sub-classes
{
	public: // Public because we want to be able to mock these functions
		// Use of '= 0' indicates a 'pure' virtual function - it MUST be implemented
		// by subclasses

		virtual std::string getCurrentStateName() = 0;
		virtual void start(RicerStateContextInterface *context, RicerMacInterface *moduleInterface) = 0;
		virtual void fromRadioLayer(RicerStateContextInterface *context, RicerMacInterface *moduleInterface, RicerMacPacket *packet) = 0;
		virtual void packetFromNetLayerHasBeenBuffered(RicerStateContextInterface *context, RicerMacInterface *moduleInterface) = 0;
		virtual void timerFired(RicerStateContextInterface *context, RicerMacInterface *moduleInterface, RicerMacTimer timer) = 0;
};

#endif //_RICERSTATE_H_