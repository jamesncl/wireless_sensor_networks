#include "RicerStateListenForData.h"

/////////////////////////////////////////////////////////////////////
// Pure virtual member functions inherited from base RicerState class
/////////////////////////////////////////////////////////////////////

std::string RicerStateListenForData::getCurrentStateName()
{
	return "ListenForData";
}

void RicerStateListenForData::start(RicerStateContextInterface *context, RicerMacInterface *moduleInterface)
{
	context->log("Entered state, starting listen-for-data timer");
	// ASSUMING THAT THIS STATE IS ONLY ENTERED FROM INITIATE RECIEVE STATE:
	// no need to set radio to RX because it will already be in RX
	moduleInterface->startTimer(RICER_MAC_TIMER_LISTEN_FOR_DATA, context->getMacParameters().listenForDataTotalDwellTime());
}

void RicerStateListenForData::fromRadioLayer(RicerStateContextInterface *context, RicerMacInterface *moduleInterface, RicerMacPacket *packet)
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
				throw std::runtime_error("Ricer Data type packet should never be sent to BROADCAST_MAC_ADDRESS");
			}
			else if(packet->getDestination() == context->getMacParameters().selfNodeId)
			{
				context->log("Received packet addressed to us. Cancelling receive timer, passing to net layer");
				moduleInterface->collectStats("Ricer sent RTR and received data");
				moduleInterface->collectStats("Ricer received packet breakdown", "data");
				// Cancel the dwell timer
				moduleInterface->stopTimer(RICER_MAC_TIMER_LISTEN_FOR_DATA);
				
				// Pass the received packet to net layer
				moduleInterface->decapsulateAndPassToNetLayer(packet);

				// Send ACK / subsequent-ready-to-receive beacon 
				context->log("Sending ACK and re-entering listen-for-data state");
				moduleInterface->sendAckReadyToReceiveTo(packet->getSource());
				// Re-enter this state to listen for any subsequent data packets
				start(context, moduleInterface);
			}
			else
			{
				context->log("Overheard packet not addressed to us - passing to net layer");
				moduleInterface->collectStats("Ricer overheard packet");
				// Overheard packet addressed to another node.
				// Just pass to net layer.
				moduleInterface->decapsulateAndPassToNetLayer(packet);
				// Do not re-enter state. Do not ACK. Do not cancel dwell timer.
			}
			
			break;
		}

		case RICER_MAC_FRAME_TYPE_ACK_RTR_BEACON:
		{
			// Ignore - we are in the middle of attempting to receive data
			context->log("WARNING - ignoring ACK/RTR beacon because we are in state listen for data");
			moduleInterface->collectStats("Ricer received packet breakdown", "ACK/RTR (ignored)");
			break;
		}

		case RICER_MAC_FRAME_TYPE_RTR_BEACON:
		{
			// Ignore - we are in the middle of attempting to receive data
			context->log("WARNING - ignoring RTR beacon because we are in the state listen for data");
			moduleInterface->collectStats("Ricer received packet breakdown", "RTR (ignored)");
			break;
		}

		default:
		{
			throw std::runtime_error("Unknown packet type");
		}
	}
}

void RicerStateListenForData::packetFromNetLayerHasBeenBuffered(RicerStateContextInterface *context, RicerMacInterface *moduleInterface)
{
	// Do nothing - packet will have to wait for next time we are in state send
}

void RicerStateListenForData::timerFired(RicerStateContextInterface *context, RicerMacInterface *moduleInterface, RicerMacTimer timer)
{
	switch(timer)
	{
		case RICER_MAC_TIMER_WAIT_FOR_RADIO_RX_TRANSITION_DELAY: 
		{
			throw std::runtime_error("Unexpected wait-for-RX-transition timer expired in listen for data state");
		}
		case RICER_MAC_TIMER_WAIT_FOR_RADIO_SLEEP_TRANSITION_DELAY:
		{
			throw std::runtime_error("Unexpected wait for radio SLEEP transition timer expired in state listen for data");
		}
		case RICER_MAC_TIMER_BACKOFF_FOR_CCA:
		{
			throw std::runtime_error("Unexpected backoff timer expired in listen for data state");
		}
		case RICER_MAC_TIMER_LISTEN_FOR_DATA:
		{
			context->log("Listen for data timer expired, no data heard.");
			moduleInterface->collectStats("Ricer sent RTR but no data");
			exitStateToWaitToSend(context, moduleInterface);
			break;
		}
		case RICER_MAC_TIMER_WAKE_FOR_RECEIVE:
		{
			throw std::runtime_error("Unexpected wake for receive timer expired in listen for data state");
		}
		case RICER_MAC_TIMER_SEND_TIMEOUT:
		{
			throw std::runtime_error("Unexpected send timeout timer in listen for data state");
		}
		case RICER_MAC_TIMER_SEND_BACKOFF:
		{
			throw std::runtime_error("Unexpected send backoff timer in state listen for data");
		}
		case RICER_MAC_TIMER_WAIT_FOR_ACK:
		{
			throw std::runtime_error("Unexpected ACK timer fired in state listen for data");
		}
		case RICER_MAC_TIMER_WAIT_FOR_RADIO_TX_COMPLETE:
		{
			throw std::runtime_error("Unexpected wait for radio TX to complete fired in state listen for data");
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

void RicerStateListenForData::exitStateToWaitToSend(RicerStateContextInterface *context, RicerMacInterface *moduleInterface)
{
	context->log("Exiting state and going to wait to send. Setting wake for receive timer");

	// Calculate a random jitter for the next wake for receive interval, to avoid syncing problems
	// which can cause nodes to want to send their RTR beacons at the same time (cuases excess collision)
	double jitterAmount = context->getMacParameters().wakeForReceiveIntervalJitter;
	// Get a random value between 0 and (jitter * 2) then subtract jitter so that we get +/- jitter amount
	double randomJitter = (context->getRandomDouble() * jitterAmount * 2) - jitterAmount;
	double wakeForReceiveInterval = context->getMacParameters().wakeForReceiveInterval + randomJitter;
	moduleInterface->startTimer(RICER_MAC_TIMER_WAKE_FOR_RECEIVE, wakeForReceiveInterval);
	context->log("Wake jitter " + std::to_string(randomJitter) + " total interval " + std::to_string(wakeForReceiveInterval));

	// Change to wait to send
	context->changeToStateWaitToSend();
}