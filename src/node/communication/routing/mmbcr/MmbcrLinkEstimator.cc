#include "MmbcrLinkEstimator.h"

// Register the module with Omnet
Define_Module(MmbcrLinkEstimator);

// Const strings for output names
const char * MmbcrLinkEstimator::OUTPUT_LINK_QUALITY = "LinkEstimator LQ";

void MmbcrLinkEstimator::initialize()
{
	// Store NED parameters
	inLqSmoothingConst = par("inLqSmoothingConst");
	etxSmoothingConst = par("etxSmoothingConst");
	inBeaconWindowSize = par("inBeaconWindowSize");
	outMessageWindowSize = par("outMessageWindowSize");

	// Initialise private variables
	noUnsuccessfulDeliveriesSinceLastSuccessful = 0;

	// We just need this temporarily in order to set the clock drift on the timer service for this module
	ResourceManager *resMgrModule = check_and_cast <ResourceManager*>(
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

	// Declare outputs
	// declareHistogram(name, min value, max value, number of buckets)
	declareHistogram(OUTPUT_LINK_QUALITY, 1, 10, 10);
}

void MmbcrLinkEstimator::handleMessage(cMessage *msg)
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

		case NETWORK_CONTROL_COMMAND: {
			RoutingControlMessage *controlMsg = check_and_cast<RoutingControlMessage*>(msg);

			switch(controlMsg->getRoutingControlMessageKind()) {

				case ROUTING_MSG_MAC_SENDING_ACKED: {
					trace() << "Updating outgoing LQ of node " << controlMsg->getValue() << ": ACK received";
					updateOutgoingLinkQuality(controlMsg->getValue(), true); // value = the outgoing node id, true = message was ACKed
					break;
				}

				case ROUTING_MSG_MAC_SENDING_FAILED_NO_ACK: {
					trace() << "Updating outgoing LQ of node " << controlMsg->getValue() << ": No ACK";
					updateOutgoingLinkQuality(controlMsg->getValue(), false); // value = the outgoing node id, false = message not ACKed
					break;
				}

				default: {
					opp_error("Unknown network control message type");
				}
			}
			break;
		}
		
		case NETWORK_LAYER_PACKET: {
			MmbcrPacket *ctpPkt = check_and_cast<MmbcrPacket*>(msg);

			switch(ctpPkt->getRoutingPacketKind()) {

				case MMBCR_PACKET_TYPE_BEACON: {
					// We have received a beacon, we need to use this to update the incoming link quality metrics
					int beaconFromNode = ctpPkt->getNetMacInfoExchange().lastHop;
					int beaconSeqNo = ctpPkt->getSequenceNumber();

					// First check this isn't a duplicate of one we have already received
					if(lastSeqNoReceivedFromNode.count(beaconFromNode) > 0 &&
						lastSeqNoReceivedFromNode[beaconFromNode] == beaconSeqNo)
					{
						trace() << "Ignoring duplicate beacon " << beaconSeqNo << " form node " << beaconFromNode;
					}
					else
					{
						trace() << "Updating incoming LQ of node " << beaconFromNode << ": received beacon " << beaconSeqNo;
						updateIncomingLinkQuality(beaconFromNode, beaconSeqNo);
						
						// Also inform the table manager of the sender's mutihop ETX to root (for selecting our parent)
						// and the sender's parent (so we can check that we're not choosing a node as parent who has us as parent)
						MmbcrTableManagerControlMessage *updateMsg = new MmbcrTableManagerControlMessage("Update beacon sender multihop ETX", TABLE_MANAGER_CONTROL_COMMAND);
						updateMsg->setTableManagerControlMessageKind(MMBCR_TABLE_MANAGER_UPDATE_REMOTE_NODE_ROUTING_TABLE_INFO);
						updateMsg->setNodeId(beaconFromNode);
						updateMsg->setValue(ctpPkt->getMultihopEtxToRoot()); // Value is the multihop ETX of the node
						updateMsg->setParentNodeId(ctpPkt->getParentNodeId()); // This is the sender's parent
						send(updateMsg, "toTableManager");

						// Add this to the last received beacon seq no list so we can check for subsequent duplciates
						lastSeqNoReceivedFromNode[beaconFromNode] = beaconSeqNo;
					}
					break;
				}
			}

			break;
		}

		case OUT_OF_ENERGY:
		{
			// We need to simulate what happens when a node runs out of energy - all state will be lost

			// Reinitialise private variables
			noUnsuccessfulDeliveriesSinceLastSuccessful = 0;
			// Clear state maps
			lastSeqNoReceivedFromNode.clear();
			inWindowOfBeaconSeqNos.clear();
			previousInLqs.clear();
			previousEtxs.clear();
			outWindowOfMessagesSent.clear();
			
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

void MmbcrLinkEstimator::updateIncomingLinkQuality(int nodeId, unsigned int seqNo)
{ 
	double newInLq;
	int numberBeaconsBroadcast;

	// First check if the last stored seqNo (if any) is greater than the one we have just received from the node	
	if(!inWindowOfBeaconSeqNos[nodeId].empty() && inWindowOfBeaconSeqNos[nodeId].front() >= seqNo)
	{
		// If it is, this must mean the node which sent the beacon has restarted (restart causes seqNo to be reset)
		trace() << "WARNING - received beacon " << seqNo << " from node " << nodeId << " which is less than last known beacon number "
			<< inWindowOfBeaconSeqNos[nodeId].front() << ". This can happen if a neighbouring node has restarted, "
			<< "and its sequence number has restarted from zero. If a neighbour hasn't just restarted, something went wrong!";

		// Therefore, we should clear the old stored seqNos for this node
		inWindowOfBeaconSeqNos[nodeId].clear();
	}

	// Add the new beacon's sequence number to the specified node's window of stored beacons	
	inWindowOfBeaconSeqNos[nodeId].push_back(seqNo);

	// Is the beacon window now full?
	if(inWindowOfBeaconSeqNos[nodeId].size() >= inBeaconWindowSize)
	{
		// If it's full, we need to recalculate the specified node's incoming LQ

		// First calculate 'number of beacons received' / 'number of beacons broadcast'
		// Number of beacons broadcast is calculated as (difference between first and last beacon sequence numbers) + 1
		numberBeaconsBroadcast = (inWindowOfBeaconSeqNos[nodeId].back() - inWindowOfBeaconSeqNos[nodeId].front()) + 1;

		if(numberBeaconsBroadcast <= 0) {
			opp_error("Error calculating number of beacons broadcast - result was negative");
		}

		// The link quality is the expected number of transmissions (ETX). This is therefore 'number sent'/'number received':
		newInLq =  (double) numberBeaconsBroadcast / (double) inBeaconWindowSize; // inBeaconWindowSize is the number of beacons received in this window
		trace() << "Beacons broadcast (" << numberBeaconsBroadcast << ") / beacons received (" << inBeaconWindowSize << ") = " << newInLq;

		// Has there been a previously calculated incoming LQ for the specified node?
		if(previousInLqs.count(nodeId) > 0)
		{
			// If yes, we need to apply the exponential smoothing filter using previous value
			newInLq = (inLqSmoothingConst * newInLq) + ((1 - inLqSmoothingConst) * previousInLqs[nodeId]);
			trace() << "After smoothing: " << newInLq;
		}

		// Update ETX using the incoming link quality as the metric
		updateEtx(nodeId, newInLq);

		// Clear the window of beacons for the specified node ready for the next window
		inWindowOfBeaconSeqNos[nodeId].clear();
		// Update the 'previous' incoming LQ as this one
		previousInLqs[nodeId] = newInLq;
	}
}

void MmbcrLinkEstimator::updateOutgoingLinkQuality(int nodeId, bool wasAcked)
{
	double newOutLq;

	// Add the new message result to this node's window of stored beacons
	outWindowOfMessagesSent[nodeId].push_back((int) wasAcked); // true = 1, false = 0

	// Is the beacon window now full?
	if(outWindowOfMessagesSent[nodeId].size() >= outMessageWindowSize)
	{
		// If it's full, we need to recalculate the specified node's outgoing LQ
		// Count the number of true results (sum all the 1s).
		// note for accumulate: the 0 at the end is the initial sum value, and is used for the return type (int)
		int numberOfAckedMsgs = std::accumulate(outWindowOfMessagesSent[nodeId].begin(), outWindowOfMessagesSent[nodeId].end(), 0); 
		
		// If the number of ACKs is zero, the link quality is calculated as the number of unsuccessful delivery attempts 
		// since the last successful delivery
		if(numberOfAckedMsgs == 0)
		{
			noUnsuccessfulDeliveriesSinceLastSuccessful += outMessageWindowSize;
			newOutLq = noUnsuccessfulDeliveriesSinceLastSuccessful;
			trace() << "No ACKs received in window so using accumulated number of unsuccessful deliveries as LQ ("
				<< noUnsuccessfulDeliveriesSinceLastSuccessful << ")";
		}
		else
		{
			// Reset the unsuccessful delivery counter
			noUnsuccessfulDeliveriesSinceLastSuccessful = 0;

			// The link quality is the expected number of transmissions (ETX). This is therefore 'number sent'/'number received'
			// outMessageWindowSize = number sent in this window
			newOutLq = (double) outMessageWindowSize / (double) numberOfAckedMsgs;
			trace() << "Msgs sent (" << outMessageWindowSize << ") / msgs ACKed (" << numberOfAckedMsgs << ") = " << newOutLq;
		}

		// Update ETX using the outgoing link quality as the metric
		updateEtx(nodeId, newOutLq);

		// Clear the window of messages sent for the specified node ready for the next window
		outWindowOfMessagesSent[nodeId].clear();
	}
}

void MmbcrLinkEstimator::updateEtx(int nodeId, double newEtx)
{
	// Has there been a previously calculated ETX for this node (using either incoming our outgoing LQ)?
	if(previousEtxs.count(nodeId) > 0)
	{
		// If yes, we need to apply exponential smoothing using previous ETX
		newEtx = (etxSmoothingConst * newEtx) + ((1 - etxSmoothingConst) * previousEtxs[nodeId]);
		trace() << "New (smoothed) ETX for node " << nodeId << ": " << newEtx;
	}
	else
	{
		trace() << "New ETX for node " << nodeId << ": " << newEtx;
	}

	// Collect stats
	collectHistogram(OUTPUT_LINK_QUALITY, newEtx);

	// send the new ETX to the routing table manager
	MmbcrTableManagerControlMessage *updateMsg = new MmbcrTableManagerControlMessage("Update node ETX message", TABLE_MANAGER_CONTROL_COMMAND);
	updateMsg->setTableManagerControlMessageKind(MMBCR_TABLE_MANAGER_UPDATE_NODE_ETX);
	updateMsg->setNodeId(nodeId);
	updateMsg->setValue(newEtx);
	send(updateMsg, "toTableManager");

	// Update the 'previous' ETX as this one
	previousEtxs[nodeId] = newEtx;
}


void MmbcrLinkEstimator::timerFiredCallback(int index)
{
	switch(index) {

		default: {
			opp_error("Unknown timer type");
		}
	}
}

void MmbcrLinkEstimator::finishSpecific()
{
	
}