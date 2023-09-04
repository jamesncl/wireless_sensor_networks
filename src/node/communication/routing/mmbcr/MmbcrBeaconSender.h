#ifndef _MMBCRBEACONSENDER_H_
#define _MMBCRBEACONSENDER_H_

#include <algorithm>
#include "CastaliaModule.h"
#include "TimerService.h"
#include "ResourceManager.h"
#include "Supercapacitor.h"
#include "MmbcrBeaconSenderControlMessage_m.h"
#include "RoutingControlMessage_m.h"
#include "MmbcrPacket_m.h"

enum mmbcrBeaconSenderTimers 
{
	MMBCR_BEACON_SENDER_TIMER_SEND_NEXT_BEACON = 1
};

class MmbcrBeaconSender : public CastaliaModule, public TimerService
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
		bool isSink;
		BatteryCapacityVector_t currentParentBatteryCapacitiesOfPathToSink;
		BatteryCapacityVector_t thisNodesBatteryCapacitiesOfPathToSink;
		Supercapacitor *supercapacitor;
		
		// Private member functions:
		void resetTrickle();
		void sendNextBeacon();
		void calculateSendingInterval();
		void advanceNextTrickleStep();
		void updateThisNodesBatteryCapacitiesOfPathToSink();

	protected:
		
		// Virtual methods from CastaliaModule / TimerService class
		void initialize();
		void finishSpecific();
		void handleMessage(cMessage *msg);
		void timerFiredCallback(int index);
};

#endif //_MMBCRBEACONSENDER_H_