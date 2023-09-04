#ifndef _CTPROUTINGCONTROLLER_H_
#define _CTPROUTINGCONTROLLER_H_

// Header for the virtual base Castalia MAC module 
#include "VirtualRouting.h"
#include "CtpRoutingControlMessage_m.h"
#include "RoutingControlMessage_m.h"
#include "CtpRoutingPacket_m.h"
#include "BeaconSenderControlMessage_m.h"

enum CtpRoutingControllerTimers {
	CTP_ROUTING_CONTROLLER_TIMER_LOOP_REPAIR = 1,
	CTP_ROUTING_CONTROLLER_TIMER_DELAY_ROUTE_DISCOVERY_PROPAGATION = 2
};

struct duplicatePacketBufferEntry {
	ctpRoutingPacket_type type;
	unsigned int seqNo;
	unsigned int hopCount;
};

class CtpRoutingController : public VirtualRouting
{
	private:
		//=========== Private NED file parameters ============
		// See NED file for comments
		int maxPacketSendRetries;
		int networkDataFrameOverheadBits;
		double repairLoopWaitTimeMin;
		double repairLoopWaitTimeRange;
		
		//=========== Other private variables ============
		static const char *OUTPUT_CTP_DROPPED_AFTER_MAX_RETRIES;
		static const char *OUTPUT_CTP_DROPPED_OUT_OF_ENERGY;
		static const char *OUTPUT_CTP_SENDING_RETRY;
		static const char *OUTPUT_CTP_FORWARDING;
		static const char *OUTPUT_CTP_HOP_COUNT;
		bool isSink;
		double delayBeforeRouteDiscoveryPropagation;
		bool implementRetries;
		int sinkNodeId;
		int currentParentNodeId;
		double currentMultihopEtxToRoot;
		bool isSending;
		bool waitingForLoopRepair;
		int currentPacketSendingAttempts;
		// The buffer size in TinyOS is 4. Doubling to 8 because beacons and data packets
		// have separate seqNo streams, so need to account for both types separately
		const unsigned int duplicateDetectionBufferSize = 4;	
		vector<list<duplicatePacketBufferEntry>> duplicateDetectionBuffer;

		//=========== Private member functions ===========
		void initiateRouteDiscoveryAndPropagation();
		bool isDuplicate(CtpRoutingPacket *pkt);
		void sendPackets();
		void repairLoop();
		void forwardPacket(CtpRoutingPacket *pkt);
		void clearDuplicateBuffer();

	protected:

		// Virtual methods from VirtualMac / CastaliaModule classes 
		void startup();
		void finishSpecific();
		void timerFiredCallback(int index);
		void fromApplicationLayer(cPacket *pkt, const char *destination);
		void fromMacLayer(cPacket *pkt, int srcMacAddress, double rssi, double lqi);
		void handleMacControlMessage(cMessage *msg);
		void handleRadioControlMessage(cMessage *msg);
		void handleNetworkControlCommand(cMessage *msg);
		void handleOutOfEnergy(cMessage *outOfEnergyMsg);
};

#endif //_CTPROUTINGCONTROLLER_H_