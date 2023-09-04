#ifndef _MMBCRCONTROLLER_H_
#define _MMBCRCONTROLLER_H_

// Header for the virtual base Castalia MAC module 
#include "VirtualRouting.h"
#include "MmbcrControlMessage_m.h"
#include "RoutingControlMessage_m.h"
#include "MmbcrPacket_m.h"
#include "MmbcrBeaconSenderControlMessage_m.h"

enum MmbcrControllerTimers {
	MMBCR_CONTROLLER_TIMER_LOOP_REPAIR = 1,
	MMBCR_CONTROLLER_TIMER_DELAY_ROUTE_DISCOVERY_PROPAGATION = 2
};

struct duplicatePacketBufferEntry {
	mmbcrPacket_type type;
	unsigned int seqNo;
	unsigned int hopCount;
};

class MmbcrController : public VirtualRouting
{
	private:
		//=========== Private NED file parameters ============
		// See NED file for comments
		int maxPacketSendRetries;
		int networkDataFrameOverheadBits;
		double repairLoopWaitTimeMin;
		double repairLoopWaitTimeRange;
		
		//=========== Other private variables ============
		static const char *OUTPUT_MMBCR_DROPPED_AFTER_MAX_RETRIES;
		static const char *OUTPUT_MMBCR_DROPPED_OUT_OF_ENERGY;
		static const char *OUTPUT_MMBCR_SENDING_RETRY;
		static const char *OUTPUT_MMBCR_FORWARDING;
		static const char *OUTPUT_MMBCR_HOP_COUNT;
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
		bool isDuplicate(MmbcrPacket *pkt);
		void sendPackets();
		void repairLoop();
		void forwardPacket(MmbcrPacket *pkt);
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

#endif //_MMBCRCONTROLLER_H_