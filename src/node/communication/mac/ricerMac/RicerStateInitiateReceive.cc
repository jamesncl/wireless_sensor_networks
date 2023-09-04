#include "RicerStateInitiateReceive.h"

/////////////////////////////////////////////////////////////////////
// Pure virtual member functions inherited from base RicerState class
/////////////////////////////////////////////////////////////////////

std::string RicerStateInitiateReceive::getCurrentStateName()
{
	return "InitiateReceive";
}

void RicerStateInitiateReceive::start(RicerStateContextInterface *context, RicerMacInterface *moduleInterface)
{
	context->log("Start - Requesting radio go to RX");
	context->resetBackoff();
	moduleInterface->setRadioState(RX);
	// // Then we need to wait for long enough for the transition to complete (otherwise we get invalid CCA results)
	moduleInterface->startTimer(RICER_MAC_TIMER_WAIT_FOR_RADIO_RX_TRANSITION_DELAY, context->getMacParameters().waitForRxTransitionDelayTime);
}

void RicerStateInitiateReceive::fromRadioLayer(RicerStateContextInterface *context, RicerMacInterface *moduleInterface, RicerMacPacket *packet)
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
				throw std::runtime_error("Received a data packet before ready-to-receive beacon has been sent!");
			}
			else
			{
				context->log("Overheard packet not addressed to us - passing to net layer");
				// Overheard packet addressed to another node.
				// Just pass to net layer.
				moduleInterface->decapsulateAndPassToNetLayer(packet);
			}

			break;
		}

		case RICER_MAC_FRAME_TYPE_ACK_RTR_BEACON:
		{
			// Ignore - we are in the middle of attempting to initiate a receive
			context->log("WARNING - ignoring ACK/RTR beacon because we are in state initiate receive");
			moduleInterface->collectStats("Ricer received packet breakdown", "ACK/RTR (ignored)");
			break;
		}

		case RICER_MAC_FRAME_TYPE_RTR_BEACON:
		{
			// Ignore - we are in the middle of attempting to initiate a receive
			context->log("WARNING - ignoring RTR beacon because we are in state initiate receive");
			moduleInterface->collectStats("Ricer received packet breakdown", "RTR (ignored)");
			break;
		}

		default:
		{
			throw std::runtime_error("Unknown packet type");
		}
	}
}

void RicerStateInitiateReceive::packetFromNetLayerHasBeenBuffered(RicerStateContextInterface *context, RicerMacInterface *moduleInterface)
{
	// Do nothing - packet will have to wait for next time we are in state send
}

void RicerStateInitiateReceive::timerFired(RicerStateContextInterface *context, RicerMacInterface *moduleInterface, RicerMacTimer timer)
{
	switch(timer)
	{
		case RICER_MAC_TIMER_WAIT_FOR_RADIO_RX_TRANSITION_DELAY: 
		{
			context->log("Radio has completed change to RX, checking CCA");
			checkCca(context, moduleInterface);
			break;
		}
		case RICER_MAC_TIMER_WAIT_FOR_RADIO_SLEEP_TRANSITION_DELAY:
		{
			throw std::runtime_error("Unexpected wait for radio SLEEP transition timer expired in state initiate receive");
		}
		case RICER_MAC_TIMER_BACKOFF_FOR_CCA:
		{
			context->log("Backoff complete, requesting CCA");
			checkCca(context, moduleInterface);
			break;
		}
		case RICER_MAC_TIMER_LISTEN_FOR_DATA:
		{
			throw std::runtime_error("Listen for data timer expired unexpectedly in initiate receive state");
		}
		case RICER_MAC_TIMER_WAKE_FOR_RECEIVE:
		{
			throw std::runtime_error("Unexpected wake for receive timer expired in initiate receive state");
		}
		case RICER_MAC_TIMER_SEND_TIMEOUT:
		{
			throw std::runtime_error("Unexpected send timeout timer in state initiate receive");
		}
		case RICER_MAC_TIMER_SEND_BACKOFF:
		{
			throw std::runtime_error("Unexpected send backoff timer in state initiate receive");
		}
		case RICER_MAC_TIMER_WAIT_FOR_ACK:
		{
			throw std::runtime_error("Unexpected ACK timer fired in state initiate receive");
		}
		case RICER_MAC_TIMER_WAIT_FOR_RADIO_TX_COMPLETE:
		{
			context->log("Finished waiting for radio to complete TX, requesting CCA");
			checkCca(context, moduleInterface);
			break;
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

void RicerStateInitiateReceive::checkCca(RicerStateContextInterface *context, RicerMacInterface *moduleInterface)
{
	switch(moduleInterface->getCcaResultFromRadio())
	{
		case CLEAR: {
			context->log("CCA clear, sending read-to-receive beacon and changing to state listen-for-data");
			moduleInterface->collectStats("Ricer CCA clear for RTR");
			moduleInterface->sendReadyToReceiveBeacon();
			context->resetBackoff();
			context->changeToStateListenForData();
			break;
		}
		case BUSY: {
			context->log("CCA busy, requesting backoff");
			moduleInterface->collectStats("Ricer CCA busy for RTR");
			moduleInterface->logPlotTrace("#MAC_RTR_CCA_BUSY");

			double backoffTime = context->getNextBackoff();
			context->log("Backoff requested. Requesting timer for " + std::to_string(backoffTime));
			moduleInterface->startTimer(RICER_MAC_TIMER_BACKOFF_FOR_CCA, backoffTime);

			break;
		}
		case CS_NOT_VALID: {
			// This can happen in the following edge case:
			// Have just sent a packet to the radio to be delivered to another node. Transmission starts.
			// It's a broadcast packet so we're not waiting for an ACK.
			// The send timeout timer happens to fire before the transmission ends (but the MAC is unaware of this)
			// State changes to sleep, but we need to send a beacon (the need-to-send-RTR flag is set).
			// State initiate-receive asks radio to go to RX, and waits, but the radio is actually still
			// transmitting. This means that when we ask for CCA its invalid. Therefore need to just wait a bit
			// until transmit has ended and radio goes back to RX

			context->log("WARNING - CCA NOT VALID. This is okay if it only happens occasionally due to radio happens to be in mid-transmission");
			moduleInterface->startTimer(RICER_MAC_TIMER_WAIT_FOR_RADIO_TX_COMPLETE, 
				context->getMacParameters().waitforRadioTxCompleteAfterInvalidCcaResult);
			break;
		}
		case CS_NOT_VALID_YET: {
			context->log("WARNING - CCA NOT VALID YET. This is okay if it only happens occasionally due to radio happens to be in mid-transmission");
			moduleInterface->startTimer(RICER_MAC_TIMER_WAIT_FOR_RADIO_TX_COMPLETE, 
				context->getMacParameters().waitforRadioTxCompleteAfterInvalidCcaResult);
			break;
		}
		default: {
			throw std::runtime_error("Unknown CCA result type");
		}
	}
}

