#include "CtpRoutingController.h"

// Register the module in Omnet++
Define_Module(CtpRoutingController);

const char * CtpRoutingController::OUTPUT_CTP_DROPPED_AFTER_MAX_RETRIES = "CtpRouting dropped packet after max retries";
const char * CtpRoutingController::OUTPUT_CTP_DROPPED_OUT_OF_ENERGY = "CtpRouting dropped packet from buffer out of energy";
const char * CtpRoutingController::OUTPUT_CTP_SENDING_RETRY = "CtpRouting retrying send";
const char * CtpRoutingController::OUTPUT_CTP_FORWARDING = "CtpRouting received packet for forwarding";
const char * CtpRoutingController::OUTPUT_CTP_HOP_COUNT = "CtpRouting hop count";

void CtpRoutingController::startup()
{
	// Check if we have already started up once already
	if(!hasStartedUpOnce)
	{
		// These things should only be done once

		// Store NED parameters
		maxPacketSendRetries = par("maxPacketSendRetries");
		repairLoopWaitTimeMin = par("repairLoopWaitTimeMin");
		repairLoopWaitTimeRange = par("repairLoopWaitTimeMax").doubleValue() - par("repairLoopWaitTimeMin").doubleValue();
		implementRetries = par("implementRetries");
		delayBeforeRouteDiscoveryPropagation = par("delayBeforeRouteDiscoveryPropagation");

		// The net data frame overhead is in the containing compound module, so it can be defined in the base module
		// and used in other routing modules
		networkDataFrameOverheadBits = getParentModule()->par("networkDataFrameOverheadBits");

		// Declare stats outputs
		declareOutput(OUTPUT_CTP_DROPPED_AFTER_MAX_RETRIES);
		declareOutput(OUTPUT_CTP_DROPPED_OUT_OF_ENERGY);
		declareOutput(OUTPUT_CTP_SENDING_RETRY);
		declareOutput(OUTPUT_CTP_FORWARDING);
		declareHistogram(OUTPUT_CTP_HOP_COUNT, 1, 10, 10);

		hasStartedUpOnce = true;
	}

	// The following may be done multiple times - each time the node runs out of energy and then restarts after energy harvesting

	// Initialise private variables
	isSink = false;
	isSending = false;
	waitingForLoopRepair = false;
	currentParentNodeId = -1; 		// -1 indicates invalid / no parent
	currentMultihopEtxToRoot = -1; 	// -1 indicates invalid / no parent
	currentPacketSendingAttempts = 0;

	// An optional delay before initiating route discovery and propogation with beacons
	// May be useful, e.g. to enable time / beacon synced MAC layers to settle 
	if (delayBeforeRouteDiscoveryPropagation == 0)
	{
		initiateRouteDiscoveryAndPropagation();
	}
	else
	{
		setTimer(CTP_ROUTING_CONTROLLER_TIMER_DELAY_ROUTE_DISCOVERY_PROPAGATION, delayBeforeRouteDiscoveryPropagation);
	}
}

void CtpRoutingController::initiateRouteDiscoveryAndPropagation()
{
	// Initialise the beacon sender: this resets the beacon sending interval,
	// and will also include a beacon with pull flag set to true which indicates to
	// neighbous that we want to quickly receive up to date routing info
	// (this is useful if a node startup is delayed with respect to the rest of the network,
	// which may be mature and established and therefore sending beacons very infrequently)
	BeaconSenderControlMessage *resetTrickleMsg = new BeaconSenderControlMessage("Reset trickle and pull message", BEACON_SENDER_CONTROL_COMMAND);
	resetTrickleMsg->setBeaconSenderControlMessageKind(BEACON_SENDER_RESET_TRICKLE_AND_PULL);
	send(resetTrickleMsg, "toBeaconSender");
}

void CtpRoutingController::handleOutOfEnergy(cMessage *outOfEnergyMsg)
{
	// We need to simulate what happens when a node runs out of energy - all state will be lost
	
	// It's unlikely but possible that a node may set a timer, then run out of energy,
	// then restart, and pick up the expired timer message from before shutting down. So
	// just in case, cancel all pending timers 
	cancelAllTimers();

	// Empty buffers
	clearDuplicateBuffer();
	
	// Note: not calling VirtualRouting's emptyPacketBuffer function because instead
	// we want to do it manually here, so we can count the number of packets being discarded when
	// out of energy
	cPacket *pkt;
	while (!TXBuffer.empty()) {
		pkt = TXBuffer.front();
		TXBuffer.pop();
		cancelAndDelete(pkt);
		collectOutput(OUTPUT_CTP_DROPPED_OUT_OF_ENERGY);
	}

	// DO NOT RESET THE PACKET SEQUENCE NUMBER - OTHERWISE DUPLICATE PACKET CHECKING WILL ERRONEOUSLY DISCARD PACKETS WHEN NODES RESTART

	// No need to reinitialise private variables set in startup() - startup will be called when node restarts

	// Signal to sub-modules that we are out of energy
	send(outOfEnergyMsg->dup(), "toBeaconSender");
	send(outOfEnergyMsg->dup(), "toLinkEstimator");
	send(outOfEnergyMsg->dup(), "toTableManager");
	cancelAndDelete(outOfEnergyMsg);
}

void CtpRoutingController::fromApplicationLayer(cPacket *pkt, const char *destination)
{
	// We are only handling the case where application specifies sink node as destination.
	// Also we don't expect the sink to send application data packets.
	if(atoi(destination) != sinkNodeId || isSink) {
		opp_error("CTP expects apps to specify destination (%s) as sink node, and sink nodes shouldn't send app packets", destination);
	}

	trace() << "Received packet from application layer. Current parent is " << currentParentNodeId; 

	// Create the packet
	CtpRoutingPacket *networkPacket = new CtpRoutingPacket("CTP routing data packet", NETWORK_LAYER_PACKET);
	// Important: set bit length before encapsulation
	networkPacket->setBitLength(networkDataFrameOverheadBits);
	// FIRST encapsulate
	encapsulatePacket(networkPacket, pkt);
	// THEN set packet fields (encapsulate function sets some fields, e.g. sequenceNumber)
	networkPacket->setRoutingPacketKind(CTP_ROUTING_PACKET_TYPE_DATA);
	networkPacket->setSource(SELF_NETWORK_ADDRESS);
	networkPacket->setOrigin(SELF_NETWORK_ADDRESS);
	networkPacket->setHopCount(0);
	// Set the multihop ETX of this node, for use in detecting routing loops:
	networkPacket->setMultihopEtxToRoot(currentMultihopEtxToRoot);

	// The network level destination is the ultimate destination, for application packets this is the sink
	// This is different from the pkt->getNetMacInfoExchange().nextHop which is the MAC level next hop destination, set in the sendPackets function
	networkPacket->setDestination(destination); 
	

	// Buffer the packet
	bufferPacket(networkPacket);

	// Start sending packets
	// Check if not already sending to avoid conflicting with current send (e.g. we may be waiting for a reply / retransmission of a previous packet)
	if(!isSending)
	{
		sendPackets();
	}
}

void CtpRoutingController::sendPackets()
{
	// First check that we have a valid parent
	if(currentParentNodeId == -1)
	{
		trace() << "No valid parent to send data to. Packet will be kept in buffer and retried if we get a parent update.";
		return;
	}

	// We also must not send packets while waiting for a loop repair
	// (sendPackets will be recalled after repair loop)
	if(waitingForLoopRepair)
	{
		trace() << "Blocking packet sending because in repair loop procedure";
		return;
	}

	if(TXBuffer.size() > 0)
	{
		if(implementRetries)
		{
			if(currentPacketSendingAttempts < maxPacketSendRetries)
			{
				isSending = true;

				// Send the packet to the MAC layer. Send a duplicate because we hold the
				// message in the buffer in case we need to retry
				//trace() << "Transmission attempt number " << currentPacketSendingAttempts;
				plotTrace() << "#ROU_SEND";
				CtpRoutingPacket *networkPacket = check_and_cast<CtpRoutingPacket*>(TXBuffer.front()->dup());
				trace() << "Sending packet (attempt number " << currentPacketSendingAttempts << "): origin " << networkPacket->getOrigin() << ", " <<
					"sequenceNo " << networkPacket->getSequenceNumber() << ", " <<
					"hop count " <<networkPacket->getHopCount() << " " <<
					"to current parent " << currentParentNodeId;

				// Application data packets are routed via our parent, so set the latest known parent as the MAC layer next hop destination
				toMacLayer(networkPacket, currentParentNodeId);

			}
			else // Max number of retries attempted - drop the packet
			{
				trace() << "Reached maximum retries for transmitting packet (" << maxPacketSendRetries << "), dropping packet";
				// Remove from the buffer
				cancelAndDelete(TXBuffer.front());
				TXBuffer.pop();
				// Reset sending attempts counter 
				currentPacketSendingAttempts = 0;
				// Collect stats
				collectOutput(OUTPUT_CTP_DROPPED_AFTER_MAX_RETRIES);
				
				// Recall send packets in case we have more in the buffer to send
				sendPackets();
			}
		}
		else
		{
			// No retries. Just send the packet. Don't take a duplicate from the buffer - only sending once.
			plotTrace() << "#ROU_SEND";
			CtpRoutingPacket *networkPacket = check_and_cast<CtpRoutingPacket*>(TXBuffer.front());
			TXBuffer.pop();
			trace() << "Sending packet (attempt number " << currentPacketSendingAttempts << "): origin " << networkPacket->getOrigin() << ", " <<
					"sequenceNo " << networkPacket->getSequenceNumber() << ", " <<
					"hop count " <<networkPacket->getHopCount() << " " <<
					"to current parent " << currentParentNodeId;
			toMacLayer(networkPacket, currentParentNodeId);

			// Recall send packets in case we have more in the buffer to send
			sendPackets();
		}
	}
	else
	{
		// Finished sending
		isSending = false;
	}
}

void CtpRoutingController::fromMacLayer(cPacket *pkt, int srcMacAddress, double rssi, double lqi)
{
	CtpRoutingPacket *ctpPkt = check_and_cast<CtpRoutingPacket*>(pkt);

	if(!ctpPkt) {
		opp_error("Expected CTP routing packet");
	}

	// Update the hop count
	ctpPkt->setHopCount(ctpPkt->getHopCount() + 1);

	if(isDuplicate(ctpPkt))
	{
		// Note: no need to delete the message - this will be done by VirtualMac
		return;
	}

	switch(ctpPkt->getRoutingPacketKind()){

		case CTP_ROUTING_PACKET_TYPE_BEACON: {

			// We have recieved a beacon
			trace() << "Received beacon " << ctpPkt->getSequenceNumber() << " from node " << ctpPkt->getNetMacInfoExchange().lastHop;
			plotTrace() << "#ROU_REC_BEACON";

			// Send the beacon to link estimator so it can update incoming link quality and possibly notify table manager of updated multihop ETX to root
			// Sending a duplicate, because the original will be deleted by VirtualMac 
			send(ctpPkt->dup(), "toLinkEstimator");
			
			// Check for Pull flag - if set, tell beacon sender to reset trickle so that we update neighbouring nodes quickly
			if(ctpPkt->getPullFlag())
			{
				trace() << "Beacon contained pull flag - resetting trickle";
				plotTrace() << "#ROU_PULL_RECEIVED " << ctpPkt->getNetMacInfoExchange().lastHop;
				BeaconSenderControlMessage *resetTrickleMsg = new BeaconSenderControlMessage("Reset trickle message", BEACON_SENDER_CONTROL_COMMAND);
				resetTrickleMsg->setBeaconSenderControlMessageKind(BEACON_SENDER_RESET_TRICKLE);
				send(resetTrickleMsg, "toBeaconSender");
			}
			
			break;
		}

		case CTP_ROUTING_PACKET_TYPE_DATA: {
			// We have received a data packet 
			
			// Check if we were the intended next-hop destination from the sender
			if(ctpPkt->getNetMacInfoExchange().nextHop == self)
			{
				
				// If this is the sink node
				if(isSink)
				{
					// The packet has reached its ultimate destination
					// pass received data packet up to application layer
					trace() << "Sink received data packet from " << ctpPkt->getNetMacInfoExchange().lastHop << ", passing to application layer";
					toApplicationLayer(decapsulatePacket(ctpPkt));				
				
					collectHistogram(OUTPUT_CTP_HOP_COUNT, ctpPkt->getHopCount());			
				}
				// Otherwise, we need to forward packet onwards towards sink
				else
				{				
					plotTrace() << "#ROU_REC_MSG_TO_FORWARD";
					forwardPacket(ctpPkt);
				}
			}
			// Otherwise, we are not the intended next-hop destination. We are snooping on a packet not meant for us
			else
			{
				trace() << "Snooped packet not addressed to is (addressed to " << ctpPkt->getNetMacInfoExchange().nextHop << ")";
			
				// TODO: scan for pull request
			}
			
			break;
		}

		default: {
			opp_error("Unexpected CTP routing packet type");
		}
	}
}

void CtpRoutingController::forwardPacket(CtpRoutingPacket *pkt)
{
	trace() << "Forwarding packet number " << pkt->getSequenceNumber() << " from " << pkt->getNetMacInfoExchange().lastHop <<
	 	", origin " << pkt->getOrigin();
	collectOutput(OUTPUT_CTP_FORWARDING);

	// Set this node as the source (origin address remains unchanged - origin is the original sender)
	pkt->setSource(SELF_NETWORK_ADDRESS);

	// Check for routing loops
	// Because multihop ETX is an additive route metric, which increases by at least 1 each hop,
	// and we receive packets for forwarding from our parent, the sender's multihop ETX (recorded 
	// in the packet) must strictly be higher than ours
	// If it is lower (or equal) this would indicate a routing loop
	// Exception in the case of the sender's multihop ETX being -1, this means invalid (it has not yet updated registered a valid route)
	if(pkt->getMultihopEtxToRoot() != -1 && pkt->getMultihopEtxToRoot() <= currentMultihopEtxToRoot)
	{
		trace() << "WARNING - routing loop detected! Node's MH-EHX is " << currentMultihopEtxToRoot
			<< ", sending node " << pkt->getNetMacInfoExchange().lastHop << " MH-ETX is " << pkt->getMultihopEtxToRoot() << " - Initiating routing loop repair";
		plotTrace() << "#ROU_LOOP_DETECTED";
		
		// Buffer the packet for re-sending later after the loop repair procedure has completed.
		// We take a duplicate because the original will be deleted by VirtualMac
		bufferPacket(pkt->dup());

		// Initialte the loop repair procedure
		repairLoop();
		return;
	}
	else
	{
		// Update the multihop ETX on the packet to this node's multihop ETX, ready for reception at the next node
		pkt->setMultihopEtxToRoot(currentMultihopEtxToRoot);

		// Buffer the packet. We take a duplicate because the original will be deleted by VirtualMac
		bufferPacket(pkt->dup());
		
		// Initiate packet sending
		if(!isSending)
		{
			sendPackets();
		}
	}
}

void CtpRoutingController::repairLoop()
{
	// Do not send / forward data packets while we repair the loop. This is controlled using a flag:
	waitingForLoopRepair = true;

	// Reset the trickle algorithm in the beacon sender module - this will cause beacons to be sent out
	// to hopefully help repair the loop by advertising most recent routing info
	// Also request pull in order to quickly receive latest updated info from neighbours 
	BeaconSenderControlMessage *resetTrickleMsg = new BeaconSenderControlMessage("Reset trickle message", BEACON_SENDER_CONTROL_COMMAND);
	resetTrickleMsg->setBeaconSenderControlMessageKind(BEACON_SENDER_RESET_TRICKLE_AND_PULL);
	send(resetTrickleMsg, "toBeaconSender");

	// Set a timer for when to re-allow packet sending (hopefully the route loop will be repaired by then)
	double repairLoopWaitTime = repairLoopWaitTimeMin + dblrand() * repairLoopWaitTimeRange;
	setTimer(CTP_ROUTING_CONTROLLER_TIMER_LOOP_REPAIR, repairLoopWaitTime);
}


bool CtpRoutingController::isDuplicate(CtpRoutingPacket *pkt)
{
	// Not using VirtualMac isNotDuplicate function because we need to use
	// a sepcific definition of duplciate according to CTP spec
	// Duplicate is defined in CTP as same Origin, SeqNo and HopCount
	// We also compare routingPacketKind, as broadcast beacons and unicast data
	// have different runs of sequence number (so it would be possible for
	// a beacon and data packet from the same node to have the same seqNo) 

	//extract fields from packet
	ctpRoutingPacket_type type = (ctpRoutingPacket_type) pkt->getRoutingPacketKind();
	unsigned int seqNo = pkt->getSequenceNumber();
	int origin = std::stoi(pkt->getOrigin());
	unsigned int hopCount = pkt->getHopCount();

	//resize packet history vector if necessary
	if (origin >= (int)duplicateDetectionBuffer.size())
		duplicateDetectionBuffer.resize(origin + 1);

	//search for this packet from this origin in the list
	list<duplicatePacketBufferEntry>::iterator iter;
	for (iter = duplicateDetectionBuffer[origin].begin(); iter != duplicateDetectionBuffer[origin].end(); iter++) 
	{
		// Same type (beacon / data), seq no and hop count means it's the same
		// no need to check origin - using separate list in vector for each origin node 
		if((*iter).type == (ctpRoutingPacket_type) pkt->getRoutingPacketKind()
		&& (*iter).seqNo == pkt->getSequenceNumber()
		&& (*iter).hopCount == pkt->getHopCount())
		{
			// This is a duplicate.
			trace() << "Dropping duplicate packet type " << type << 
				" seqNo " << seqNo << 
				" from origin node " << origin <<
			 	", hop count " << hopCount;
			return true;
		}
	}

	// this is new packet, not a duplicate, add to duplicate buffer for this origin
	struct duplicatePacketBufferEntry entry;
	entry.type =  (ctpRoutingPacket_type) pkt->getRoutingPacketKind();
	entry.seqNo =  pkt->getSequenceNumber();
	entry.hopCount =  pkt->getHopCount();
	duplicateDetectionBuffer[origin].push_front(entry);

	// Rotate buffer
	if (duplicateDetectionBuffer[origin].size() > duplicateDetectionBufferSize)
		duplicateDetectionBuffer[origin].pop_back();

	return false;
}

void CtpRoutingController::clearDuplicateBuffer()
{
	// Delete any remiaining messages in the duplicate check buffer (avoids undisposed object warning)
	duplicateDetectionBuffer.clear();
}

void CtpRoutingController::handleMacControlMessage(cMessage *msg)
{
	// We don't handle any mac control messages in this routting module.
	// However we must ensure it is properly deleted
	cancelAndDelete(msg);
}

void CtpRoutingController::handleRadioControlMessage(cMessage *msg)
{
	// We don't handle any radio control messages in this routting module.
	// However we must ensure it is properly deleted
	cancelAndDelete(msg);
}

void CtpRoutingController::handleNetworkControlCommand(cMessage *msg) 
{ 
	switch(msg->getKind()) {
		case NETWORK_CONTROL_COMMAND: {
			RoutingControlMessage *controlMsg = check_and_cast<RoutingControlMessage*>(msg);

			switch(controlMsg->getRoutingControlMessageKind()) {

				case ROUTING_MSG_MAC_SENDING_ACKED: {

					trace() << "Message ACKed";
					// Message sending succeeded
					isSending = false;

					// If we're implementing retires
					if(implementRetries)
					{
						// Remove the current message from the TX buffer
						// (if we are not implementing retires, the packet is already taken out
						// of the buffer, since we are only sending to MAC layer once)
						cancelAndDelete(TXBuffer.front());
						TXBuffer.pop();

						// Reset the number of retries
						currentPacketSendingAttempts = 0;
					}
					
					//plotTrace() << "#ROU_DATA_SEND_ACKED";

					// Call send packets again in case there are more packets to send in buffer
					sendPackets();

					// Pass on to the link estimator the succeeded send message so it can update link quality estimate
					send(controlMsg, "toLinkEstimator");
					break;
				}

				case ROUTING_MSG_MAC_SENDING_FAILED_NO_ACK: {

					// Message sending failed.
					isSending = false;
					
					if(implementRetries)
					{
						// Increment the number of retries
						currentPacketSendingAttempts++;
						trace() << "Message not ACKed. Sending attempts counter incremented to " << currentPacketSendingAttempts;

						collectOutput(OUTPUT_CTP_SENDING_RETRY);
					}
					
					//plotTrace() << "#ROU_DATA_SEND_NOT_ACKED";
					
					// Attempt to send again
					sendPackets();

					// Pass on to the link estimator the failed send message so it can update link quality estimate
					send(controlMsg, "toLinkEstimator");
					break;
				}

				// The application layer is updating the network layer on what the sink node ID is
				case ROUTING_MSG_SINK_NODE_UPDATE: {

					sinkNodeId = controlMsg->getValue();
					if(sinkNodeId == self) {
						trace() << "This node is sink";
						isSink = true;
						// If this is sink, set the parent node ID to itself
						currentParentNodeId = self;
					}

					// Pass a copy of this update on to the table manager
					send(controlMsg->dup(), "toTableManager");
					cancelAndDelete(controlMsg);
					break;
				}

				default: {
					opp_error("Unknown network control message type");
				}
			}

			break;
		}

		case CTP_NETWORK_CONTROL_COMMAND:
		{
			CtpRoutingControlMessage *controlMsg = check_and_cast<CtpRoutingControlMessage*>(msg);

			switch(controlMsg->getCtpRoutingControlMessageKind()) 
			{
				// The table manager is updating the controller of the latest chosen parent node ID and multihop ETX
				case CTP_ROUTING_MSG_UPDATE_ROUTE_INFO: {
					
					// Update our stored parent node ID
					currentParentNodeId = controlMsg->getValue();
					trace() << "Parent Node ID is " << currentParentNodeId;
					// Update our stored multihop ETX for routing loop detection
					currentMultihopEtxToRoot = controlMsg->getMultihopEtx();
					trace() << "Multihop ETX is " << currentMultihopEtxToRoot;
					
					// In case we have packets queued from earlier because we previously had no valid parent, initiate send packets
					// Check if not already sending to avoid conflicting with current send
					if(!isSending)
					{
						sendPackets();
					}
					cancelAndDelete(controlMsg);
					break;
				}

				default: {
					opp_error("Unknown CTP network control message type");
				}
			}
			break;
		}

		default: {
			opp_error("Expected network control command");
		}
	}
}

void CtpRoutingController::timerFiredCallback(int index)
{
	switch(index)
	{
		case CTP_ROUTING_CONTROLLER_TIMER_LOOP_REPAIR: {
			
			trace() << "Routing loop repair wait period complete";
			// We have waited long enough for the loop to hopefully have been repaired.
			// Allow for packets to be sent by resetting flag.
			waitingForLoopRepair = false;

			// Send any packets that have been buffered while waiting			
			if(!isSending)
			{
				sendPackets();
			}
			break;
		}

		case CTP_ROUTING_CONTROLLER_TIMER_DELAY_ROUTE_DISCOVERY_PROPAGATION: {
			initiateRouteDiscoveryAndPropagation();
			break;
		}

		default: {
			opp_error("Unknown timer type");
		}
	}
}

void CtpRoutingController::finishSpecific()
{
	clearDuplicateBuffer();
}