#ifndef _CTPROUTINGTABLEMANAGER_H_
#define _CTPROUTINGTABLEMANAGER_H_

#include "CastaliaModule.h"
#include "TimerService.h"
#include "ResourceManager.h"
#include "TableManagerControlMessage_m.h"
#include "RoutingControlMessage_m.h"
#include "CtpRoutingControlMessage_m.h"
#include "BeaconSenderControlMessage_m.h"

enum tableManagerTimers {

};

struct NodeRoutingInfo_t {
	// default Constructor: initialise with an invalid MHETX, SHETX and parent (-1 means invalid)
	NodeRoutingInfo_t(): nodeMultihopEtxToRoot(-1), etxLinkQualityToNode(-1), parentNodeId(-1) { }   
	double nodeMultihopEtxToRoot;
	double etxLinkQualityToNode;
	int parentNodeId;
};

class CtpRoutingTableManager : public CastaliaModule, public TimerService
{
	private:
		// NED file parameters:
		unsigned int nodeRoutingTableMaxSize;
		double evictionEtxThreshold;
		double newParentSwitchAdditionalMhEtx;
		double unreachableNodeShEtxThreshold;
		
		// Other private variables:
		static const char *OUTPUT_SH_ETX_TO_PARENT;
		static const char *OUTPUT_MH_ETX;
		static const char *OUTPUT_TIMES_SWITCHED_PARENT;
		int selfNodeId;
		int sinkNodeId;
		bool isSink;
		double currentMultihopEtxToRoot;
		int currentParentNodeId;
		std::map<int, NodeRoutingInfo_t> nodeRoutingTable;

		// Private member functions:
		
		void invalidateParent(bool notifyOtherModules);
		void updateEtxLinkQualityToNode(int nodeId, double singleHopEtx);
		void updateParentAndMultihopEtxToRootForRemoteNode(int nodeId, double multihopEtxToRoot, int parentNodeId);
		bool attemptAddNodeToTable(int nodeId, double newNodeMultihopEtxToRoot);
		bool attemptEvictNode(bool force, double newNodeMultihopEtxToRoot);
		void updateParentAndMultihopEtxToRoot();
		int findBestParentCandidate();
		void notifyBeaconSenderNewParent();
		void notifyBeaconSenderMultihopEtx();
		void notifyControllerMultihopEtxAndParent();

	protected:
		
		// Virtual methods from CastaliaModule / TimerService class
		void initialize();
		void finishSpecific();
		void handleMessage(cMessage *msg);
		void timerFiredCallback(int index);
};

#endif //_CTPROUTINGTABLEMANAGER_H_