#include "BoxMacTwoController.h"

// Register the module in Omnet++
Define_Module(BoxMacTwoController);

const char * BoxMacTwoController::OUTPUT_OVERHEARD = "BoxMac Overheard";
const char * BoxMacTwoController::OUTPUT_RECEIVED_BROADCAST = "BoxMac Received broadcast";
const char * BoxMacTwoController::OUTPUT_RECEIVED_DATA = "BoxMac Received data";
const char * BoxMacTwoController::OUTPUT_RECEIVED_ACK = "BoxMac Received ACK";
const char * BoxMacTwoController::OUTPUT_SENT_ACK = "BoxMac Sent ACK";
const char * BoxMacTwoController::OUTPUT_IDLE_LISTENING = "BoxMac Idle listening";
const char * BoxMacTwoController::OUTPUT_TOTAL_SLEEP_DURATION = "BoxMac Total sleep duration";

void BoxMacTwoController::startup()
{
	trace() << "Controller starting up";

	// Check if we have already done an initial startup
	if(!hasStartedUpOnce)
	{
		// These things should only be initialised once

		// Store NED parameters
		sleepTime = par("sleepTime");
		receivePeriodAfterCcaBusy = par("receivePeriodAfterCcaBusy");
		ackFrameSizeBits = par("ackFrameSizeBits");
		dataFrameSizeBits = par("dataFrameSizeBits");

		// Declare stats outputs
		declareOutput(OUTPUT_OVERHEARD);
		declareOutput(OUTPUT_RECEIVED_BROADCAST);
		declareOutput(OUTPUT_RECEIVED_DATA);
		declareOutput(OUTPUT_RECEIVED_ACK);
		declareOutput(OUTPUT_SENT_ACK);
		declareOutput(OUTPUT_IDLE_LISTENING);
		declareOutput(OUTPUT_TOTAL_SLEEP_DURATION);

		hasStartedUpOnce = true;
	}
	
	// The following may be initialised multiple times - every time the node runs out of energy, then restarts after energy harvesting

	// Initialise member variables
	boxMacState = BOX_MAC_STATE_STARTUP;
	sleepTimerTimeLeft = 0;
	isSleepTimerPaused = false;
	idleListen = true;
	sleepStartedAt = -1;

	// Start polling CCA
	startCcaPolling();
}

void BoxMacTwoController::handleOutOfEnergy(cMessage *outOfEnergyMsg)
{
	// We need to simulate what happens when a node runs out of energy - all state will be lost
	
	// It's unlikely but possible that a node may set a timer, then run out of energy,
	// then restart, and pick up the expired timer message from before shutting down. So
	// just in case, cancel all pending timers 
	cancelAllTimers();
	
	// No need to clear VirtualMac packet buffer because we don't use it (Sender submodule handles buffering)
	// No need to clear VirtualMac pktHistory buffer (used for dupe checking) because we don't use it (we handle dupe checking in routing layer)

	// DO NOT RESET THE PACKET SEQUENCE NUMBER - OTHERWISE DUPLICATE PACKET CHECKING WILL ERRONEOUSLY DISCARD PACKETS WHEN OTHER NODES RESTART

	// Reset state
	boxMacState = BOX_MAC_STATE_STARTUP;
	sleepTimerTimeLeft = 0;
	isSleepTimerPaused = false;
	idleListen = true;

	// No need to reinitialise private variables and state set in startup() - startup will be called again when node restarts

	// Signal to sub-modules that we are out of energy
	send(outOfEnergyMsg->dup(), "toBoxMacCca");
	send(outOfEnergyMsg->dup(), "toBoxMacSender");
	cancelAndDelete(outOfEnergyMsg);
}

void BoxMacTwoController::startCcaPolling()
{
	changeState(BOX_MAC_STATE_POLLING_CCA);

	//trace() << "Asking the CCA poller to start polling";
	// Ask the CCA module to start polling
	CcaControlCommand *startCcaPollingMsg = new CcaControlCommand("CCA control command", CCA_CONTROL_COMMAND);
	startCcaPollingMsg->setCcaControlCommandKind(CCA_CONTROL_START_POLLING);
	send(startCcaPollingMsg, "toBoxMacCca");
}

int BoxMacTwoController::handleControlCommand(cMessage * msg)
{
	if(msg->getKind() != MAC_CONTROL_COMMAND) {
		opp_error("BoxMacTwoController: Expected MAC Control Command");
	}

	BoxMacControlMessage *controlMsg = check_and_cast <BoxMacControlMessage*>(msg);
	switch(controlMsg->getMacControlCommandKind()) {

		case CCA_CHECK_CHANNEL_IS_BUSY: {
			trace() << "Received busy signal from CCA check, listening for messages";
			// We have detected signals being transmitted.
			changeState(BOX_MAC_STATE_LISTENING);
			// The radio stays on, and we listen for the specified period:
			setTimer(BOX_MAC_TIMER_LISTEN_PERIOD, receivePeriodAfterCcaBusy);
			// Reset flag for idle listen statistics (no messages received addressed to us when listening)
			// If we hear a message while listening, this flag will be set to false
			idleListen = true;
			break;
		}

		case CCA_CHECK_CHANNEL_IS_CLEAR: {
			trace() << "Received clear signal from CCA check, signalling Sender okay-to-send";
			// There are no messages to hear. Send any messages (if there are any).
			// When the sender signals back it has finished sending, we will then go to sleep.
			signalSenderOkayToSend();
			break;
		}

		case SENDER_IS_SENDING: {
			// The sender is signalling that it is about to start sending messages.
			// We need to check whether we are in the middle of sleep. In this case we need to pause the sleep timer, 
			// and restart it when the sender signals finished.

			trace() << "Sender has signalled that it is sending";
			// If we are in sleep, pause the sleep timer
			if(boxMacState == BOX_MAC_STATE_SLEEPING)
			{
				trace() << "In sleep, so pausing sleep timer";
				// For stats
				recordSleepDurationStats();

				// Get the arrival time of the sleep timer (the time at which sleep will end):
				simtime_t sleepTimerTime = getTimer(BOX_MAC_TIMER_LPL_SLEEP);
				if(sleepTimerTime == -1) {
					opp_error("BoxMacTwoController: Failed to get current sleep timer time");
				}
				// Work out how much time is left until the timer fires and store this in sleepTimerTimeLeft
				sleepTimerTimeLeft = sleepTimerTime - getClock();
				
				if(sleepTimerTimeLeft < 0) {
					opp_error("Negative time left on timer, something went wrong! Timer time was %d, time now (including drift) was %d, difference is %d",
						sleepTimerTime.dbl(),
						getClock().dbl(),
						sleepTimerTimeLeft.dbl());
				}

				// We can't really pause the timer, we have to cancel it and reschedule later:
				cancelTimer(BOX_MAC_TIMER_LPL_SLEEP);
				isSleepTimerPaused = true;

				changeState(BOX_MAC_STATE_WAITING_FOR_SENDER_FROM_SLEEP);
			}
			break;
		}

		case SENDER_FINISHED_SENDING: {
			
			trace() << "Sender has signalled that it has finished sending";
			// Either we are in the middle of sleep (with sleep timer paused) or we can now start the sleep period.
			if(boxMacState == BOX_MAC_STATE_WAITING_FOR_SENDER_FROM_SLEEP)
			{
				if(!isSleepTimerPaused) {
					opp_error("State is 'waiting-for-sender-from-sleep, but sleep timer is not paused - sotheing went wrong");
				}
				trace() << "Sleep is paused, so restarting timer";
				// Sleep is paused. Restart the timer
				setTimer(BOX_MAC_TIMER_LPL_SLEEP, sleepTimerTimeLeft);
				isSleepTimerPaused = false;
			}
			else
			{
				plotTrace() << "#MAC_SLEEP";
				// Otherwise, we need to start the sleep period.
				trace() << "Starting sleep period of " << sleepTime << " ms";
				setTimer(BOX_MAC_TIMER_LPL_SLEEP, sleepTime);
				trace() << "Sleep timer time: " << std::to_string(getTimer(BOX_MAC_TIMER_LPL_SLEEP).dbl());
			}

			// For stats
			sleepStartedAt = simTime().dbl();

			changeState(BOX_MAC_STATE_SLEEPING);
			toRadioLayer(createRadioCommand(SET_STATE, SLEEP));
			break;
		}

		case SENDING_FAILED_NO_ACK: {
			
			// The sender has reported that the send has failed to be ACKed by recipient. Inform the Network layer
			// The network layer is responsible for retrying, and may update it's link quality metrics.
			RoutingControlMessage *sendFailedMsg = new RoutingControlMessage("routing control msg", NETWORK_CONTROL_COMMAND);
			sendFailedMsg->setRoutingControlMessageKind(ROUTING_MSG_MAC_SENDING_FAILED_NO_ACK);
			sendFailedMsg->setValue(controlMsg->getValue()); // The value is the node ID of the node we were trying to send to
			toNetworkLayer(sendFailedMsg);
			break;
		}

		default: {
			opp_error("BoxMacTwoController: Unknown MAC control command kind");
		}
	}

	// if we return true (1), the message is not deleted.
	// if we return false (not 1) the message is deleted.
	// Wehave finished with the message, so return 0
	return 0;
}

void BoxMacTwoController::fromRadioLayer(cPacket * pkt, double rssi, double lqi)
{
	BoxMacTwoPacket *macFrame = check_and_cast <BoxMacTwoPacket*>(pkt);

	if(!macFrame) {
		opp_error("BoxMacTwoController: Received MAC frame from unknown MAC");
	}

	if(boxMacState == BOX_MAC_STATE_SLEEPING) {
		trace() << "WARNING - Ignoring received MAC frame from radio because we are in sleep. This probably shouldn't occur?";
		return;
	}

	
	int destination = macFrame->getDestination();
	int source = macFrame->getSource();

	// If we receive a broadcast message, do not send an ACK - just de-dupe and pass up to network layer
	if (destination ==  BROADCAST_MAC_ADDRESS) {
		if(isNotDuplicatePacket(macFrame))
		{
			trace() << "Received broacast message from " << source << ", passing to Network layer";
			plotTrace() << "#MAC_REC_BROADCAST";
			collectOutput(BoxMacTwoController::OUTPUT_RECEIVED_BROADCAST); // Add 1 to stat
			toNetworkLayer(decapsulatePacket(macFrame));
			
		}
		else
		{
			plotTrace() << "#MAC_REC_BROADCAST_DUP";
			trace() << "Discarding duplicate broadcast packet " << macFrame->getSequenceNumber() << " from node " << macFrame->getSource();
		}
		return;
	}

	// If we overhear a packet not addressed to us
	if (destination != SELF_MAC_ADDRESS){
		// Check if its an ACK or DATA type
		if(macFrame->getFrameType() == BOX_MAC_FRAME_TYPE_DATA)
		{
			//De-dupe
			if(isNotDuplicatePacket(macFrame))
			{
				// Pass data frames up to the network layer in case it wants to snoop on overheard packets (e.g. to detect pull requests)
				plotTrace() << "#MAC_OVERHEAR";
				collectOutput(BoxMacTwoController::OUTPUT_OVERHEARD); // Add 1 to stat
				toNetworkLayer(decapsulatePacket(macFrame));
			}
			else
			{
				trace() << "Discarding duplicate overheard packet " << macFrame->getSequenceNumber() << " from node " << macFrame->getSource();
			}
		}
		else
		{
			// It must be an ACK. Just ignore this
			trace() << "Overheard an ACK not addressed to me - addressed to node " << destination << ". Ignoring.";
		}
		
		return;
	}

	// Otherwise, must be addressed to us
	switch(macFrame->getFrameType()){

		// Data addressed to us is ACKed, then passed to network layer
		case BOX_MAC_FRAME_TYPE_DATA: {

			trace() << "Received a data packet from " << source << " addressed to us. Sending ACK";
			collectOutput(BoxMacTwoController::OUTPUT_RECEIVED_DATA); // Add 1 to stat		
			
			// Set the idle listen flag to false to indicate that this listening period was not idle -
			// idle listening means waking up to listen, but not receiveing any messages addressed to us
			idleListen = false;

			// We received a data packet addressed to us. Send an ACK
			plotTrace() << "#MAC_SEND_ACK";
			collectOutput(BoxMacTwoController::OUTPUT_SENT_ACK);
			BoxMacTwoPacket *ackFrame =	new BoxMacTwoPacket("BoxMac ACK", MAC_LAYER_PACKET);
			ackFrame->setSource(SELF_MAC_ADDRESS);
			ackFrame->setDestination(source);
			ackFrame->setFrameType(BOX_MAC_FRAME_TYPE_ACK);
			ackFrame->setBitLength(ackFrameSizeBits);
			ackFrame->setSequenceNumber(currentSequenceNumber++);
			// Note: we first send the frame to the radio (gets added to the radio buffer)
			toRadioLayer(ackFrame);
			// THEN we set to TX. After TXing all packets in buffer, it should automatically go back to previous state (RX)
			toRadioLayer(createRadioCommand(SET_STATE, TX));

			// Do not send duplciate packets up to network layer.
			// De-dupe AFTER sending an ACK. We still want to send ACKs for duplicate messages received. We should only
			// receive duplicate data packets if a previous ACK failed to arrive, therefore keep sending ACKs so that
			// sender can stop re-trying. 
			if(isNotDuplicatePacket(macFrame))
			{
				plotTrace() << "#MAC_REC_UNICAST";					
				// Pass the data frame up to the network layer
				toNetworkLayer(decapsulatePacket(macFrame));
			}
			else
			{
			plotTrace() << "#MAC_REC_UNICAST_DUP";
				trace() << "Not passing duplicate packet " << macFrame->getSequenceNumber() << 
					" from node " << macFrame->getSource() <<
					" up to network layer";
			}

			break;
		}

		// ACKs addressed to us are used to notify the sender that the message currently being transmitted has been received
		case BOX_MAC_FRAME_TYPE_ACK: {
			collectOutput(BoxMacTwoController::OUTPUT_RECEIVED_ACK); // Add 1 to stat					
			trace() << "Received ACK from " << source << ". Passing to sender";
			plotTrace() << "#MAC_REC_ACK";

			// Pass on the ACK to the Sender module (so it knows it can stop transmitting early if appropriate)
			// Note we have to send a DUPLICATE - by default VirtualMac will delete the Mac packet when this function returns
			// (becasue it assumes we are going to decapsulate and get the contained Network packet) 
			send(macFrame->dup(), "toBoxMacSender");

			// Also inform the network layer of the successful send. This may be used to update route quality metrics.
			RoutingControlMessage *sendSucceededMsg = new RoutingControlMessage("routing control msg", NETWORK_CONTROL_COMMAND);
			sendSucceededMsg->setRoutingControlMessageKind(ROUTING_MSG_MAC_SENDING_ACKED);
			sendSucceededMsg->setValue(source);
			toNetworkLayer(sendSucceededMsg);

			break;
		}

		default: {
			opp_error("BoxMacTwoController: Unknown MAC frame type");
		}
	}
}

void BoxMacTwoController::fromNetworkLayer(cPacket *netPkt, int destination)
{
	// Packet arrived from network layer for transmission

	// Create a new MAC frame
	BoxMacTwoPacket *macFrame = new BoxMacTwoPacket("BoxMac data packet", MAC_LAYER_PACKET);
	// Important: set bit length before encapsulation
	macFrame->setBitLength(dataFrameSizeBits);
	// Important: *FIRST* encapsulate the packet THEN set its dest and source
	// (encapsulate sets the destination to default broadcast)
	// encapusulate also increments / sets the sequence number
	encapsulatePacket(macFrame, netPkt);
	macFrame->setSource(SELF_MAC_ADDRESS);
	macFrame->setDestination(destination);
	macFrame->setFrameType(BOX_MAC_FRAME_TYPE_DATA);

	// Give the packet to the Sender module. It is the Sender's responsibility to buffer packets and send them when appropriate,
	// it is the controller's responsibility to tell the Sender when it is okay to send
	send(macFrame, "toBoxMacSender");
}



void BoxMacTwoController::signalSenderOkayToSend()
{
	trace() << "Signalling to Sender okay-to-send";
	changeState(BOX_MAC_STATE_WAITING_FOR_SENDER);

	SenderControlCommand *okayToSendCmd = new SenderControlCommand("Sender control command", MAC_SENDER_CONTROL_COMMAND);
	okayToSendCmd->setSenderControlCommandKind(SENDER_CONTROL_OKAY_TO_SEND);
	send(okayToSendCmd, "toBoxMacSender");
}

void BoxMacTwoController::signalSenderNotOkayToSend()
{
	trace() << "Signalling to Sender do-not-send";
	SenderControlCommand *notOkayToSendCmd = new SenderControlCommand("Sender control command", MAC_SENDER_CONTROL_COMMAND);
	notOkayToSendCmd->setSenderControlCommandKind(SENDER_CONTROL_DO_NOT_SEND);
	send(notOkayToSendCmd, "toBoxMacSender");
}


void BoxMacTwoController::timerFiredCallback(int index)
{
	switch (index) {
		case BOX_MAC_TIMER_LPL_SLEEP: {
			plotTrace() << "#MAC_WAKE";
			trace() << "Sleep period has ended";
			recordSleepDurationStats();
			// The sleep period has finished. We need to signal the sender not to send messages:
			signalSenderNotOkayToSend();
			// And start CCA polling:
			startCcaPolling();
			break;
		}

		case BOX_MAC_TIMER_LISTEN_PERIOD: {
			trace() << "Listening period has ended";
			// We have reached the end of the listening period.
			
			if(idleListen) {
				collectOutput(BoxMacTwoController::OUTPUT_IDLE_LISTENING); // Add 1 to stat
			}

			//Signal the Sender okay to start sending.
			signalSenderOkayToSend();
			break;
		}

		default: {
			opp_error("BoxMacTwoController: Unknown timer type");
		}
	}
}

void BoxMacTwoController::changeState(int newState)
{
	// Implement any state machine logic / transition checks

	if(newState == BOX_MAC_STATE_POLLING_CCA && (boxMacState != BOX_MAC_STATE_STARTUP && boxMacState != BOX_MAC_STATE_SLEEPING)) {
		opp_error("Transitioning to polling state from invalid state. Transitioning from %d", newState);
	}

	if(newState == BOX_MAC_STATE_LISTENING && boxMacState != BOX_MAC_STATE_POLLING_CCA) {
		opp_error("Transitioning to listening state from invalid state. Transitioning from %d", newState);
	}

	if(newState == BOX_MAC_STATE_WAITING_FOR_SENDER && (boxMacState != BOX_MAC_STATE_POLLING_CCA && boxMacState != BOX_MAC_STATE_LISTENING)){
		opp_error("Transitioning to waiting-for-sender state from invalid state. Transitioning from %d", newState);
	}

	if(newState == BOX_MAC_STATE_SLEEPING && (boxMacState != BOX_MAC_STATE_WAITING_FOR_SENDER && boxMacState != BOX_MAC_STATE_WAITING_FOR_SENDER_FROM_SLEEP)){
		opp_error("Transitioning to sleeping state from invalid state. Transitioning from %d", newState);
	}

	if(newState == BOX_MAC_STATE_WAITING_FOR_SENDER_FROM_SLEEP && boxMacState != BOX_MAC_STATE_SLEEPING) {
		opp_error("Transitioning to waiting-for-sender-from-sleep state from invalid state. Transitioning from %d", newState);
	}

	boxMacState = newState;
}

void BoxMacTwoController::recordSleepDurationStats()
{
	double sleptForDuration = simTime().dbl() - sleepStartedAt;
	if(sleptForDuration < 0 || sleepStartedAt == -1)
	{
		opp_error("Error calculating sleep duration");
	}
	collectOutput(OUTPUT_TOTAL_SLEEP_DURATION, "", sleptForDuration);
	sleepStartedAt = -1;
}

void BoxMacTwoController::finishSpecific()
{
	
}