#ifndef _CTPROUTINGBEACONSENDER_H_
#define _CTPROUTINGBEACONSENDER_H_

#include <algorithm>
#include "CastaliaModule.h"
#include "TimerService.h"
#include "ResourceManager.h"
#include "BeaconSenderControlMessage_m.h"
#include "CtpRoutingPacket_m.h"

enum beaconSenderTimers {
	BEACON_SENDER_TIMER_SEND_NEXT_BEACON = 1
};

class CtpRoutingBeaconSender : public CastaliaModule, public TimerService
{
	private:
		// NED file parameters:
		double trickleFrequencyCoefficientMax;
		double trickleFrequencyCoefficientMin;
		int beaconFrameSizeBits;

		// Other private variables:
		double trickleFrequencyCoefficientCurrent;
		double trickleSendingIntervalCurrent;
		bool staticFrequency;
		int selfNodeId;
		int currentBeaconSequenceNumber;
		double currentMultihopEtxToRoot;
		int currentParentNodeId;
		bool setPullFlag;
		
		// Private member functions:
		void resetTrickle();
		void sendNextBeacon();
		void calculateSendingInterval();
		void advanceNextTrickleStep();

	protected:
		
		// Virtual methods from CastaliaModule / TimerService class
		void initialize();
		void finishSpecific();
		void handleMessage(cMessage *msg);
		void timerFiredCallback(int index);
};

#endif //_CTPROUTINGBEACONSENDER_H_