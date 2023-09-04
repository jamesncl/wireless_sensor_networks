#include "RicerMac.h"

Define_Module(RicerMac);

void RicerMac::startup()
{
	trace() << "Startup";
	
	if(!hasStartedUpOnce)
	{
		hasStartedUpOnce = true;
		
		declareOutput("Ricer dropped packet");
		declareOutput("Ricer buffer overflow");
		declareOutput("Ricer packets left in buffer");
		declareOutput("Ricer CCA clear for data");
		declareOutput("Ricer CCA busy for data");
		declareOutput("Ricer CCA clear for RTR");
		declareOutput("Ricer CCA busy for RTR");
		declareOutput("Ricer wakeup");
		declareOutput("Ricer send packet breakdown");
		declareOutput("Ricer received packet breakdown");
		declareOutput("Ricer sent RTR but no data");
		declareOutput("Ricer sent RTR and received data");
		declareOutput("Ricer overheard packet");
		declareOutput("Ricer packet not ACKed");
		declareOutput("Ricer packet ACKed");
		declareOutput("Ricer sleep time");
		declareOutput("Ricer wait to send time");

		macParameters.waitForRxTransitionDelayTime = par("waitForRxTransitionDelayTime");
		macParameters.waitForSleepTransitionDelayTime = par("waitForSleepTransitionDelayTime");
		macParameters.binaryExponentialBackoffSlotDuration = par("binaryExponentialBackoffSlotDuration");
		macParameters.binaryExponentialBackoffMaxExponent = par("binaryExponentialBackoffMaxExponent");
		macParameters.sendDataBackoffMin = par("sendDataBackoffMin");
		macParameters.sendDataBackoffMax = par("sendDataBackoffMax");
		macParameters.phyDataRate = par("phyDataRate");
		macParameters.macBufferSize = par("macBufferSize");
		macParameters.wakeForReceiveInterval = par("wakeForReceiveInterval");
		macParameters.wakeForReceiveIntervalJitter = par("wakeForReceiveIntervalJitter");
		macParameters.maxSendRetries = par("maxSendRetries");
		macParameters.waitforRadioTxCompleteAfterInvalidCcaResult = par("waitforRadioTxCompleteAfterInvalidCcaResult");
		macParameters.selfNodeId = self;
		macParameters.ricerAckRtrFrameSizeBits = par("ricerAckRtrFrameSizeBits");
		macParameters.ricerRtrFrameSizeBits = par("ricerRtrFrameSizeBits");
		macParameters.ricerDataFrameSizeBits = par("ricerDataFrameSizeBits");
		macParameters.waitForDataAndAckResponseMultiplier = par("waitForDataAndAckResponseMultiplier");

		// Get packet overheads for all other layers - we need this so we can predict how long
		// a transmission will last (used when determining how long to wait for data after sending RTR) 
		macParameters.phyFrameOverheadBytes = getParentModule() // Communication module
			->getSubmodule("Radio")->par("phyFrameOverhead");

		macParameters.networkDataFrameOverheadBits = getParentModule() // Communication module
			->getSubmodule("Routing")->par("networkDataFrameOverheadBits");

		macParameters.applicationPacketOverheadBytes = getParentModule() // Communication module
			->getParentModule() // Node module
			->getSubmodule("Application")->par("packetHeaderOverhead");

	}
	

	macContext.initialiseContext(this, macParameters);
	macContext.startup();
}

void RicerMac::setRadioState(BasicState_type radioState)
{
	RadioControlCommand *cmd = new RadioControlCommand("Radio control command", RADIO_CONTROL_COMMAND);
	cmd->setRadioControlCommandKind(SET_STATE);
	cmd->setState(radioState);
	send(cmd, "toRadioModule");
}

CCA_result RicerMac::getCcaResultFromRadio()
{
	return radioModule->isChannelClear();
}

void RicerMac::startTimer(RicerMacTimer timer, double timerDuration)
{
	if(isTimerRunning(timer))
	{
		opp_error("Asked to start timer which is already running");
	}

	//trace() << "Setting timer for " << timerDuration;
	setTimer(timer, timerDuration);
}

void RicerMac::stopTimer(RicerMacTimer timer)
{
	//log("Stopping timer");
	cancelTimer(timer);
}

bool RicerMac::isTimerRunning(RicerMacTimer timer)
{
	return getTimer(timer) != -1;
}

void RicerMac::pauseTimer(RicerMacTimer timer)
{
	// Arrival time is the future simualtion time at which the timer will expire
	simtime_t timerArrivalTime = getTimer(timer);

	// getTimer returns -1 if the timer is not runing
	if(timerArrivalTime == -1) 
	{
		opp_error("Attempted to pause timer which is not running");
	}
	else
	{
		// Work out how much time is left until the timer fires
		simtime_t timerTimeLeft = timerArrivalTime - getClock();
		
		if(timerTimeLeft < 0) {
			opp_error("Negative time left on timer, something went wrong! Timer time was %d, time now (including drift) was %d, difference is %d",
				timerArrivalTime.dbl(),
				getClock().dbl(),
				timerTimeLeft.dbl());
		}

		// We can't really pause the timer, we have to cancel it
		cancelTimer(timer);
		// and store in the pausedTimers map so we can resume (reschedule) later when asked to
		pausedTimers[timer] = timerTimeLeft.dbl();

		//trace() << "Paused timer, time left on timer " << std::to_string(timerTimeLeft.dbl());
	}
}

void RicerMac::resumeTimer(RicerMacTimer timer)
{
	auto searchPausedTimers = pausedTimers.find(timer);
    if(searchPausedTimers == pausedTimers.end())
	{
		opp_error("Attempted to resume timer which is not currently paused");
	}
	else
	{
		double pausedTimeRemaining = searchPausedTimers->second;
		setTimer(timer, pausedTimeRemaining);
		//trace() << "Resuming timer with " << pausedTimeRemaining << " seconds remaining";
	}
}

bool RicerMac::isTimerPaused(RicerMacTimer timer)
{
	auto searchPausedTimers = pausedTimers.find(timer);
    if(searchPausedTimers == pausedTimers.end())
	{
		return false;
	}
	else
	{
		return true;
	}
}

void RicerMac::removePausedTimer(RicerMacTimer timer)
{
	if(isTimerPaused(timer))
	{
		pausedTimers.erase(timer);
		//trace() << "Removed (deleted) paused timer";
	}
	else
	{
		opp_error("Asked to remove a paused timer but timer not found in paused list");
	}
}

double RicerMac::getTimerTimeLeft(RicerMacTimer timer)
{
	simtime_t timerArrivalTime = getTimer(timer);
	if(timerArrivalTime == -1)
	{
		opp_error("Asked to get timer time left on a timer which is not running");
	}
	simtime_t timeLeft = timerArrivalTime - getClock();
	return timeLeft.dbl();
}

void RicerMac::timerFiredCallback(int index)
{
	macContext.timerFired(static_cast<RicerMacTimer>(index));
}

void RicerMac::fromNetworkLayer(cPacket *netPkt, int destination)
{
	trace() << "Received packet from network layer to send";
	RicerMacPacket *macPacket = new RicerMacPacket("Ricer mac packet", MAC_LAYER_PACKET);
	// Important: set bit length before encapsulation
	macPacket->setBitLength(macParameters.ricerDataFrameSizeBits);
	// Important: *FIRST* encapsulate the packet THEN set its dest and source
	// (encapsulate sets the destination to default broadcast)
	// encapusulate also increments / sets the sequence number
	encapsulatePacket(macPacket, netPkt);
	macPacket->setSource(SELF_MAC_ADDRESS);
	macPacket->setDestination(destination);
	macPacket->setFrameType(RICER_MAC_FRAME_TYPE_DATA);

	// Note: destination may be BROADCAST_MAC_ADDRESS
	// However, in this MAC, broadcasts are handled as a series of unicasts to any node which sends a RTR beacon.
	// Therefore, when sending a broadcast data packet (as a unicast) we need to set the
	// isDataForBroadcast flag to true, so that the receiving node is able to tell that it was sent broadcast
	// Note: RTR and ACK/RTR beacons are ALWAYS broadcast so flag is not used for these packet types
	if(destination == BROADCAST_MAC_ADDRESS)
	{
		macPacket->setIsDataForBroadcast(true);
	}
	else
	{
		macPacket->setIsDataForBroadcast(false);
	}

	if(!macContext.bufferPacketFromNetLayer(macPacket))
	{
		trace() << "WARNING - Unable to buffer packet, so dropping";
		cancelAndDelete(macPacket);
	}
}

void RicerMac::fromRadioLayer(cPacket *pkt, double rssi, double lqi)
{
	RicerMacPacket *ricerMacPacket = check_and_cast <RicerMacPacket*>(pkt);

	switch(ricerMacPacket->getFrameType())
	{
		case RICER_MAC_FRAME_TYPE_RTR_BEACON:
		{
			trace() << "Received RTR beacon from radio layer from node " << ricerMacPacket->getSource();
			plotTrace() << "#MAC_REC_RTR " << ricerMacPacket->getSource();
			break;
		}
		case RICER_MAC_FRAME_TYPE_ACK_RTR_BEACON:
		{
			trace() << "Received ACK/RTR beacon from radio layer from node " << ricerMacPacket->getSource();
			plotTrace() << "#MAC_REC_ACK_RTR " << ricerMacPacket->getSource();
			break;
		}
		case RICER_MAC_FRAME_TYPE_DATA:
		{
			trace() << "Received data packet from radio layer from node " << ricerMacPacket->getSource();
			if(ricerMacPacket->getDestination() == self)
			{
				if(ricerMacPacket->getIsDataForBroadcast())
				{
					plotTrace() << "#MAC_REC_DATA_BROADCAST_AS_UNICAST " << ricerMacPacket->getSource();
				}
				else
				{
					plotTrace() << "#MAC_REC_DATA_UNICAST " << ricerMacPacket->getSource();
				}
			}

			break;
		}
	}
	macContext.fromRadioLayer(ricerMacPacket);
}

void RicerMac::decapsulateAndPassToNetLayer(RicerMacPacket *packet)
{
	if(packet->getEncapsulatedPacket() == NULL)
	{
		opp_error("Asked to decapsulate mac packet and pass encapsulated net packet to net layer, but packet has no encapsulated packet");
	}

	trace() << "Passing packet to network layer";
	toNetworkLayer(decapsulatePacket(packet));
}

void RicerMac::sendReadyToReceiveBeacon()
{
	trace() << "Sending RTR beacon to radio.";
	plotTrace() << "#MAC_SEND_RTR";
	collectStats("Ricer send packet breakdown", "RTR");
	RicerMacPacket *readyToReceiveBeacon = new RicerMacPacket("RicerMac ready-to-receive beacon", MAC_LAYER_PACKET);
	readyToReceiveBeacon->setSource(self);
	readyToReceiveBeacon->setDestination(BROADCAST_MAC_ADDRESS);
	readyToReceiveBeacon->setFrameType(RICER_MAC_FRAME_TYPE_RTR_BEACON);
	readyToReceiveBeacon->setBitLength(macParameters.ricerRtrFrameSizeBits);

	toRadioLayer(readyToReceiveBeacon);
	// THEN we set to TX. After TXing all packets in buffer, it should automatically go back to previous state
	toRadioLayer(createRadioCommand(SET_STATE, TX));
}

void RicerMac::sendAckReadyToReceiveTo(int nodeIdToAck)
{
	trace() << "Sending ACK/RTR beacon to radio. ACK is in response to " << nodeIdToAck;
	plotTrace() << "#MAC_SEND_ACK_RTR " << nodeIdToAck;
	collectStats("Ricer send packet breakdown", "ACK/RTR");
	// This packet has two purposes -
	// 1) ACKnowledges a unicast packet received by this node from another node.
	//    The packet therefore contains the field ackForNode which is set the the
	//    node who sent the original unicast packet
	// 2) Is itself a further ready-to-receive beacon for subsequent transmissions
	//    (subsequent transimssions could be from any node)
	//    The packet is therefore addressed to BROADCAST_MAC_ADDRESS
	RicerMacPacket *ackAndReadyToReceiveBeacon = new RicerMacPacket("RicerMac ACK and ready-to-receive beacon", MAC_LAYER_PACKET);
	ackAndReadyToReceiveBeacon->setSource(self);
	ackAndReadyToReceiveBeacon->setFrameType(RICER_MAC_FRAME_TYPE_ACK_RTR_BEACON);
	ackAndReadyToReceiveBeacon->setDestination(BROADCAST_MAC_ADDRESS);
	ackAndReadyToReceiveBeacon->setAckForNode(nodeIdToAck);
	ackAndReadyToReceiveBeacon->setBitLength(macParameters.ricerAckRtrFrameSizeBits);

	toRadioLayer(ackAndReadyToReceiveBeacon);
	// THEN we set to TX. After TXing all packets in buffer, it should automatically go back to previous state
	toRadioLayer(createRadioCommand(SET_STATE, TX));
}

void RicerMac::sendData(RicerMacPacket* macPacket)
{
	trace() << "Sending data to radio to send to node " << macPacket->getDestination();

	if(macPacket->getIsDataForBroadcast())
	{
		plotTrace() << "#MAC_SEND_DATA_BROADCAST_AS_UNICAST " << macPacket->getDestination();
		collectStats("Ricer send packet breakdown", "data broadcast (as unicast)");
	}
	else
	{
		plotTrace() << "#MAC_SEND_DATA_UNICAST " << macPacket->getDestination();
		collectStats("Ricer send packet breakdown", "data unicast");
	}

	// Send to radio layer
	toRadioLayer(macPacket);
	// THEN we set to TX. After TXing all packets in buffer, it should automatically go back to previous state
	toRadioLayer(createRadioCommand(SET_STATE, TX));
}

void RicerMac::handleOutOfEnergy(cMessage *outOfEnergyMsg)
{
	trace() << "Out of energy!";
	macContext.clearAllState();
	cancelAllTimers();
	pausedTimers.clear();
	cancelAndDelete(outOfEnergyMsg);
}

int RicerMac::handleControlCommand(cMessage *msg)
{
	// if we return true (1), the message is not deleted.
	// if we return false (not 1) the message is deleted.
	return 0;
}

int RicerMac::handleRadioControlMessage(cMessage *msg)
{
	// if we return true (1), the message is not deleted.
	// if we return false (not 1) the message is deleted.
	return 0;
}

void RicerMac::log(std::string message)
{
	trace() << message;
}

void RicerMac::logPlotTrace(std::string message)
{
	plotTrace() << message;
}

void RicerMac::collectStats(const char *outputName)
{
	collectOutput(outputName);
}

void RicerMac::collectStats(const char *outputName, const char *outputLabel)
{
	collectOutput(outputName, outputLabel);
}

void RicerMac::collectStats(const char *outputName, const char *outputLabel, double value)
{
	collectOutput(outputName, outputLabel, value);
}

double RicerMac::getCurrentSimulationTime()
{
	return simTime().dbl();
}

void RicerMac::cleanUpAndRemoveMessage(cPacket *packet)
{
	cancelAndDelete(packet);
}

void RicerMac::reportSendingFailedToNode(int nodeIdSendFailedTo)
{
	trace() << "Passing sending failed control message to net layer";
	plotTrace() << "#MAC_UNICAST_FAILED";
	RoutingControlMessage *sendFailedMsg = new RoutingControlMessage("routing control msg", NETWORK_CONTROL_COMMAND);
	sendFailedMsg->setRoutingControlMessageKind(ROUTING_MSG_MAC_SENDING_FAILED_NO_ACK);
	sendFailedMsg->setValue(nodeIdSendFailedTo); // The value is the node ID of the node we were trying to send to
	toNetworkLayer(sendFailedMsg);
}

void RicerMac::reportSendingSucceededToNode(int nodeIdSentTo)
{
	trace() << "Passing sending succeeded control message to net layer";
	plotTrace() << "#MAC_UNICAST_SUCCEEDED";
	RoutingControlMessage *sendAckedMsg = new RoutingControlMessage("routing control msg", NETWORK_CONTROL_COMMAND);
	sendAckedMsg->setRoutingControlMessageKind(ROUTING_MSG_MAC_SENDING_ACKED);
	sendAckedMsg->setValue(nodeIdSentTo); // The value is the node ID of the node we were trying to send to
	toNetworkLayer(sendAckedMsg);
}

void RicerMac::finishSpecific()
{
	collectOutput("Ricer packets left in buffer", "Unicast", macContext.howManyUnicastPacketsInBuffer());
	collectOutput("Ricer packets left in buffer", "Broadcast", macContext.howManyBroadcastPacketsInBuffer());
	macContext.clearAllState();
}