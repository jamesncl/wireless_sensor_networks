#include "RicerStateSend.h"

/////////////////////////////////////////////////////////////////////
// Pure virtual member functions inherited from base RicerState class
/////////////////////////////////////////////////////////////////////

std::string RicerStateSend::getCurrentStateName()
{
	return "Send";
}

void RicerStateSend::start(RicerStateContextInterface *context, RicerMacInterface *moduleInterface)
{
	context->log("Entered state, backing off");

	if(context->getReceivedBeaconFromNodeToSendTo() == -1)
	{
		throw std::runtime_error("In Send state but node to send to hasn't been set");
	}

	// Calculate a single random backoff
	double backoffRange = context->getMacParameters().sendDataBackoffMax - context->getMacParameters().sendDataBackoffMin;
	double randomSendBackoff = context->getMacParameters().sendDataBackoffMin +
	 							(context->getRandomDouble() * backoffRange);
	//double randomSendBackoff = context->getRandomDouble() * context->getMacParameters().sendDataBackoffMax;

	context->log("Backoff is " + std::to_string(randomSendBackoff));
	
	// Start backoff timer
	moduleInterface->startTimer(RICER_MAC_TIMER_SEND_BACKOFF, randomSendBackoff);
}

void RicerStateSend::fromRadioLayer(RicerStateContextInterface *context, RicerMacInterface *moduleInterface, RicerMacPacket *packet)
{
	switch(packet->getFrameType())
	{
		case RICER_MAC_FRAME_TYPE_DATA:
		{
			if(packet->getDestination() == BROADCAST_MAC_ADDRESS)
			{
				// Plan is to implement broadcast data packets (e.g. from networking layer)
				// as a series of unicast packets. Therefore we are not handling data packets
				// with BROADCAST_MAC_ADDRESS as destination.
				throw std::runtime_error("Ricer Data type packet should never be sent to BROADCAST?");
			}
			else if(packet->getDestination() == context->getMacParameters().selfNodeId)
			{
				throw std::runtime_error("Unexpected data packet addressed to this node in state send");
			}
			else
			{
				context->log("Overheard packet not addressed to us - passing to net layer");
				moduleInterface->collectStats("Ricer overheard packet");
				// Overheard packet addressed to another node.
				// Just pass to net layer. Do not ACK.
				moduleInterface->decapsulateAndPassToNetLayer(packet);
			}
			
			break;
		}

		case RICER_MAC_FRAME_TYPE_ACK_RTR_BEACON:
		{
			if(packet->getAckForNode() == context->getMacParameters().selfNodeId)
			{
				if(moduleInterface->isTimerRunning(RICER_MAC_TIMER_WAIT_FOR_ACK))
				{
					moduleInterface->collectStats("Ricer received packet breakdown", "ACK/RTR");
					int nodeWeAreSendingTo = context->getReceivedBeaconFromNodeToSendTo();
					context->log("Received ACK from " + std::to_string(nodeWeAreSendingTo) + " - stopping ACK timer");
					moduleInterface->collectStats("Ricer packet ACKed");

					// Cancel the ACK timer
					moduleInterface->stopTimer(RICER_MAC_TIMER_WAIT_FOR_ACK);

					// If the packet was unicast
					if(!context->peekAtNextBroadcastOrUnicastWaitingToSendTo(context->getReceivedBeaconFromNodeToSendTo())->getIsDataForBroadcast())
					{
						context->log("ACK was for unicast so reporting successful send to net layer and removing packet from queue");
						// Report to net layer
						moduleInterface->reportSendingSucceededToNode(nodeWeAreSendingTo);
						// Remove the packet from the queue
						context->unicastPacketHasBeenSentToNodeSoRemoveFromQueue(nodeWeAreSendingTo);
					}
					else
					{
						// Record have sent braodcast to node 
						context->log("ACK was for broadcast packet, so recording as sent to node");
						context->recordHaveSentBroadcastPacketToNode(nodeWeAreSendingTo);
					}

					// The ACK doubles as a subsequent ready-to-receive beacon. Therefore if we have another packet
					// waiting to send to this node, go immediately to Send state
					if(context->hasNextBroadcastOrUnicastWaitingToSendTo(nodeWeAreSendingTo))
					{
						context->log("Another packet to send to ACKing node, so going straight to state Send");
						context->changeToStateSend();
					}
					else
					{
						// Otherwise, continue to state wait for send
						context->log("No subsequent packet to send to ACKing node, so going to wait to send");
						context->changeToStateWaitToSend();
					}
				}
				else
				{
					throw std::runtime_error("Unexpected ACK to this node when we are not waiting for one");
				}
			}
			else
			{
				moduleInterface->collectStats("Ricer received packet breakdown", "ACK/RTR (ignored)");
				context->log("Ignoring ACK/RTR beacon which isnt an ACK to this node - can't deal with beacon in Send state, may be backing off for send or waiting for ACK");
			}
			
			break;
		}

		case RICER_MAC_FRAME_TYPE_RTR_BEACON:
		{
			context->log("Ignoring RTR beacon - can't deal with beacon in Send state, may be backing off for send or waiting for ACK");
			moduleInterface->collectStats("Ricer received packet breakdown", "RTR (ignored)");
			break;
		}

		default:
		{
			throw std::runtime_error("Unknown packet type");
		}
	}
}

void RicerStateSend::packetFromNetLayerHasBeenBuffered(RicerStateContextInterface *context, RicerMacInterface *moduleInterface)
{
	// Nothing to do
}

void RicerStateSend::timerFired(RicerStateContextInterface *context, RicerMacInterface *moduleInterface, RicerMacTimer timer)
{
	switch(timer)
	{
		case RICER_MAC_TIMER_WAIT_FOR_RADIO_RX_TRANSITION_DELAY: 
		{
			throw std::runtime_error("Unexpected wait for radio RX transition timer expired in send state");
		}
		case RICER_MAC_TIMER_WAIT_FOR_RADIO_SLEEP_TRANSITION_DELAY:
		{
			throw std::runtime_error("Unexpected wait for radio SLEEP transition timer expired in state Send");
		}
		case RICER_MAC_TIMER_BACKOFF_FOR_CCA:
		{
			throw std::runtime_error("Unexpected CCA backoff timer expired in send state");
		}
		case RICER_MAC_TIMER_LISTEN_FOR_DATA:
		{
			throw std::runtime_error("Unexpected listen for data timer expired in send state");
		}
		case RICER_MAC_TIMER_WAKE_FOR_RECEIVE:
		{
			context->log("Wake for receive timer fired in send state so setting need-to-send-RTR flag");			
			context->setNeedToSendReadyToReceiveBeacon(true);
			break;
		}
		case RICER_MAC_TIMER_SEND_TIMEOUT:
		{
			if(moduleInterface->isTimerRunning(RICER_MAC_TIMER_WAIT_FOR_ACK))
			{
				context->log("WARNING - send timeout fired while waiting for an ACK. Extending send timeout to complete wait for ACK.");
				double ackWaitTimeLeft = moduleInterface->getTimerTimeLeft(RICER_MAC_TIMER_WAIT_FOR_ACK);
				moduleInterface->startTimer(RICER_MAC_TIMER_SEND_TIMEOUT, ackWaitTimeLeft + 0.000000001);
			}
			else if(moduleInterface->isTimerRunning(RICER_MAC_TIMER_SEND_BACKOFF))
			{
				context->log("WARNING - send timeout fired while waiting for send backoff. Aborting backoff and send, going to sleep.");
				moduleInterface->stopTimer(RICER_MAC_TIMER_SEND_BACKOFF);
				vector<int> nodeIdsOfDroppedUnicastPackets = context->dropPacketsAboveMaxSendAttempts();
				for(vector<int>::iterator it = nodeIdsOfDroppedUnicastPackets.begin(); it != nodeIdsOfDroppedUnicastPackets.end(); it++) // Note: no it++ because we need to handle removing elements, see below
				{
					moduleInterface->reportSendingFailedToNode(*it);
					context->log("Reporting failed to send unciast packet to " + std::to_string(*it));
				}
				context->changeToStateSleep();
			}
			else
			{
				throw std::runtime_error("Send timeout fired while in Send state, and not waiting for an ACK");
			}
			
			break;
		}
		case RICER_MAC_TIMER_SEND_BACKOFF:
		{
			context->log("Send backoff ended, checking CCA");
			backoffEndedCheckCca(context, moduleInterface);
			break;
		}
		case RICER_MAC_TIMER_WAIT_FOR_ACK:
		{
			context->log("ACK timer fired, no ACK received. Reporting to fail to Network layer and changing to wait to send state");
			moduleInterface->collectStats("Ricer packet not ACKed");
			// If the packet was unicast
			// if(!context->peekAtNextBroadcastOrUnicastWaitingToSendTo(context->getReceivedBeaconFromNodeToSendTo())->getIsDataForBroadcast())
			// {
			// 	// Report send failed to network layer
			// 	moduleInterface->reportSendingFailedToNode(context->getReceivedBeaconFromNodeToSendTo());
			// }
			
			sendFinishedGoToWaitToSend(context, moduleInterface);
			break;
		}
		case RICER_MAC_TIMER_WAIT_FOR_RADIO_TX_COMPLETE:
		{
			throw std::runtime_error("Unexpected wait for radio TX to complete fired in state send");
		}
		default: {
			throw std::runtime_error("Unknown timer");
		}
	}
}

//////////////////////////
// Other member functions
//////////////////////////
void RicerStateSend::backoffEndedCheckCca(RicerStateContextInterface *context, RicerMacInterface *moduleInterface)
{
	switch(moduleInterface->getCcaResultFromRadio())
	{
		case CLEAR:
		 {
			context->log("CCA clear, sending data");
			moduleInterface->collectStats("Ricer CCA clear for data");

			int nodeSendingTo = context->getReceivedBeaconFromNodeToSendTo();
			if(!context->hasNextBroadcastOrUnicastWaitingToSendTo(nodeSendingTo))
			{
				// Should never get here, should have already checked that there is a packet waiting to send
				throw std::runtime_error("Ready to send packet to node " + std::to_string(nodeSendingTo) + " but no packet found waiting");
			}

			// Get a copy of the packet
			RicerMacPacket *copyOfPacketToSend = context->getCopyOfNextBroadcastOrUnicastWaitingToSendTo(nodeSendingTo);
			// If the packet is a broadcast packet, its destination address will still at this point be BROADCAST_MAC_ADDRESS.
			// Because in this MAC we send broadcasts as a series of unicasts to individual nodes, we need to replace the
			// BROADCAST_MAC_ADDRESS with the actual "unicast" destination
			if(copyOfPacketToSend->getIsDataForBroadcast())
			{
				context->log("Sending broadcast as a unicast to node " + std::to_string(nodeSendingTo));
				copyOfPacketToSend->setDestination(nodeSendingTo);
			}
			
			moduleInterface->sendData(copyOfPacketToSend);
			
			context->log("Setting ACK timer");
			moduleInterface->startTimer(RICER_MAC_TIMER_WAIT_FOR_ACK, context->getMacParameters().waitForAckTime());
			
			break;
		}
		case BUSY: 
		{
			// Another node has probably won contention to send to the receiving node in resposne to a ready-to-receive beacon.
			// The receiving node should respond to that other node with an ACK, which we also use as another ready-to-receive beacon.
			// So just go back to state wait-to-send and wait, hopefully we will receive that next beacon and we can contend again 
			context->log("CCA busy, returning to wait-to-send state");
			moduleInterface->collectStats("Ricer CCA busy for data");

			int nodeSendingTo = context->getReceivedBeaconFromNodeToSendTo();
			if(context->peekAtNextBroadcastOrUnicastWaitingToSendTo(nodeSendingTo)->getIsDataForBroadcast())
			{
				moduleInterface->logPlotTrace("#MAC_BROAD_CCA_BUSY " + std::to_string(context->getReceivedBeaconFromNodeToSendTo()));
			}
			else
			{
				moduleInterface->logPlotTrace("#MAC_UNI_CCA_BUSY " + std::to_string(context->getReceivedBeaconFromNodeToSendTo()));
			}
			
			sendFinishedGoToWaitToSend(context, moduleInterface);
			break;
		}
		case CS_NOT_VALID: {
			throw std::runtime_error("CCA not valid");
			break;
		}
		case CS_NOT_VALID_YET: {
			throw std::runtime_error("CCA not valid yet");
			break;
		}
		default: {
			throw std::runtime_error("Unknown CCA result type");
		}
	}
}

void RicerStateSend::sendFinishedGoToWaitToSend(RicerStateContextInterface *context, RicerMacInterface *moduleInterface)
{
	// We have finished responding to a node's ready-to-receive beacon (we have either successfully or
	// not successfully sent some data)
	// Reset the integer indicating which node we received the last ready-to-receive beacon from, ready for the next beacon.
	context->setReceivedBeaconFromNodeToSendTo(-1); // -1 indicates not set
	context->changeToStateWaitToSend();
}

// void RicerStateSend::stopSendingAndGoToSleep(RicerStateContextInterface *context, RicerMacInterface *moduleInterface)
// {
// 	// If wake-for-receive timer is paused, resume
// 	if(moduleInterface->isTimerPaused(RICER_MAC_TIMER_WAKE_FOR_RECEIVE))
// 	{
// 		moduleInterface->resumeTimer(RICER_MAC_TIMER_WAKE_FOR_RECEIVE);
// 	}

// 	// Stop the send timeout timer if its running
// 	if(moduleInterface->isTimerRunning(RICER_MAC_TIMER_SEND_TIMEOUT))
// 	{
// 		moduleInterface->stopTimer(RICER_MAC_TIMER_SEND_TIMEOUT);
// 	}

// 	context->setReceivedBeaconFromNodeToSendTo(-1); // -1 indicates not set
// 	context->changeToStateSleep();
// }