#include "StaticRouting.h"

Define_Module(StaticRouting);

void StaticRouting::startup()
{
	trace() << "Startup";

	if(hasPar("routeToNode"))
	{
		routeToNode = par("routeToNode").stringValue();
	}
	else
	{
		opp_error("Need to set a route destination for every node in static routing");
	}

	implementRetries = par("implementRetries");
	staticRoutingFrameSizeBits = par("staticRoutingFrameSizeBits");
	maxPacketSendRetries = par("maxPacketSendRetries");
	currentPacketSendingAttempts = 0;
	isSending = false;
}

void StaticRouting::handleOutOfEnergy(cMessage *outOfEnergyMsg)
{
	// We need to simulate what happens when a node runs out of energy - all state will be lost
	
	// It's unlikely but possible that a node may set a timer, then run out of energy,
	// then restart, and pick up the expired timer message from before shutting down. So
	// just in case, cancel all pending timers 
	cancelAllTimers();

	// Empty buffers
	pktHistory.clear();
	clearPacketBuffer();

	// DO NOT RESET THE PACKET SEQUENCE NUMBER - OTHERWISE DUPLICATE PACKET CHECKING WILL ERRONEOUSLY DISCARD PACKETS WHEN NODES RESTART

	// No need to reinitialise private variables set in startup() - startup will be called when node restarts

	cancelAndDelete(outOfEnergyMsg);
}

void StaticRouting::fromApplicationLayer(cPacket * pkt, const char *destination)
{
	trace() << "fromApplicationLayer";
	StaticRoutingPacket *netPacket = new StaticRoutingPacket("StaticRouting packet", NETWORK_LAYER_PACKET);
	// Important: set bit length before encapsulation
	netPacket->setBitLength(staticRoutingFrameSizeBits);
	encapsulatePacket(netPacket, pkt);
	netPacket->setSource(SELF_NETWORK_ADDRESS);
	netPacket->setOrigin(SELF_NETWORK_ADDRESS);
	netPacket->setDestination(routeToNode.c_str());

	// Buffer the packet
	bufferPacket(netPacket);

	// Start sending packets
	// Check if not already sending to avoid conflicting with current send (e.g. we may be waiting for a reply / retransmission of a previous packet)
	if(!isSending)
	{
		sendPackets();
	}
}

void StaticRouting::sendPackets()
{
	if(TXBuffer.size() > 0)
	{
		isSending = true;

		if(implementRetries)
		{
			if(currentPacketSendingAttempts < maxPacketSendRetries)
			{
				
				// Send the packet to the MAC layer. Send a duplicate because we hold the
				// message in the buffer in case we need to retry
				//trace() << "Transmission attempt number " << currentPacketSendingAttempts;
				plotTrace() << "#ROU_SEND";
				StaticRoutingPacket *networkPacket = check_and_cast<StaticRoutingPacket*>(TXBuffer.front()->dup());
				trace() << "Sending packet (attempt number " << currentPacketSendingAttempts << ", " <<
					"sequenceNo " << networkPacket->getSequenceNumber() <<
					" to node " << routeToNode.c_str();

				// Application data packets are routed via our parent, so set the latest known parent as the MAC layer next hop destination
				toMacLayer(networkPacket, resolveNetworkAddress(routeToNode.c_str()));
			}
			else // Max number of retries attempted - drop the packet
			{
				trace() << "Reached maximum retries for transmitting packet (" << maxPacketSendRetries << "), dropping packet";
				// Remove from the buffer
				cancelAndDelete(TXBuffer.front());
				TXBuffer.pop();
				// Reset sending attempts counter 
				currentPacketSendingAttempts = 0;

				// Recall send packets in case we have more in the buffer to send
				sendPackets();
			}
		}
		else
		{
			// No retries. Just send the packet. Don't take a duplicate from the buffer - only sending once.
			plotTrace() << "#ROU_SEND";
			StaticRoutingPacket *networkPacket = check_and_cast<StaticRoutingPacket*>(TXBuffer.front());
			TXBuffer.pop();
			trace() << "Sending packet sequenceNo " << networkPacket->getSequenceNumber() <<
					" to node " << routeToNode.c_str();
			toMacLayer(networkPacket, resolveNetworkAddress(routeToNode.c_str()));

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


void StaticRouting::fromMacLayer(cPacket * pkt, int srcMacAddress, double rssi, double lqi)
{
	StaticRoutingPacket *receivedPacket = dynamic_cast <StaticRoutingPacket*>(pkt);
	if (receivedPacket) 
	{
		//If addressed to this node, and not a duplicate
		if(strcmp(receivedPacket->getDestination(), SELF_NETWORK_ADDRESS) == 0)
		{
			if(isNotDuplicatePacket(receivedPacket))
			{
				// If this is sink node
				if(strcmp(SELF_NETWORK_ADDRESS, "0") == 0)
				{
					toApplicationLayer(decapsulatePacket(receivedPacket));
				}
				else
				{
					trace() << "Received packet from node " << srcMacAddress << " origin " << receivedPacket->getOrigin() 
						<< ", forwarding to " << routeToNode.c_str();
					// Take a duplciate because original will be deleted
					StaticRoutingPacket *packetToForward = receivedPacket->dup();
					packetToForward->setSource(SELF_NETWORK_ADDRESS);
					packetToForward->setDestination(routeToNode.c_str());

					bufferPacket(packetToForward);
			
					// Initiate packet sending
					if(!isSending)
					{
						sendPackets();
					}
				}
			}
			else
			{
				trace() << "Discarding duplicate packet from " << receivedPacket->getSource() << " origin " 
					<< receivedPacket->getOrigin() << " sequence no " << receivedPacket->getSequenceNumber();
			}
		}
		else
		{
			trace() << "Ignoring packet from " << receivedPacket->getSource() << " origin " << receivedPacket->getOrigin() 
				<< " not addressed to us, addressed to " << receivedPacket->getDestination();
		}
	}
	else
	{
		opp_error("Failed to cast routing packet");
	}
}


void StaticRouting::handleNetworkControlCommand(cMessage *msg) 
{ 
	switch(msg->getKind()) 
	{
		case NETWORK_CONTROL_COMMAND: 
		{
			RoutingControlMessage *controlMsg = check_and_cast<RoutingControlMessage*>(msg);

			switch(controlMsg->getRoutingControlMessageKind()) {

				case ROUTING_MSG_MAC_SENDING_ACKED: {

					trace() << "Message ACKed";
					// Message sending succeeded

					// If we're implementing retires
					if(implementRetries)
					{
						// Remove the current message from the TX buffer
						// (if we are not implementing retires, the packet is already taken out
						// of the buffer, since we are only sending to MAC layer once)
						if(TXBuffer.size() > 0)
						{
							cancelAndDelete(TXBuffer.front());
							TXBuffer.pop();
						}

						// Reset the number of retries
						currentPacketSendingAttempts = 0;
					}
					
					//plotTrace() << "#ROU_DATA_SEND_ACKED";

					// Call send packets again in case there are more packets to send in buffer
					sendPackets();

					break;
				}

				case ROUTING_MSG_MAC_SENDING_FAILED_NO_ACK: {

					// Message sending failed.
					trace() << "Message not ACKed";

					if(implementRetries)
					{
						// Increment the number of retries
						currentPacketSendingAttempts++;
						trace() << "Sending attempts counter incremented to " << currentPacketSendingAttempts;
					}
					
					//plotTrace() << "#ROU_DATA_SEND_NOT_ACKED";
					
					// Attempt to send again
					sendPackets();

					break;
				}

				case ROUTING_MSG_SINK_NODE_UPDATE:
				{
					// Ignore this - using static routing so no need for update from app layer
					cancelAndDelete(controlMsg);
					break;
				}

				default: {
					opp_error("Unknown network control message type");
				}
			}

			break;
		}

		default: {
			opp_error("Expected network control command");
		}
	}
}