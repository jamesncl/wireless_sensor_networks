#include "BoxMacTwoSender.h"

// Register the module in Omnet++
Define_Module(BoxMacTwoSender);

// Const strings for output names
const char * BoxMacTwoSender::OUTPUT_SENT_UNICAST = "BoxMac Sent unicast";
const char * BoxMacTwoSender::OUTPUT_SENT_BROADCAST = "BoxMac Sent broadcast";
const char * BoxMacTwoSender::OUTPUT_BACKOFF_INITIAL = "BoxMac Backoff initial";
const char * BoxMacTwoSender::OUTPUT_BACKOFF_CONGESTION = "BoxMac Backoff congestion";
const char * BoxMacTwoSender::OUTPUT_MSG_NOT_ACKED = "BoxMac Msg not acked";
const char * BoxMacTwoSender::OUTPUT_MESSAGES_IN_UNICAST_MESSAGE_TRAIN = "BoxMac Messages in unicast train";
const char * BoxMacTwoSender::OUTPUT_UNICAST_MESSAGE_TRAIN_DURATION = "BoxMac Message train duration";

void BoxMacTwoSender::initialize()
{
	// Store NED parameters
	initialBackoffMin = par("initialBackoffMin");
	initialBackoffRange = par("initialBackoffMax").doubleValue() - par("initialBackoffMin").doubleValue();
	congestionBackoffMin = par("congestionBackoffMin");
	congestionBackoffRange = par("congestionBackoffMax").doubleValue() - par("congestionBackoffMin").doubleValue();
	maxMessageBufferSize = par("maxMessageBufferSize");
	interTransmissionAckReceiveDelay = par("interTransmissionAckReceiveDelay");
	interTransmissionBroadcastDelay = par("interTransmissionBroadcastDelay");
	waitForRxTransitionDelayTime = par("waitForRxTransitionDelayTime");

	// We need to get the sleepTime parameter from the controller, add the padding and 
	// use this as the total transmission-train time such that the repeated transmissions cover a whole sleep interval plus a margin
	transmissionTimeToOverlapLplWakeInterval = getParentModule()->getSubmodule("Controller")->par("sleepTime").doubleValue()
	+ par("lplWakeIntervalSendPadding").doubleValue();

	// Initialise variables
	initialisePrivateVariables();

	/* Get a valid references to the Radio module, so that we can make direct calls to Radio isChannelClear method

		This means we are directly calling a public method on the Radio module, rather than using Omnet message passing.
		This design decision (which is borrowed from Castalia's other MAC implementations) was discussed with
		Thanassis Boulis and justified as follows:

		Thanassis Boulis:
		"OMNeT's messages are great to use when you have asynchronous events. For example MAC sends a packet to the Radio 
		and expects it to be transmitted. The transmission of the packet though is irrelevant to the MAC's other functions. 
		MAC should not have to wait for the Radio to respond before continuing with its own operation. Messages are of 
		great use in this case.

		In a real system as well we se an analogous behaviour: the processor would write the packet to a buffer and set 
		the bits of some other buffers (or set some pins) and then the Radio will take over, leaving the processor free 
		to continue with its operation. 

		Figuring out if the channel is clear however, is usually a synchronous operation. The processor just reads the 
		value of a register (it might perform an I/O operation which can also be synchronous). In Castalia we are modelling 
		this by a function call. The result is immediately known by the returned value of the function. This also makes the 
		code simpler as you point out, but the main motivation was to model this synchronous operation of a real system."

	 */
	radioModule = check_and_cast <Radio*>(getParentModule()->getParentModule()->getSubmodule("Radio"));
	
	// We just need this temporarily in order to set the clock drift on the timer service for this module
	ResourceManager *resMgrModule = check_and_cast <ResourceManager*>(getParentModule()->getParentModule()->getParentModule()->getSubmodule("ResourceManager")
		->getSubmodule("ResourceManager"));

	if (!resMgrModule || !radioModule) {
		opp_error("Error getting a valid reference to radio module");
	}

	// Set the timer drift for the timer service. If we do not do this all timers will return immediately!
	setTimerDrift(resMgrModule->getCPUClockDrift());

	// Declare outputs
	declareOutput(OUTPUT_SENT_UNICAST);
	declareOutput(OUTPUT_SENT_BROADCAST);
	declareOutput(OUTPUT_BACKOFF_INITIAL);
	declareOutput(OUTPUT_BACKOFF_CONGESTION);
	declareOutput(OUTPUT_MSG_NOT_ACKED);
	declareHistogram(OUTPUT_UNICAST_MESSAGE_TRAIN_DURATION, 0, 0.6, 20);
	declareHistogram(OUTPUT_MESSAGES_IN_UNICAST_MESSAGE_TRAIN, 0, 100, 20);
}

void BoxMacTwoSender::initialisePrivateVariables()
{
	controllerDirectiveState = BOX_MAC_SENDER_DIRECTIVE_DO_NOT_SEND;
	sendState = BOX_MAC_SENDER_STATE_IDLE;
	numberOfSendsDone = 0;
	hasSendingLplWakeIntervalExpired = false;
	countNumberOfMessagesSentInTrain = 0;
	trainStartTime = -1;
}

void BoxMacTwoSender::handleMessage(cMessage *msg)
{
	// A message has arrived from the controller
	switch (msg->getKind()) {

		// To use Castalia's timer services in a module which does not extend VirtualMac, VirtualApplication etc.
		// we have to to this bit of joining up: call the timer service's handleTimerMessage function when we receive a timer message
		case TIMER_SERVICE:
		{
			handleTimerMessage(msg, false);
			delete msg;
			break;
		}

		// We are getting a command
		case MAC_SENDER_CONTROL_COMMAND: {

			SenderControlCommand *cmd = check_and_cast <SenderControlCommand*>(msg);

			switch(cmd->getSenderControlCommandKind()) {

				case SENDER_CONTROL_OKAY_TO_SEND: {
					//trace() << "Received 'okay to send'";
					controllerDirectiveState = BOX_MAC_SENDER_DIRECTIVE_OKAY_TO_SEND;
					startSendingNextMessageTrainInQueue();
					break;
				}

				case SENDER_CONTROL_DO_NOT_SEND: {
					//trace() << "Received 'do not send'";
					controllerDirectiveState = BOX_MAC_SENDER_DIRECTIVE_DO_NOT_SEND;
					// as long as we are not in the middle of a transmit (and waiting for an ACK),
					// safe just to cancel all timers and set state to idle to stop any message sending 
					// which may be occurring
					if(sendState != BOX_MAC_SENDER_STATE_TRANSMITTING) 
					{
						trace() << "Not in transmitting state, so cancelling any timers and going to idle";
						cancelTimer(BOX_MAC_SENDER_TIMER_LPL_WAKE_INTERVAL);
						cancelTimer(BOX_MAC_SENDER_TIMER_INTER_TRANSMISSION_DELAY);
						cancelTimer(BOX_MAC_SENDER_TIMER_BACKOFF);
						changeState(BOX_MAC_SENDER_STATE_IDLE);
					}
					else{
						trace() << "In the middle of a transmission, so allowing to finish";
					}
					break;
				}
			}

			// Delete the message
			cancelAndDelete(cmd);
			break;
		}

		// We are getting a packet
		case MAC_LAYER_PACKET: {
			BoxMacTwoPacket *macFrame = check_and_cast <BoxMacTwoPacket*>(msg);
			
			switch(macFrame->getFrameType()) {
		
				// It's a data packet for transmission
				case BOX_MAC_FRAME_TYPE_DATA: {
					trace() << "Received data packet for transmission to " << macFrame->getDestination();

					if(sendQueue.size() >= maxMessageBufferSize)
					{
						trace() << "WARNING: send buffer full, discarding packet";
						plotTrace() << "#MAC_BUFFER_OVERFLOW";
						delete macFrame;
					}
					else
					{
						// Add to the queue
						//plotTrace() << "#MAC_BUFFERED";
						sendQueue.push(macFrame);
						trace() << "Added to queue - queue size now " << sendQueue.size();
					}
					
					// If we are okay to send, and we are not already sending (i.e. we are idle), trigger sending
					if(controllerDirectiveState == BOX_MAC_SENDER_DIRECTIVE_OKAY_TO_SEND
					 	&& sendState == BOX_MAC_SENDER_STATE_IDLE)  {
						//trace() << "Okay to send, and send state is idle. Triggering start sending next message train in queue";
						startSendingNextMessageTrainInQueue();
					}
					break;
				}

				// Were getting an ACK for our transmission.
				case BOX_MAC_FRAME_TYPE_ACK: {

					trace() << "Message was ACKed. Ending transmission early";
					plotTrace() << "#MAC_REC_ACK";

					if(sendState != BOX_MAC_SENDER_STATE_TRANSMITTING) 
					{
						trace() << "WARNING - received ACK when not in transmitting phase. Something went wrong?";
						delete msg;
						break;
					}
					else
					{
						// For stats collection - we want to know how long / how many messages are used in transmissions
						// Important - DO THIS BEFORE DELETING THE MESSAGE!
						if(sendQueue.front()->getDestination() != BROADCAST_MAC_ADDRESS)
						{
							collectUnicastMessageTrainStats();
						}

						// Cancel running transmission timers for this current message train
						cancelTimer(BOX_MAC_SENDER_TIMER_LPL_WAKE_INTERVAL);
						cancelTimer(BOX_MAC_SENDER_TIMER_INTER_TRANSMISSION_DELAY);
						// Delete the ACK, no need for it anymore
						delete msg;
						// Delete the message from the queue. It was successfully sent.
						trace() << "Delete the message from the queue. It was successfully sent.";
						cancelAndDelete(sendQueue.front());
						sendQueue.pop();

						// Start the next message send train (if there are any)
						//trace() << "Message was ACKed, so start the next message send train (if there are any)";
						changeState(BOX_MAC_SENDER_STATE_START_NEXT_TRAIN);
						startSendingNextMessageTrainInQueue();
						break;
					}
				}
			}
			break;
		}

		case OUT_OF_ENERGY:
		{
			// We need to simulate what happens when a node runs out of energy - all state will be lost
			trace() << "OUT OF ENERGY. Clearing state.";

			// Reinitialise private variables
			initialisePrivateVariables();
			
			// Clear the send queue
			clearSendQueue();

			// Cancel any pending timers
			cancelAllTimers();
			
			cancelAndDelete(msg);
			break;
		}
	}
}

void BoxMacTwoSender::startSendingNextMessageTrainInQueue()
{
	// Ask the radio to change to RX mode (we will need this to do the backoff first)
	RadioControlCommand *cmd = new RadioControlCommand("Radio control command", RADIO_CONTROL_COMMAND);
	cmd->setRadioControlCommandKind(SET_STATE);
	cmd->setState(RX);
	send(cmd, "toBoxMacController");

	//trace() << "Asked to start sending the next message in the queue. Checking queue";
	// If there are no more messages in the queue, just go to idle
	if(sendQueue.empty())
	{
		trace() << "Send queue empty, so going to idle";
		finishedSending();
	}
	else
	{
		//trace() << "Queue size " << sendQueue.size() << ", so signalling controller that we're going to send";
		// Signal the controller that we're sending
		BoxMacControlMessage *sendingMsg = new BoxMacControlMessage("Mac control command", MAC_CONTROL_COMMAND); 
		sendingMsg->setMacControlCommandKind(SENDER_IS_SENDING);
		send(sendingMsg, "toBoxMacController");

		// Start sending the next message in the queue
		changeState(BOX_MAC_SENDER_STATE_START_NEXT_TRAIN);
		advanceMessageSendState();
	}
}

void BoxMacTwoSender::advanceMessageSendState()
{
	// Check we have messages to send and that it's okay to send now
	if(!sendQueue.empty() && controllerDirectiveState != BOX_MAC_SENDER_DIRECTIVE_DO_NOT_SEND)
	{
		switch(sendState) {

			// We need to start a new message train
			case BOX_MAC_SENDER_STATE_START_NEXT_TRAIN: {

				// For stats collection
				countNumberOfMessagesSentInTrain = 0;
				trainStartTime = simTime().dbl();

				if(sendQueue.front()->getDestination() == BROADCAST_MAC_ADDRESS)
				{
					collectOutput(OUTPUT_SENT_BROADCAST);
					plotTrace() << "#MAC_SEND_BROADCAST";
				}
				else
				{
					collectOutput(OUTPUT_SENT_UNICAST);
					plotTrace() << "#MAC_SEND_UNICAST " << sendQueue.front()->getDestination();	
				}

				//trace() << "State: Starting the next message train";
				// Set a timer for the total transmission-train time allowed for the train
				hasSendingLplWakeIntervalExpired = false; // Reset the timer expired flag in case it has been set on an earlier transmission
				//trace() << "Setting total LP period send timer to " << transmissionTimeToOverlapLplWakeInterval;
				setTimer(BOX_MAC_SENDER_TIMER_LPL_WAKE_INTERVAL, transmissionTimeToOverlapLplWakeInterval);
				// Now we can send the first message in the train. Set the state
				changeState(BOX_MAC_SENDER_STATE_START_NEXT_MSG_IN_TRAIN);
				// and call this function again
				advanceMessageSendState();
				break;
			}

			// We need to start sending the next message in the train
			case BOX_MAC_SENDER_STATE_START_NEXT_MSG_IN_TRAIN: {

				// First, check that the sending LPL wake interval hasn't expired
				if(!hasSendingLplWakeIntervalExpired)
				{
					//trace() << "State: LPL period hasn't expired. Starting next message in train";
					// Do initial backoff
					collectOutput(BoxMacTwoSender::OUTPUT_BACKOFF_INITIAL);
				
					double initialBackoffTime = initialBackoffMin + dblrand() * initialBackoffRange;
					//plotTrace() << "#MAC_BACKOFF_I Doing initial backoff for " << initialBackoffTime << " sim time";
					//trace() << "Setting initial backoff timer for " << initialBackoffTime;
					setTimer(BOX_MAC_SENDER_TIMER_BACKOFF, initialBackoffTime);
					//trace() << "State: waiting for intial backoff";
					changeState(BOX_MAC_SENDER_STATE_BACKING_OFF_INITIAL);
					// When the backoff timer fires, the advanceMessageSendState function will be called again 
					// with state BOX_MAC_SENDER_STATE_BACKING_OFF_COMPLETE
				}
				else
				{
					// If the LPL wake interval has expired, and we have waited long enough for an ACK,
					// we need to stop transmitting the current mesage train and clean up
					
					// If the message was not a broadcast, we were expecting an ACK but did not get one.
					// Notify the routing later (via the controller) that the message send failed
					if(sendQueue.front()->getDestination() != BROADCAST_MAC_ADDRESS)
					{
						// Note: his will only happen if the message is not ACKed.
						trace() << "State: Completed transmissions over entire LPL wake interval and message was not ACKed. "
							<< "Destination not reachable. Informing controller of fail";
						collectOutput(BoxMacTwoSender::OUTPUT_MSG_NOT_ACKED);
						plotTrace() << "#MAC_UNICAST_FAILED_DROPPED";

						BoxMacControlMessage *failedMsg = new BoxMacControlMessage("Mac control command", MAC_CONTROL_COMMAND); 
						failedMsg->setMacControlCommandKind(SENDING_FAILED_NO_ACK);
						failedMsg->setValue(sendQueue.front()->getDestination()); // Set the value as the node we were trying to transmit to
						send(failedMsg, "toBoxMacController");

						// Stats collection - we want to know how long / how many preamble messages are sent in transmissions
						collectUnicastMessageTrainStats();
					}
					else
					{
						trace() << "State: Finished broadcasting message";
					}

					// Remove the message from the queue and delete it. We're done with it.
					trace() << "Deleting the message from queue.";
					cancelAndDelete(sendQueue.front());
					sendQueue.pop();

					// Change state
					//trace() << "This message transimssion has finished. Ask to start the next message in train.";
					changeState(BOX_MAC_SENDER_STATE_START_NEXT_TRAIN);
					// Send the next message
					startSendingNextMessageTrainInQueue();
				}

				break;
			}

			case BOX_MAC_SENDER_STATE_BACKING_OFF_COMPLETE: {

				//trace() << "State: Checking CCA";
				// Check the CCA status
				switch(radioModule->isChannelClear()) {
					
					// Channel is clear. Transmit the message
					case CLEAR:{

						BoxMacTwoPacket *packetToSend = sendQueue.front();
						trace() << "CCA clear. Transmitting BoxMac packet type " << packetToSend->getFrameType()
							<< " seqNo " << packetToSend->getSequenceNumber() << " to " << packetToSend->getDestination();
						// Send a DUPLICATE of the next message in the queue to the radio. We need to send
						// duplicates because we will need to send multiple times. 
						send(packetToSend->dup(), "toBoxMacController");

						// THEN turn the radio to TX mode so it sends the message (it will automatically turn back to RX after send)
						RadioControlCommand *txCmd = new RadioControlCommand("Radio control command", RADIO_CONTROL_COMMAND);
						txCmd->setRadioControlCommandKind(SET_STATE);
						txCmd->setState(TX);
						send(txCmd, "toBoxMacController");

						// For stats collection
						countNumberOfMessagesSentInTrain++;

						// Set a timer for this individual transmission
						// If this is a broadcast transmission, we wait a short time as we are not listening for an ACK
						if(packetToSend->getDestination() == BROADCAST_MAC_ADDRESS) 
						{
							setTimer(BOX_MAC_SENDER_TIMER_INTER_TRANSMISSION_DELAY, interTransmissionBroadcastDelay);
						}
						// Otherwise it is unicast - we wait a longer period as we are listening for an ACK
						else 
						{					
							setTimer(BOX_MAC_SENDER_TIMER_INTER_TRANSMISSION_DELAY, interTransmissionAckReceiveDelay);
						}
						
						changeState(BOX_MAC_SENDER_STATE_TRANSMITTING);
						// If the receive delay timer fires, the advanceMessageSendState function will be called again 
						// with state BOX_MAC_SENDER_STATE_START_NEXT_MSG_IN_TRAIN
						// If an ACK is received, the advanceMessageSendState function will be called again 
						// with state BOX_MAC_SENDER_STATE_START_NEXT_TRAIN
						break;
					}
			
					// Channel is busy. We need to do a congestion backoff
					case BUSY:{
						trace() << "CCA Busy";
						//plotTrace() << "#MAC_CANT_SEND_CCA_BUSY";

						collectOutput(BoxMacTwoSender::OUTPUT_BACKOFF_CONGESTION);				
						double congestionBackoffTime = congestionBackoffMin + dblrand() * congestionBackoffRange;
						//plotTrace() << "#MAC_BACKOFF_C CCA busy. Doing congestion backoff for " << congestionBackoffTime << " sim time";
						setTimer(BOX_MAC_SENDER_TIMER_BACKOFF, congestionBackoffTime);
						changeState(BOX_MAC_SENDER_STATE_BACKING_OFF_CONGESTION);
						// When the backoff timer fires, the advanceMessageSendState function will be called again 
						// with state BOX_MAC_SENDER_STATE_BACKING_OFF_COMPLETE
						break;
					}
				
					// CS_NOT_VALID means that the radio is not in RX. Shouldn't happen!
					case CS_NOT_VALID: {
						trace() << "WARNING - Polled CCA, but radio not in RX mode. This will happen if the controller happens to be in the middle of sending an ACK. Okay as long as it doesn't happen a lot";
						setTimer(BOX_MAC_SENDER_TIMER_WAIT_FOR_RADIO_RX_TRANSITION_DELAY, waitForRxTransitionDelayTime);
						changeState(BOX_MAC_SENDER_STATE_WAITING_FOR_RX_TRANSITION_DELAY);
						break;
					}
			
					// CS_NOT_VALID_YET means we are in RX, just not long enough
					case CS_NOT_VALID_YET:{
						trace() << "WARNING - Polled CCA, but radio not in RX mode for long enough. This may be because the backoff was quite short. Probably okay as long as it doesn't happen a lot";
						setTimer(BOX_MAC_SENDER_TIMER_WAIT_FOR_RADIO_RX_TRANSITION_DELAY, waitForRxTransitionDelayTime);
						changeState(BOX_MAC_SENDER_STATE_WAITING_FOR_RX_TRANSITION_DELAY);
						break;
					}
				}
				break;
			}

			default: {
				opp_error("Advancing send state from an invalid state.");
			}
		}
	}
	else
	{
		trace() << "WARNING: Asked to advance message state, but no messages in queue or not okay to send. Switching to idle.";
		finishedSending();
	}
}

void BoxMacTwoSender::timerFiredCallback(int index)
{
	switch (index) {

		case BOX_MAC_SENDER_TIMER_INTER_TRANSMISSION_DELAY:	{
			//trace() << "Finished waiting for inter-transmission delay (if this was a unicast, an ACK was not received)";
			// An individual transmission in a train has finished after delay, and not ACKed. Send the next in the train.
			changeState(BOX_MAC_SENDER_STATE_START_NEXT_MSG_IN_TRAIN);
			advanceMessageSendState();
			break;
		}

		case BOX_MAC_SENDER_TIMER_LPL_WAKE_INTERVAL: {

			trace() << "LPL wake interval transmit timer expired";
			// The timer specifying how long we should keep transmitting messages to cover an entire LPL
			// wake period has expired. However we shouldn't give up hope yet - there may be an ACK on its
			// way to us (unlikely but it will occasionally happen)
			// Therefore just set a flag here, which is checked after waiting for the ACK receive delay
			hasSendingLplWakeIntervalExpired = true;
			break;
		}

		case BOX_MAC_SENDER_TIMER_BACKOFF: {
			//trace() << "Backoff complete";
			// Backoff complete
			changeState(BOX_MAC_SENDER_STATE_BACKING_OFF_COMPLETE);
			// Go to the next send state
			advanceMessageSendState();
			break;
		}

		case BOX_MAC_SENDER_TIMER_WAIT_FOR_RADIO_RX_TRANSITION_DELAY: {
			//trace() << "Finished waiting for RX transition delay";
			// TO simplify things, just consider this as a different type of backoff
			changeState(BOX_MAC_SENDER_STATE_BACKING_OFF_COMPLETE);
			advanceMessageSendState();
			break;
		}

		default: {
			opp_error("Unknown timer type");
		}
	}
}

void BoxMacTwoSender::collectUnicastMessageTrainStats()
{
	collectHistogram(BoxMacTwoSender::OUTPUT_MESSAGES_IN_UNICAST_MESSAGE_TRAIN, countNumberOfMessagesSentInTrain);
	double trainDuration = simTime().dbl() - trainStartTime;
	if(trainDuration < 0 | trainStartTime == -1)
	{
		opp_error("Error calculating message train duration. Was train start time set correctly?");
	}
	collectHistogram(BoxMacTwoSender::OUTPUT_UNICAST_MESSAGE_TRAIN_DURATION, trainDuration);
}

void BoxMacTwoSender::finishedSending()
{
	trace() << "Finshed sending. informing controller, going to idle state";

	// Signal the controller that we've finished sending
	BoxMacControlMessage *finishedMsg = new BoxMacControlMessage("Mac control command", MAC_CONTROL_COMMAND); 
	finishedMsg->setMacControlCommandKind(SENDER_FINISHED_SENDING);
	send(finishedMsg, "toBoxMacController");

	changeState(BOX_MAC_SENDER_STATE_IDLE);
}

void BoxMacTwoSender::changeState(int newState)
{
	// Implement any state machine logic / transition checks
	//trace() << "Changing to state " << newState;
	sendState = newState;
}

void BoxMacTwoSender::clearSendQueue()
{
	// Remove any packets left in sendQueue
	while (!sendQueue.empty()) {
		BoxMacTwoPacket *pkt = sendQueue.front();
		sendQueue.pop();
		cancelAndDelete(pkt);
	}
}

void BoxMacTwoSender::finishSpecific()
{
	clearSendQueue();
}