#include "RicerStateSleep.h"

/////////////////////////////////////////////////////////////////////
// Pure virtual member functions inherited from base RicerState class
/////////////////////////////////////////////////////////////////////

std::string RicerStateSleep::getCurrentStateName()
{
	return "Sleep";
}

void RicerStateSleep::start(RicerStateContextInterface *context, RicerMacInterface *moduleInterface)
{
	context->log("Entered state");

	// Check if we need to send a RTR beacon. This happens when the wake-for-receive
	// timer goes off while in wait-to-send state.
	if(context->getNeedToSendReadyToReceiveBeacon())
	{
		context->log("Need to send RTR beacon so not sleeping, going to initiate receive state");
		// Reset the flag
		context->setNeedToSendReadyToReceiveBeacon(false);
		context->changeToStateInitiateReceive();
	}
	else
	{
		context->log("Setting radio to SLEEP and waiting for minimum transition time before allowing wakeup");
		moduleInterface->setRadioState(SLEEP);
		moduleInterface->startTimer(RICER_MAC_TIMER_WAIT_FOR_RADIO_SLEEP_TRANSITION_DELAY, 
			context->getMacParameters().waitForSleepTransitionDelayTime);
		sleepStartedAt = moduleInterface->getCurrentSimulationTime();
	}
}

void RicerStateSleep::fromRadioLayer(RicerStateContextInterface *context, RicerMacInterface *moduleInterface, RicerMacPacket *packet)
{
	// Its possible to receive a message while radio is transitioning
	// to sleep mode. Need to handle this edge case. 
	
	context->log(std::string("WARNING: Received message from radio layer in sleep mode. This should only possible ") +
		std::string("if a message arrives during the small time it takes for radio to transition to sleep mode."));

	if(packet->getDestination() == context->getMacParameters().selfNodeId)
	{
		// This should never happen. Packets should only be sent to this node after this 
		// node has sent a ready-to-receive beacon (and node is in listen-for-data state)
		throw std::runtime_error("Unexpected message received in sleep state which is addressed to this node");
	}
	else
	{
		// Must be broadcast or overheard packet.
		switch(packet->getFrameType())
		{
			// Pass data up to the net layer
			case RICER_MAC_FRAME_TYPE_DATA:
			{
				context->log("Passing overheard message to net layer");
				moduleInterface->collectStats("Ricer overheard packet");
				moduleInterface->decapsulateAndPassToNetLayer(packet);
				break;
			}
			case RICER_MAC_FRAME_TYPE_ACK_RTR_BEACON:
			{
				context->log("WARNING - Ignoring ACK/RTR beacon - in sleep state");
				moduleInterface->collectStats("Ricer received packet breakdown", "ACK/RTR (ignored)");
				break;
			}
			case RICER_MAC_FRAME_TYPE_RTR_BEACON:
			{
				context->log("WARNING - Ignoring RTR beacon - in sleep state");
				moduleInterface->collectStats("Ricer received packet breakdown", "RTR (ignored)");
				break;
			}
			default:
			{
				throw std::runtime_error("Unknown packet type");
			}
		}
	}
}

void RicerStateSleep::packetFromNetLayerHasBeenBuffered(RicerStateContextInterface *context, RicerMacInterface *moduleInterface)
{
	// Edge case: it is possible for a message to arrive from the network layer for sending, during the
	// time the radio is transitioning to sleep mode. Don't go to wait-to-send immediately - it will
	// attempt to switch radio to RX, which will fail (silently!) and no RTR beacons can be heard.
	// Instead, wait until the transition timer has finished, then go to wait-to-send
	if(moduleInterface->isTimerRunning(RICER_MAC_TIMER_WAIT_FOR_RADIO_SLEEP_TRANSITION_DELAY))
	{
		context->log("Packet from net later has been buffered but radio is still transitioning to SLEEP state - waiting for transition to complete");
		context->setNeedToWakeToSendNewPacket(true);
	}
	else
	{
		context->log("Packet from net later has been buffered so waking up from sleep - changing to state wait to send");
		recordSleepTime(context, moduleInterface);
		context->changeToStateWaitToSend();
	}
}

void RicerStateSleep::timerFired(RicerStateContextInterface *context, RicerMacInterface *moduleInterface, RicerMacTimer timer)
{
	switch(timer)
	{
		case RICER_MAC_TIMER_WAIT_FOR_RADIO_RX_TRANSITION_DELAY: 
		{
			throw std::runtime_error("Unexpected wait for radio RX transition timer expired in sleep state");
		}
		case RICER_MAC_TIMER_WAIT_FOR_RADIO_SLEEP_TRANSITION_DELAY:
		{
			if(context->getNeedToWakeForReceive())
			{
				context->log("Finished waiting for radio transition, can now wake for receive");
				context->setNeedToWakeForReceive(false);
				recordSleepTime(context, moduleInterface);
				context->changeToStateInitiateReceive();
			}
			else if(context->getNeedToWakeToSendNewPacket())
			{
				context->log("Finished waiting for radio transition, can now send packet buffered during sleep");
				context->setNeedToWakeToSendNewPacket(false);
				recordSleepTime(context, moduleInterface);
				context->changeToStateWaitToSend();
			}
			break;
		}
		case RICER_MAC_TIMER_BACKOFF_FOR_CCA:
		{
			throw std::runtime_error("Unexpected backoff timer expired in sleep state");
		}
		case RICER_MAC_TIMER_LISTEN_FOR_DATA:
		{
			throw std::runtime_error("Unexpected listen for data timer expired in sleep state");
		}
		case RICER_MAC_TIMER_WAKE_FOR_RECEIVE:
		{
			if(moduleInterface->isTimerRunning(RICER_MAC_TIMER_WAIT_FOR_RADIO_SLEEP_TRANSITION_DELAY))
			{
				context->log("WARNING - wake for receive timer fired but radio is still transitioning to SLEEP state - waiting for transition to complete");
				context->setNeedToWakeForReceive(true);
			}
			else
			{
				context->log("Wake for receive timer has expired so going to initiate receive state");
				moduleInterface->collectStats("Ricer wakeup");
				recordSleepTime(context, moduleInterface);
				context->changeToStateInitiateReceive();
			}
			break;
		}
		case RICER_MAC_TIMER_SEND_TIMEOUT:
		{
			throw std::runtime_error("Unexpected send timeout expired in sleep state");
		}
		case RICER_MAC_TIMER_SEND_BACKOFF:
		{
			throw std::runtime_error("Unexpected send backoff timer in state sleep");
		}
		case RICER_MAC_TIMER_WAIT_FOR_ACK:
		{
			throw std::runtime_error("Unexpected ACK timer fired in state sleep");
		}
		case RICER_MAC_TIMER_WAIT_FOR_RADIO_TX_COMPLETE:
		{
			throw std::runtime_error("Unexpected wait for radio TX to complete fired in state sleep");
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
void RicerStateSleep::recordSleepTime(RicerStateContextInterface *context, RicerMacInterface *moduleInterface)
{
	double sleepDuration = moduleInterface->getCurrentSimulationTime() - sleepStartedAt;
	if(sleepDuration < 0 || sleepStartedAt == -1)
	{
		opp_error("Error calculating sleep time. Was sleep start time properly set on entering state?");
	}
	moduleInterface->collectStats("Ricer sleep time", "", sleepDuration);
	sleepStartedAt = -1;
}