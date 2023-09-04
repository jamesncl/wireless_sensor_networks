#include "RicerStateWaitToSend.h"

/////////////////////////////////////////////////////////////////////
// Pure virtual member functions inherited from base RicerState class
/////////////////////////////////////////////////////////////////////

std::string RicerStateWaitToSend::getCurrentStateName()
{
	return "WaitToSend";
}

void RicerStateWaitToSend::start(RicerStateContextInterface *context, RicerMacInterface *moduleInterface)
{
	context->log("Entered state");

	waitToSendStartedAt = moduleInterface->getCurrentSimulationTime();

	if(!context->hasMessagesToSend())
	{
		context->log("Nothing to send, exiting state");
		stopSendingAndGoToSleep(context, moduleInterface);
		return;
	}

	// It is possible to re-enter this state after sending a packet (from state Send)
	// In this case the send timeout timer will still be running. Need to check for this.
	if(!moduleInterface->isTimerRunning(RICER_MAC_TIMER_SEND_TIMEOUT))
	{
		// If send timeout timer is not running, must be entering this state 'fresh'
		// i.e. not immediately after a send.
		context->log("Send timeout timer not running, so we must be entering this state for first time in this sleep/wake cycle");
		
		// Increment send attempts on all packets.
		// We do this at the *start* of the wait-to-send state so that we know which packets have had
		// a full wait-to-send window of opportunity to be sent. Packets which happen to be added to
		// the send buffer *after* we have started waiting remain on zero send attempts (which is fair)
		// and thus get another try at sending 
		context->incrementSendAttemptsOnAllWaitingPackets();

		// Need to check again if any messages to send
		if(!context->hasMessagesToSend())
		{
			context->log("After incrementing retries and dropping as required, now have nothing to send, exiting state");
			stopSendingAndGoToSleep(context, moduleInterface);
			return;
		}

		
		// // Pause wakeup timer
		// moduleInterface->pauseTimer(RICER_MAC_TIMER_WAKE_FOR_RECEIVE);

		// Start send timeout timer
		context->log("Starting send timeout timer");
		// We need to wait for the wakeForReceiveInterval parameter, so that we have a chance to hear
		// ready-to-receive beacons from the intended destinations
		// (Note that if the destination node itself has to wait for sending, it may not send a ready-to-receive
		// beacon in time because RTR beacon sending is paused when waiting to send. This is a design decision tradeoff)
		moduleInterface->startTimer(RICER_MAC_TIMER_SEND_TIMEOUT, 
			context->getMacParameters().wakeForReceiveInterval + context->getMacParameters().wakeForReceiveIntervalJitter);

		// Make sure radio set to RX
		moduleInterface->setRadioState(RX);
	}
	else
	{
		context->log("Send timeout timer is running, so we must be returning to this state after a Send");
	}

	// Nothing else to do - just wait for ready-to-receive beacons
}

void RicerStateWaitToSend::fromRadioLayer(RicerStateContextInterface *context, RicerMacInterface *moduleInterface, RicerMacPacket *packet)
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
				//throw std::runtime_error("Unexpected data packet addressed to this node in state wait-to-send");
				// Edge case
				// Should be handled better with extra checks on radio state. Radio module doesn't currently support this so
				// taking the decision to ignore this very infrequent edge case 
				context->log("WARNING - Unexpected data packet addressed to this node in state wait-to-send. Okay if this only happens occasionally - "
					+ std::string("edge case is that a node sends an RTR beacon, starts the listen for data timer, but an ongoing transmission from a ")
					+ std::string("previous packet delays the actual sending of the RTR, making it possible for a node to hear a packet after the listen-for-data ")
					+ std::string("timer has expired."));
				break;
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
			// If the ACK is addressed to this node, something has gone wrong - we are not expecting to receive an ACK
			// when waiting to send
			if(packet->getAckForNode() == context->getMacParameters().selfNodeId)
			{
				throw std::runtime_error("Unexpected ACK when waiting to send. Should have received this when in Send state?");
			}
			
			// Otherwise, we have overheard an ACK from a neighbour to another neighbour. We can use this as an RTR beacon
			moduleInterface->collectStats("Ricer received packet breakdown", "ACK/RTR");
			receivedBeaconFrom(context, moduleInterface, packet->getSource());

			break;
		}

		case RICER_MAC_FRAME_TYPE_RTR_BEACON:
		{
			moduleInterface->collectStats("Ricer received packet breakdown", "RTR");
			receivedBeaconFrom(context, moduleInterface, packet->getSource());
			break;
		}

		default:
		{
			throw std::runtime_error("Unknown packet type");
		}
	}
}

void RicerStateWaitToSend::packetFromNetLayerHasBeenBuffered(RicerStateContextInterface *context, RicerMacInterface *moduleInterface)
{
	
}

void RicerStateWaitToSend::timerFired(RicerStateContextInterface *context, RicerMacInterface *moduleInterface, RicerMacTimer timer)
{
	switch(timer)
	{
		case RICER_MAC_TIMER_WAIT_FOR_RADIO_RX_TRANSITION_DELAY: 
		{
			throw std::runtime_error("Unexpected wait for radio RX transition timer expired in wait-to-send state");
		}
		case RICER_MAC_TIMER_WAIT_FOR_RADIO_SLEEP_TRANSITION_DELAY:
		{
			throw std::runtime_error("Unexpected wait for radio SLEEP transition timer expired in state wait-to-send");
		}
		case RICER_MAC_TIMER_BACKOFF_FOR_CCA:
		{
			throw std::runtime_error("Unexpected CCA backoff timer expired in wait-to-send state");
		}
		case RICER_MAC_TIMER_LISTEN_FOR_DATA:
		{
			throw std::runtime_error("Unexpected listen for data timer expired in wait-to-send state");
		}
		case RICER_MAC_TIMER_WAKE_FOR_RECEIVE:
		{
			// Set flag to indicate that the wake for receive timer has expired.
			// On sleep, this flag will be checked, and if true will initiate receive
			context->log("Wake for receive timer fired in wait to send state so setting need-to-send-RTR flag");
			context->setNeedToSendReadyToReceiveBeacon(true);
			break;
			//throw std::runtime_error("Unexpected wake for receive timer fired - should be paused when waiting to send");
		}
		case RICER_MAC_TIMER_SEND_TIMEOUT:
		{
			context->log("Send timeout fired - dropping packets above max send attempts, exiting send state");

			// REMOVED THIS - UNEXPECTEDLY HIGH FAILURE REPORTING, WHICH IS CAUSING LARGE FLUCTUATIONS IN
			// OUTGOING LINK QUALITY ESTIMATION, CAUSING HIGHER FALSE-POSITIVE LOOP DETECTION 
			// If we have any unicast packets waiting in the queue, report failure to send for the first packet in the queue to net layer
			// if(context->howManyUnicastPacketsInBuffer() > 0)
			// {
			// 	int failedToSendUnicastDestinationNodeId = context->peekAtNextUnicastPacket()->getDestination();
			// 	moduleInterface->reportSendingFailedToNode(failedToSendUnicastDestinationNodeId);
			// 	context->log("There is a unicast packet for destination " + std::to_string(failedToSendUnicastDestinationNodeId) 
			// 		+ " so reporting failure to send to net layer");
			// }

			vector<int> nodeIdsOfDroppedUnicastPackets = context->dropPacketsAboveMaxSendAttempts();
			for(vector<int>::iterator it = nodeIdsOfDroppedUnicastPackets.begin(); it != nodeIdsOfDroppedUnicastPackets.end(); it++) // Note: no it++ because we need to handle removing elements, see below
			{
				moduleInterface->reportSendingFailedToNode(*it);
				context->log("Reporting failed to send unciast packet to " + std::to_string(*it));
			}

			stopSendingAndGoToSleep(context, moduleInterface);
			break;
		}
		case RICER_MAC_TIMER_SEND_BACKOFF:
		{
			throw std::runtime_error("Unexpected send backoff timer in state wait-to-send");
		}
		case RICER_MAC_TIMER_WAIT_FOR_ACK:
		{
			throw std::runtime_error("Unexpected ACK timer fired in state wait to send");
		}
		case RICER_MAC_TIMER_WAIT_FOR_RADIO_TX_COMPLETE:
		{
			throw std::runtime_error("Unexpected wait for radio TX to complete fired in state wait to send");
		}
		default: 
		{
			throw std::runtime_error("Unknown timer");
		}
	}
}

//////////////////////////
// Other member functions
//////////////////////////
void RicerStateWaitToSend::receivedBeaconFrom(RicerStateContextInterface *context, RicerMacInterface *moduleInterface, int beaconFromNode)
{
	// If we have a packet waiting to send to the node which has issued the ready-to-receive beacon
	if(context->hasNextBroadcastOrUnicastWaitingToSendTo(beaconFromNode))
	{
		context->log("Received RTR beacon from " + std::to_string(beaconFromNode) + " and have packet to send so changing to state Send");
		context->setReceivedBeaconFromNodeToSendTo(beaconFromNode);
		recordWaitingTime(context, moduleInterface);
		context->changeToStateSend();
	}
	else
	{
		context->log("Received RTR beacon from " + std::to_string(beaconFromNode) + " but no packet waiting to send to it, so ignoring");
	}
}

void RicerStateWaitToSend::stopSendingAndGoToSleep(RicerStateContextInterface *context, RicerMacInterface *moduleInterface)
{
	// If wake-for-receive timer is paused, resume
	// if(moduleInterface->isTimerPaused(RICER_MAC_TIMER_WAKE_FOR_RECEIVE))
	// {
	// 	moduleInterface->resumeTimer(RICER_MAC_TIMER_WAKE_FOR_RECEIVE);
	// }

	// Stop the send timeout timer if its running
	if(moduleInterface->isTimerRunning(RICER_MAC_TIMER_SEND_TIMEOUT))
	{
		moduleInterface->stopTimer(RICER_MAC_TIMER_SEND_TIMEOUT);
	}

	recordWaitingTime(context, moduleInterface);
	context->changeToStateSleep();
}

void RicerStateWaitToSend::recordWaitingTime(RicerStateContextInterface *context, RicerMacInterface *moduleInterface)
{
	double waitToSendDuration = moduleInterface->getCurrentSimulationTime() - waitToSendStartedAt;
	if(waitToSendDuration < 0 || waitToSendStartedAt == -1)
	{
		opp_error("Error calculating wait to send time. Was wait to send start time properly set on entering state?");
	}
	moduleInterface->collectStats("Ricer wait to send time", "", waitToSendDuration);
	waitToSendStartedAt = -1;
}