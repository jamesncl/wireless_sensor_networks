#include "MmbcrTableManager.h"

// Register the module with Omnet
Define_Module(MmbcrTableManager);

const char * MmbcrTableManager::OUTPUT_SH_ETX_TO_PARENT = "Mmbcr SH-ETX to parent";
const char * MmbcrTableManager::OUTPUT_MH_ETX = "Mmbcr MH-ETX";
const char * MmbcrTableManager::OUTPUT_TIMES_SWITCHED_PARENT = "Mmbcr Times switched parent";

void MmbcrTableManager::initialize()
{
	// Store NED parameters
	nodeRoutingTableMaxSize = par("nodeRoutingTableMaxSize");
	evictionEtxThreshold = par("evictionEtxThreshold");
	newParentSwitchThreshold = par("newParentSwitchThreshold");
	unreachableNodeShEtxThreshold = par("unreachableNodeShEtxThreshold");
	weightingMhEtx = par("weightingMhEtx");
	weightingMmbcr = par("weightingMmbcr");

	// Initialise private variables
	invalidateParent(false); // (re)initialises current parent and MHETX variables 
	isSink = false;

	// We just need this temporarily in order to set the clock drift on the timer service for this module
	resMgrModule = check_and_cast <ResourceManager*>(
		getParentModule()   // Routing compound module
		->getParentModule() // Communication module
		->getParentModule() // Node module
		->getSubmodule("ResourceManager")
		->getSubmodule("ResourceManager"));

	if (!resMgrModule) {
		opp_error("Error getting a valid reference to resource manager module");
	}

	// Set the timer drift for the timer service. If we do not do this all timers will return immediately!
	setTimerDrift(resMgrModule->getCPUClockDrift());

	// Get the Node ID - we need this for setting the origin / source of beacons
	selfNodeId = getParentModule() // Routing container module
		->getParentModule()  // Communication module
		->getParentModule()  // Node module
		->getIndex();

	// Declare stats outputs
	// declareHistogram(name, min value, max value, number of buckets)
	declareHistogram(OUTPUT_SH_ETX_TO_PARENT, 1, 10, 10);
	declareHistogram(OUTPUT_MH_ETX, 1, 10, 10);
	declareOutput(OUTPUT_TIMES_SWITCHED_PARENT);
}

void MmbcrTableManager::handleMessage(cMessage *msg)
{
	// A message has arrived from the routing controller
	switch (msg->getKind()) {

		// To use Castalia's timer services in a module which does not extend VirtualMac, VirtualApplication etc.
		// we have to to this bit of joining up: call the timer service's handleTimerMessage function when we receive a timer message
		case TIMER_SERVICE:
		{
			handleTimerMessage(msg, false);
			break;
		}

		case NETWORK_LAYER_PACKET:
		{
			MmbcrPacket *beacon = check_and_cast<MmbcrPacket*>(msg);
			switch(beacon->getRoutingPacketKind())
			{
				case MMBCR_PACKET_TYPE_BEACON:
				{
					if(!isSink)
					{
						updateNodeBatteryCapacitiesToSink(beacon->getNetMacInfoExchange().lastHop, beacon->getBatteryCapacitiesOfPathToSink());
					}
					break;
				}

				default:
				{
					opp_error("Unexpected network layer packet kind");
				}
			}
			break;
		}

		case TABLE_MANAGER_CONTROL_COMMAND:
		{
			MmbcrTableManagerControlMessage *controlMsg = check_and_cast <MmbcrTableManagerControlMessage*>(msg);
			switch(controlMsg->getTableManagerControlMessageKind())
			{
				case MMBCR_TABLE_MANAGER_UPDATE_NODE_ETX: {
					// If we are the sink, we don't need the routing table so don't do anything
					if(!isSink)
					{
						trace() << "Received single-hop ETX for node " << controlMsg->getNodeId() << ": " << controlMsg->getValue();
						updateEtxLinkQualityToNode(controlMsg->getNodeId(), controlMsg->getValue());
					}
					break;
				}

				case MMBCR_TABLE_MANAGER_UPDATE_REMOTE_NODE_ROUTING_TABLE_INFO: {
					// If we are the sink, we don't need the routing table so don't do anything
					if(!isSink)
					{
						trace() << "Received remote routing table info for node " << controlMsg->getNodeId() 
							<< ", MH-ETX: " << controlMsg->getValue() << ", Parent node ID: " << controlMsg->getParentNodeId();
						updateParentAndMultihopEtxToRootForRemoteNode(
							controlMsg->getNodeId(), 
							controlMsg->getValue(),	// Value is MH-ETX
							controlMsg->getParentNodeId());
					}
					else
					{
						trace() << "This is sink node so ignoring routing table info";
					}
					break;
				}

				default: {
					opp_error("Unknown table manager control message kind");
				}
			}
			
			break;
		}

		case NETWORK_CONTROL_COMMAND: {
			RoutingControlMessage *controlMsg = check_and_cast<RoutingControlMessage*>(msg);

			switch(controlMsg->getRoutingControlMessageKind()) {

				case ROUTING_MSG_SINK_NODE_UPDATE: {

					sinkNodeId = controlMsg->getValue();
					if(sinkNodeId == selfNodeId) {
						trace() << "This node is sink";
						isSink = true;

						// The sink always has multihop ETX to root value of zero
						currentMultihopEtxToRoot = 0;
					}
					break;
				}

				default: {
					opp_error("Unknown network control message type");
				}
			}

			break;
		}

		case OUT_OF_ENERGY:
		{
			// We need to simulate what happens when a node runs out of energy - all state will be lost

			// Reinitialise private variables
			currentMultihopEtxToRoot = -1; 	// -1 means invalid
			currentParentNodeId = -1;		// -1 means no parent
			// Clear routing state
			nodeRoutingTable.clear();
			// Cancel any pending timers
			cancelAllTimers();
			break;
		}

		default:
		{
			opp_error("Unknown message type");
		}
	}

	cancelAndDelete(msg);
}

void MmbcrTableManager::updateNodeBatteryCapacitiesToSink(int nodeId, BatteryCapacityVector_t nodeBatteryCapacitiesToSink)
{
	// If we already have an entry for this node
	if(nodeRoutingTable.count(nodeId) > 0)
	{
		// Update the information
		nodeRoutingTable[nodeId].batteryCapacitiesOfPathToSink = nodeBatteryCapacitiesToSink;
	}
	else
	{
		// Otherwise, try to add a new entry.
		if(nodeRoutingTable.size() < nodeRoutingTableMaxSize)
		{
			// Add the new node and update
			nodeRoutingTable[nodeId] = NodeRoutingInfo_t();
			nodeRoutingTable[nodeId].batteryCapacitiesOfPathToSink = nodeBatteryCapacitiesToSink;
		}
		else
		{
			trace() << "MMBCR - unable to add new node battery capacities to sink record - routing table full";
			return;
		}
	}

	trace() << "Updated battery capacities to sink for node " << nodeId << ":";
	for(BatteryCapacityVector_t::iterator it = nodeBatteryCapacitiesToSink.begin(); it != nodeBatteryCapacitiesToSink.end(); ++it) 
	{
    	trace() << std::to_string(*it);
	}

	// Opportunity to update the parent
	updateParentAndMultihopEtxToRoot();
}

void MmbcrTableManager::notifyBeaconSenderMultihopEtx()
{
	//trace() << "Updating beacon sender with multihop ETX to root: " << currentMultihopEtxToRoot;
	MmbcrBeaconSenderControlMessage *updateMhEtxMsg = new MmbcrBeaconSenderControlMessage("Update beacon sender MH-ETX to root", BEACON_SENDER_CONTROL_COMMAND);
	updateMhEtxMsg->setBeaconSenderControlMessageKind(MMBCR_BEACON_SENDER_UPDATE_MULTIHOP_ETX_TO_ROOT);
	updateMhEtxMsg->setMultihopEtxToRoot(currentMultihopEtxToRoot);
	send(updateMhEtxMsg, "toBeaconSender");
}

void MmbcrTableManager::notifyControllerMultihopEtxAndParent()
{
	//trace() << "Updating controller with parent: " << currentParentNodeId << " and multihop ETX " << currentMultihopEtxToRoot;
	MmbcrControlMessage *updateMultihopEtxParentMsg = new MmbcrControlMessage("Update controller parent", CTP_NETWORK_CONTROL_COMMAND);
	updateMultihopEtxParentMsg->setMmbcrControlMessageKind(MMBCR_MSG_UPDATE_ROUTE_INFO);
	updateMultihopEtxParentMsg->setValue(currentParentNodeId);
	// We update the controller with multihop ETX for routing loop detection 
	updateMultihopEtxParentMsg->setMultihopEtx(currentMultihopEtxToRoot);
	send(updateMultihopEtxParentMsg, "toController");
}

void MmbcrTableManager::updateEtxLinkQualityToNode(int nodeId, double singleHopEtx)
{
	// If we already have an entry for this node
	if(nodeRoutingTable.count(nodeId) > 0)
	{
		// Check if the SH ETX has risen above the maximum threshold, which would
		// indicate the node is unreachable
		if(singleHopEtx > unreachableNodeShEtxThreshold)
		{
			trace() << "Evicting unreachable node " << nodeId << " with SH-ETX " << singleHopEtx;

			// If it is, we need to remove this node from the routing table
			nodeRoutingTable.erase(nodeId);
			
			// If it was our parent
			if(currentParentNodeId == nodeId)
			{
				invalidateParent(true); // true means send updates to submodules (beacon sender, controller etc.) to notify of update
	
				// Also, trigger a pull request so we get updated routing info from
				// neighbours
				MmbcrBeaconSenderControlMessage *resetTrickleAndPullMsg = new MmbcrBeaconSenderControlMessage("Reset trickle and pull message", BEACON_SENDER_CONTROL_COMMAND);
				resetTrickleAndPullMsg->setBeaconSenderControlMessageKind(MMBCR_BEACON_SENDER_RESET_TRICKLE_AND_PULL);
				send(resetTrickleAndPullMsg, "toBeaconSender");
			}
		}
		else
		{
			// Update the existing entry
			trace() << "Updating existing routing table entry for node " << nodeId << " with single hop ETX " << singleHopEtx;
			nodeRoutingTable[nodeId].etxLinkQualityToNode = singleHopEtx;
		}
	}
	else
	{
		// Otherwise, try to add a new entry.
		// -1 refers to the multihop ETX to root - because this is a new node, we must not have received it's multihop ETX to root yet
		if(attemptAddNodeToTable(nodeId, -1))
		{
			// Update the new entry
			trace() << "Adding new routing table entry for node " << nodeId << " with single hop ETX " << singleHopEtx;
			nodeRoutingTable[nodeId].etxLinkQualityToNode = singleHopEtx;
		}
	}

	// Select / amend our parent based on new routing info
	updateParentAndMultihopEtxToRoot();
}

void MmbcrTableManager::invalidateParent(bool notifyOtherModules)
{
	// we need to record our current parent as now invalid
	currentMultihopEtxToRoot = -1;
	currentParentNodeId = -1;
	currentParentCombinedMmbcrMhEtxMetric = -1;

	if(notifyOtherModules)
	{
		// We need to notify controller and beacon sender because our
		// parent has changed
		notifyBeaconSenderMultihopEtx();
		notifyControllerMultihopEtxAndParent();
		notifyBeaconSenderParentBatteryCapacitiesToSink();
		plotTrace() << "#ROU_INVALID_PARENT";
	}
}

void MmbcrTableManager::updateParentAndMultihopEtxToRootForRemoteNode(int nodeId, double multihopEtxToRoot, int parentNodeId)
{
	// If we already have an entry for this node
	if(nodeRoutingTable.count(nodeId) > 0)
	{
		// Update the existing entry
		trace() << "Updating existing routing table entry for node " << nodeId 
			<< " with multihop ETX to root " << multihopEtxToRoot
			<< " and parent " << parentNodeId;
		nodeRoutingTable[nodeId].nodeMultihopEtxToRoot = multihopEtxToRoot;
		nodeRoutingTable[nodeId].parentNodeId = parentNodeId;
	}
	else
	{
		// Otherwise, try to add a new entry.
		if(attemptAddNodeToTable(nodeId, multihopEtxToRoot))
		{
			// If we succeed in adding a new entry, update the new entry
			trace() << "Adding new routing table entry for node " << nodeId 
				<< " with multihop ETX to root " << multihopEtxToRoot
				<< " and parent " << parentNodeId;
			nodeRoutingTable[nodeId].nodeMultihopEtxToRoot = multihopEtxToRoot;
			nodeRoutingTable[nodeId].parentNodeId = parentNodeId;
		}
	}

	// Select / amend our parent based on new routing info
	updateParentAndMultihopEtxToRoot();
}

bool MmbcrTableManager::attemptAddNodeToTable(int nodeId, double newNodeMultihopEtxToRoot)
{
	// If there is space in the table
	if(nodeRoutingTable.size() < nodeRoutingTableMaxSize)
	{
		// simply add the new node
		nodeRoutingTable[nodeId] = NodeRoutingInfo_t();
		return true;
	}
	else
	{
		// Otherwise, we need to see if we can evict a node
		// we set force to true if the node we want to add is root
		// Pass the new node's multihop etx to root as this may be used when choosing a node to evict
		if(attemptEvictNode(nodeId == sinkNodeId ? true: false, newNodeMultihopEtxToRoot))
		{
			nodeRoutingTable[nodeId] = NodeRoutingInfo_t();
			return true;
		}
		else
		{
			trace() << "Could not add new node entry to table - table full and no node could be evicted";
			return false;
		}
	}
}

bool MmbcrTableManager::attemptEvictNode(bool force, double newNodeMultihopEtxToRoot)
{
	bool foundNodeEligibleForEviction = false;
	int eligibleNodeId = -1;
	double eligibleNodeEtx = -1;
	std::map<int, NodeRoutingInfo_t>::iterator it;

	// First, check if there are any entries with one-hop ETX above eviction threshold
	// If there are any, choose the one with highest ETX
	for (it = nodeRoutingTable.begin(); it != nodeRoutingTable.end(); ++it)
	{
		// it->first is the key (int nodeId)
		// it->second is the value (NodeRoutingInfo_t)

		// If the single hop ETX link quality to node is above threshold, and the node is not the root (we never evict root)
		if(it->second.etxLinkQualityToNode >= evictionEtxThreshold && it->first != sinkNodeId)
		{
			// If we have already found an eligible node
			if(foundNodeEligibleForEviction)
			{
				// Replace it if we have found a new one with an even higher ETX
				if(it->second.etxLinkQualityToNode > eligibleNodeEtx)
				{
					eligibleNodeId = it->first;
					eligibleNodeEtx = it->second.etxLinkQualityToNode;
				}
				// Otherwise don't replace - do nothing
			}
			else // Otherwise we have found the first eligible node
			{
				foundNodeEligibleForEviction = true;
				// Store the node details
				eligibleNodeId = it->first;
				eligibleNodeEtx = it->second.etxLinkQualityToNode;
			}
		}

		if(foundNodeEligibleForEviction) {
			trace () << "Found node eligible for eviction due to high ETX above threshold: " << eligibleNodeId;
		}
	}

	// If we haven't found an eligible node yet, check again to see if any node has a higher multihop ETX to root than the one we want to add
	if(!foundNodeEligibleForEviction)
	{
		for (it = nodeRoutingTable.begin(); it != nodeRoutingTable.end(); ++it)
		{
			// it->first is the key (int nodeId)
			// it->second is the value (NodeRoutingInfo_t)

			// If the node's multihop ETX to root is higher than the new one we want to add, and it's not the root (we never evict root)
			if(it->second.nodeMultihopEtxToRoot > newNodeMultihopEtxToRoot && it->first != sinkNodeId)
			{
				// We can evict the node
				foundNodeEligibleForEviction = true;
				eligibleNodeId = it->first;
				trace () << "Found node eligible for eviction (node id " << eligibleNodeId << ") because it had a lower multihop ETX to root(" 
					<< it->second.nodeMultihopEtxToRoot << " compared to " << newNodeMultihopEtxToRoot << ")";
			}
		}
	}

	// Finally, if we haven't found an eligible node, and force eviction if flag is set (e.g. because we are adding root node)
	if((!foundNodeEligibleForEviction) && force)
	{
		// we need to force an eviction of a random node
		it = nodeRoutingTable.begin();
		// Advance through the table a random number of places
		std::advance(it, intrand(nodeRoutingTable.size()));
		// Evict the unlucky node
		foundNodeEligibleForEviction = true;
		eligibleNodeId = it->first;
		trace() << "Forcing eviction of randomly chosen node " << eligibleNodeId;
	}
	
	// If we have found an eligible node, evict it
	if(foundNodeEligibleForEviction)
	{
		trace() << "Evicting node " << eligibleNodeId;
		nodeRoutingTable.erase(eligibleNodeId);
		return true;
	}
	else
	{
		return false;
	}
}

void MmbcrTableManager::updateParentAndMultihopEtxToRoot()
{
	// Keep track of if anything has changed which requires sending an update message to other modules
	bool parentHasChanged = false;
	bool mhEtxHasChanged = false;

	// Select parent

	// First find the most suitable potential parent (if any)
	// If there are no suitable candidates this returns -1
	int potentialNewParentNodeId = findBestParentCandidate();

	// Have we found a valid candidate?
	if(potentialNewParentNodeId != -1)
	{
		collectOutput(OUTPUT_TIMES_SWITCHED_PARENT);
		trace() << "Setting new parent node: " << potentialNewParentNodeId;
		plotTrace() << "#ROU_PARENT " << potentialNewParentNodeId;
		currentParentNodeId = potentialNewParentNodeId;
		notifyBeaconSenderNewParent();
		parentHasChanged = true;
	}
	
	if(currentParentNodeId != -1)
	{
		// If we have a valid parent, calculate our multihop ETX as parent's multihop ETX plus our single-hop link quality to the parent
		double updatedNodeMultihopEtxToRoot = nodeRoutingTable[currentParentNodeId].nodeMultihopEtxToRoot + 
										nodeRoutingTable[currentParentNodeId].etxLinkQualityToNode;
		
		// Also update battery capacities to sink
		notifyBeaconSenderParentBatteryCapacitiesToSink();
		
		// For performance reasons, test if this has actually changed
		if(currentMultihopEtxToRoot != updatedNodeMultihopEtxToRoot)
		{
			currentMultihopEtxToRoot = updatedNodeMultihopEtxToRoot;
			mhEtxHasChanged = true;

			trace() << "Our multihop ETX to root is our parent's MH-ETX(" << nodeRoutingTable[currentParentNodeId].nodeMultihopEtxToRoot
			<< ") + SH-ETX to parent (" << nodeRoutingTable[currentParentNodeId].etxLinkQualityToNode
			<< ") = " << currentMultihopEtxToRoot;
			plotTrace() << "#ROU_MHETX " << currentParentNodeId << " " << currentMultihopEtxToRoot;

			// Collect stats
			collectHistogram(OUTPUT_SH_ETX_TO_PARENT, nodeRoutingTable[currentParentNodeId].etxLinkQualityToNode);
		} 
	}
	
	if(mhEtxHasChanged) 
	{
		// Notify the beacon sender with our new MH ETX so that beacons now contain the latest MH ETX
		notifyBeaconSenderMultihopEtx();

		// For stats
		if(currentParentNodeId != -1)
		{
			collectHistogram(OUTPUT_MH_ETX, currentMultihopEtxToRoot);
		}
	}

	if(mhEtxHasChanged || parentHasChanged) {
		// Notify controller with our new parent and/or MHETX so it sends data packets to the correct node
		notifyControllerMultihopEtxAndParent();
	}
}

void MmbcrTableManager::notifyBeaconSenderParentBatteryCapacitiesToSink()
{
	BatteryCapacityVector_t updatedBatteryCapacitiesToSink;

	if(currentParentNodeId != -1)
	{
		// Get parent's list of batteries to sink
		updatedBatteryCapacitiesToSink = nodeRoutingTable[currentParentNodeId].batteryCapacitiesOfPathToSink;
	}
	else
	{
		// If invalid parent, we just send the empty list
	}
	
	MmbcrBeaconSenderControlMessage *newBatCapMsg = new MmbcrBeaconSenderControlMessage("Reset trickle message", BEACON_SENDER_CONTROL_COMMAND);
	newBatCapMsg->setBeaconSenderControlMessageKind(MMBCR_BEACON_SENDER_NEW_BATTERY_CAPACITIES_TO_SINK);
	newBatCapMsg->setBatteryCapacitiesOfPathToSink(updatedBatteryCapacitiesToSink);
	send(newBatCapMsg, "toBeaconSender");
}

void MmbcrTableManager::notifyBeaconSenderNewParent()
{
	// When we have selected a new parent, we need to reset the trickle algorithm 
	// so we quickly send the updated routing into to neighbours
	// Also allow the beacon sender to send the new parent node ID in beacons
	MmbcrBeaconSenderControlMessage *newParentMsg = new MmbcrBeaconSenderControlMessage("Reset trickle message", BEACON_SENDER_CONTROL_COMMAND);
	newParentMsg->setBeaconSenderControlMessageKind(MMBCR_BEACON_SENDER_NEW_PARENT);
	newParentMsg->setParentNodeId(currentParentNodeId);
	send(newParentMsg, "toBeaconSender");
}

void MmbcrTableManager::updateCurrentParentMmbcrMetric()
{
	// Go thorugh every node
	for (std::map<int, NodeRoutingInfo_t>::iterator it = nodeRoutingTable.begin(); it != nodeRoutingTable.end(); ++it)
	{
		// If its the current parent node
		if(it->first == currentParentNodeId)
		{
			// Take the reciprocal of the MHETX, so that we get a number from 0-1 with higher being better, lower worse
			// Need to check if MHETX is zero (only sink has MHETX 0), if zero make metric 1
			double mhEtxMetric;

			if(it->second.nodeMultihopEtxToRoot == 0)
			{
				mhEtxMetric = 1;
			}
			else
			{
				mhEtxMetric = 1.0 / it->second.nodeMultihopEtxToRoot;
			}

			double mmbcrMetric;
			if(it->second.batteryCapacitiesOfPathToSink.size() == 0)
			{
				// Must be sink - set highest (best) possible mmbcr metric
				mmbcrMetric = 1;
			}
			else
			{
				BatteryCapacityVector_t::iterator minimumBatteryCapacityIt = std::min_element(it->second.batteryCapacitiesOfPathToSink.begin(), it->second.batteryCapacitiesOfPathToSink.end());
				mmbcrMetric = *minimumBatteryCapacityIt;
			}

			double weightedMmbcrMetric = mmbcrMetric * weightingMmbcr;
			double weightedMhEtxMetric = mhEtxMetric * weightingMhEtx;
			currentParentCombinedMmbcrMhEtxMetric = weightedMmbcrMetric + weightedMhEtxMetric;
		}
	}
}

int MmbcrTableManager::findBestParentCandidate()
{
	// Update the current parent's mmbcr metric so that were comparing against a valid value
	updateCurrentParentMmbcrMetric();

	// vector to store potential candidates
	std::vector<CandidateParent_t> candidates;

	// FIRST calculate the MMBCR metric:
	// - For each node in the routing table, if it has a valid parent (and therefore a route to sink),
	//   AND it has a valid singleHopEtx (so that we have collected enough beacons to determine link quality)
	//   AND the singleHopEtx is under unreachableNodeShEtxThreshold:
	// - Look at the list of battery capacities of nodes on the route to sink. Calcualte the minimum node
	//   battery capacity (the battery capacity of the node with the lowest capacity)
	//   (note that the list may be empty, if the node is the sink - in which case the metric should be 1 (maximum possible))
	//   This is the MMBCR cost metric.

	for (std::map<int, NodeRoutingInfo_t>::iterator it = nodeRoutingTable.begin(); it != nodeRoutingTable.end(); ++it)
	{
		// trace() << "MMBCR - checking potential candidate " << it->first << ": "
		// 	<< "nodeMultihopEtxToRoot != -1? " << (it->second.nodeMultihopEtxToRoot != -1 ? "yes" : "no")
		// 	<< " it->second.etxLinkQualityToNode != -1? " << (it->second.etxLinkQualityToNode != -1 ? "yes" : "no")
		// 	<< " it->second.parentNodeId != selfNodeId? " << (it->second.parentNodeId != selfNodeId ? "yes" : "no")
		// 	<< " it->second.etxLinkQualityToNode < unreachableNodeShEtxThreshold? " << (it->second.etxLinkQualityToNode < unreachableNodeShEtxThreshold ? "yes" : "no")
		// 	;

		// If it has a valid multihop ETX to root,
		// and it has a valid singlehop ETX to the node,
		// and it does not have us as their parent (this would create a parent-parent loop)
		// and its not the current parent
		// and the singlehop etx is above threshold
		if(it->second.nodeMultihopEtxToRoot != -1
			&& it->second.etxLinkQualityToNode != -1
			&& it->second.parentNodeId != selfNodeId
			&& it->first != currentParentNodeId)
		{
			// Take the reciprocal of the MHETX, so that we get a number from 0-1 with higher being better, lower worse
			// Need to check if MHETX is zero (only sink has MHETX 0), if zero make metric 1
			double mhEtxMetric;

			if(it->second.nodeMultihopEtxToRoot == 0)
			{
				mhEtxMetric = 1;
			}
			else
			{
				mhEtxMetric = 1.0 / it->second.nodeMultihopEtxToRoot;
			}

			double mmbcrMetric;
			if(it->second.batteryCapacitiesOfPathToSink.size() == 0)
			{
				// Must be sink - set highest (best) possible mmbcr metric
				mmbcrMetric = 1;
			}
			else
			{
				BatteryCapacityVector_t::iterator minimumBatteryCapacityIt = std::min_element(it->second.batteryCapacitiesOfPathToSink.begin(), it->second.batteryCapacitiesOfPathToSink.end());
				mmbcrMetric = *minimumBatteryCapacityIt;
			}

			CandidateParent_t candidate;
			candidate.nodeId = it->first;
			candidate.weightedMmbcrMetric = mmbcrMetric * weightingMmbcr;
			candidate.weightedMhEtxMetric = mhEtxMetric * weightingMhEtx;
			candidate.combinedMetric = candidate.weightedMmbcrMetric + candidate.weightedMhEtxMetric;
			candidates.push_back(candidate);

			// if this is the current parent, update the stored current parent metric (we may need this later)
			if(candidate.nodeId == currentParentNodeId)
			{
				currentParentCombinedMmbcrMhEtxMetric = candidate.combinedMetric;
			}

			trace() << "MMBCR - found candidate parent " << candidate.nodeId
				<< " MMBCR " << mmbcrMetric << " (weighted " << candidate.weightedMmbcrMetric << ") "
				<< "MhETX original " <<it->second.nodeMultihopEtxToRoot << ", reciprocal " << mhEtxMetric << " (weighted " << candidate.weightedMhEtxMetric << ") "
				<< "combined " << candidate.combinedMetric;
			trace() << "The candidate parents chain of capacities:";
			for(BatteryCapacityVector_t::iterator capIt = it->second.batteryCapacitiesOfPathToSink.begin();
				capIt != it->second.batteryCapacitiesOfPathToSink.end(); capIt++)
			{
				trace() << (*capIt);
			}
		}
	}

	if(candidates.size() == 0)
	{
		trace() << "MMBCR - No candidate parents";
		return -1;
	}
	else
	{
		// Find the candidate with the maximum combined weighting
		std::vector<CandidateParent_t>::iterator bestCandidateIt = std::max_element(candidates.begin(), candidates.end(),
			[](CandidateParent_t a, CandidateParent_t b){ return a.combinedMetric < b.combinedMetric; });
		CandidateParent_t bestCandidate = *bestCandidateIt;
		
		// If the best candidate is already the parent, return no new candidate
		if(bestCandidate.nodeId == currentParentNodeId)
		{
			trace() << "Best candidate is already the curret parent";
			return -1;
		}
		else
		{
			// If there is a valid current parent
			if(currentParentNodeId != -1)
			{
				// Only return a new candidate if its better than at least the switching threshold
				if(bestCandidate.combinedMetric >= currentParentCombinedMmbcrMhEtxMetric + newParentSwitchThreshold)
				{
					trace() << "Best candidate (ID " << bestCandidate.nodeId << " metric " << bestCandidate.combinedMetric 
						<< ") is better (greater than) current parent metric (" << currentParentCombinedMmbcrMhEtxMetric 
						<< ") plus switching threshold (" << newParentSwitchThreshold << ") = " 
						<< currentParentCombinedMmbcrMhEtxMetric + newParentSwitchThreshold;
					currentParentCombinedMmbcrMhEtxMetric = bestCandidate.combinedMetric;
					return bestCandidate.nodeId;
				}
				else
				{
					trace() << "Best candidate (ID " << bestCandidate.nodeId << " metric " << bestCandidate.combinedMetric 
						<< ") is NOT better (greater than) current parent metric (" << currentParentCombinedMmbcrMhEtxMetric 
						<< ") plus switching threshold (" << newParentSwitchThreshold << ") = " 
						<< currentParentCombinedMmbcrMhEtxMetric + newParentSwitchThreshold;
					return -1;
				}
			}
			else
			{
				trace() << "Best candidate is ID " << bestCandidate.nodeId << " metric" << bestCandidate.combinedMetric;
				return bestCandidate.nodeId;
			}
		}
	}
}

void MmbcrTableManager::timerFiredCallback(int index)
{
	// switch(index) {
	// 	case ???: {

	// 		break;
	// 	}

	// 	default: {
	// 		opp_error("Unknown timer type");
	// 	}
	// }
}

void MmbcrTableManager::finishSpecific()
{
	trace() << "Routing table (parent is " << currentParentNodeId << "):";
	trace() << "nodeid:  SH-ETX  MH-ETX";
	// Print the routing table
	for (std::map<int, NodeRoutingInfo_t>::iterator it = nodeRoutingTable.begin(); it != nodeRoutingTable.end(); ++it)
	{
		// it->first is the key (int nodeId)
		// it->second is the value (NodeRoutingInfo_t)
		trace() << it->first << ":\t" << it->second.etxLinkQualityToNode << "\t" << it->second.nodeMultihopEtxToRoot;
	}
}