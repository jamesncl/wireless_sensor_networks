#ifndef _MMBCRTABLEMANAGER_H_
#define _MMBCRTABLEMANAGER_H_

#include <algorithm> // std::min_element
#include "CastaliaModule.h"
#include "TimerService.h"
#include "ResourceManager.h"
#include "MmbcrTableManagerControlMessage_m.h"
#include "RoutingControlMessage_m.h"
#include "MmbcrControlMessage_m.h"
#include "MmbcrPacket_m.h"
#include "MmbcrBeaconSenderControlMessage_m.h"

struct NodeRoutingInfo_t {
	// default Constructor: initialise with an invalid MHETX, SHETX and parent (-1 means invalid)
	NodeRoutingInfo_t(): nodeMultihopEtxToRoot(-1), etxLinkQualityToNode(-1), parentNodeId(-1) { }   
	double nodeMultihopEtxToRoot;
	double etxLinkQualityToNode;
	int parentNodeId;
	BatteryCapacityVector_t batteryCapacitiesOfPathToSink;
};

struct CandidateParent_t {
	int nodeId;
	double weightedMmbcrMetric;
	double weightedMhEtxMetric;
	double combinedMetric;
};

class MmbcrTableManager : public CastaliaModule, public TimerService
{
	private:
		// NED file parameters:
		unsigned int nodeRoutingTableMaxSize;
		double evictionEtxThreshold;
		double newParentSwitchThreshold;
		double unreachableNodeShEtxThreshold;
		double weightingMhEtx;
		double weightingMmbcr;
		
		// Other private variables:
		static const char *OUTPUT_SH_ETX_TO_PARENT;
		static const char *OUTPUT_MH_ETX;
		static const char *OUTPUT_TIMES_SWITCHED_PARENT;
		int selfNodeId;
		int sinkNodeId;
		bool isSink;
		double currentMultihopEtxToRoot;
		double currentParentCombinedMmbcrMhEtxMetric;
		int currentParentNodeId;
		std::map<int, NodeRoutingInfo_t> nodeRoutingTable;
		ResourceManager *resMgrModule;
		

		// Private member functions:
		
		void invalidateParent(bool notifyOtherModules);
		void updateEtxLinkQualityToNode(int nodeId, double singleHopEtx);
		void updateParentAndMultihopEtxToRootForRemoteNode(int nodeId, double multihopEtxToRoot, int parentNodeId);
		void updateNodeBatteryCapacitiesToSink(int nodeId, BatteryCapacityVector_t batteryCapacitiesOfPathToSink);
		bool attemptAddNodeToTable(int nodeId, double newNodeMultihopEtxToRoot);
		bool attemptEvictNode(bool force, double newNodeMultihopEtxToRoot);
		void updateParentAndMultihopEtxToRoot();
		void updateCurrentParentMmbcrMetric();
		int findBestParentCandidate();
		void notifyBeaconSenderNewParent();
		void notifyBeaconSenderMultihopEtx();
		void notifyBeaconSenderParentBatteryCapacitiesToSink();
		void notifyControllerMultihopEtxAndParent();

	protected:
		
		// Virtual methods from CastaliaModule / TimerService class
		void initialize();
		void finishSpecific();
		void handleMessage(cMessage *msg);
		void timerFiredCallback(int index);
};

#endif //_MMBCRTABLEMANAGER_H_