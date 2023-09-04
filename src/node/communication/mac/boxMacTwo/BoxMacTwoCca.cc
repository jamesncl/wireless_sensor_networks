#include "BoxMacTwoCca.h"

// Register the module in Omnet++
Define_Module(BoxMacTwoCca);

const char * BoxMacTwoCca::OUTPUT_CCA_BUSY = "BoxMac CCA busy";
const char * BoxMacTwoCca::OUTPUT_CCA_CLEAR = "BoxMac CCA clear";

void BoxMacTwoCca::initialize()
{
	maxCcaChecks = par("maxCcaChecks");
	minRequiredBusyCcaResults = par("minRequiredBusyCcaResults");
	timeForOneCcaCheck = par("timeForOneCcaCheck");
	waitForRxTransitionDelayTime = par("waitForRxTransitionDelayTime");
	pollingCcaPower = par("pollingCcaPower");
	numberOfCcaPollsMade = 0;
	noOfBusyCcaResults = 0;
	boxMacCcaState = BOX_MAC_CCA_STATE_IDLE;
	declareOutput(OUTPUT_CCA_BUSY);
	declareOutput(OUTPUT_CCA_CLEAR);

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

	if (!radioModule) {
		opp_error("BoxMacTwoCca: Error getting a valid reference to radio module");
	}

	// We just need this temporarily in order to set the clock drift on the timer service for this module
	ResourceManager *resMgrModule = check_and_cast <ResourceManager*>(getParentModule()->getParentModule()->getParentModule()->getSubmodule("ResourceManager")
		->getSubmodule("ResourceManager"));

	if (!resMgrModule || !radioModule) {
		opp_error("Error getting a valid reference to radio module");
	}
	
	// Set the timer drift for the timer service. If we do not do this all timers will return immediately!
	setTimerDrift(resMgrModule->getCPUClockDrift());
}

void BoxMacTwoCca::handleMessage(cMessage *msg)
{
	int msgKind = msg->getKind();

	switch(msgKind) {

		// To use Castalia's timer services in a module which does not extend VirtualMac, VirtualApplication etc.
		// we have to to this bit of joining up: call the timer service's handleTimerMessage function when we receive a timer message
		case TIMER_SERVICE:
		{
			handleTimerMessage(msg, false);
			break;
		}

		case CCA_CONTROL_COMMAND: {

			CcaControlCommand *ccaControlCmd = check_and_cast<CcaControlCommand*>(msg);
			switch(ccaControlCmd->getCcaControlCommandKind()) {
				
				case CCA_CONTROL_START_POLLING: {
					
					if(boxMacCcaState == BOX_MAC_CCA_STATE_POLLING) {
						opp_error("Asked to start polling, but already polling - this shouldn't happen!");
					}

					trace() << "Received signal to start polling";
					initialiseCcaPolling();
					break;
				}
				
				default: {
					opp_error("BoxMacTwoCca: Unexpected CCA control command kind");
				}
			}

			break;
		}

		case OUT_OF_ENERGY:
		{
			// We need to simulate what happens when a node runs out of energy - all state will be lost

			// Reinitialise private variables
			numberOfCcaPollsMade = 0;
			noOfBusyCcaResults = 0;
			
			// Cancel any pending timers
			cancelAllTimers();

			// If polling, reduce power drawn back to zero
			if(boxMacCcaState == BOX_MAC_CCA_STATE_POLLING)
			{
				powerChange(-pollingCcaPower);
			}

			// Reset state to idle
			boxMacCcaState = BOX_MAC_CCA_STATE_IDLE;

			break;
		}

		default: {
			opp_error("BoxMacTwoCca: Unexpected message type");
		}
	}

	cancelAndDelete(msg);
}



void BoxMacTwoCca::initialiseCcaPolling()
{
	numberOfCcaPollsMade = 0;
	noOfBusyCcaResults = 0;

	// Ask the radio to change to RX mode
	RadioControlCommand *cmd = new RadioControlCommand("Radio control command", RADIO_CONTROL_COMMAND);
	cmd->setRadioControlCommandKind(SET_STATE);
	cmd->setState(RX);
	send(cmd, "toBoxMacController");

	// Then we need to wait for long enough for the transition to complete (otherwise we get invalid CCA results)
	trace() << "Asked the Radio to go to RX, waiting for transition to complete";
	setTimer(BOX_MAC_CCA_TIMER_WAIT_FOR_RADIO_RX_TRANSITION_DELAY, waitForRxTransitionDelayTime);

	// Update the power drawn for this module - experimental data shows polling CCA consumes an additional amount of power
	powerChange(pollingCcaPower);
	boxMacCcaState = BOX_MAC_CCA_STATE_POLLING;
}

void BoxMacTwoCca::requestCca()
{
	//trace() << "Requesting CCA result from Radio";
	switch(radioModule->isChannelClear()) {
		
		// Channel is clear
		case CLEAR:{
			//trace() << "Channel is clear";
			numberOfCcaPollsMade++;
			break;
		}

		// Channel is busy. Increment number of positive results.
		case BUSY:{
			trace() << "CCA poll result - Channel is busy";
			numberOfCcaPollsMade++;
			noOfBusyCcaResults++;
			break;
		}
	
		// CS_NOT_VALID means that the radio is not in RX. Shouldn't happen!
		case CS_NOT_VALID: {
			trace() << "WARNING: Polled CCA, but radio has not yet transitioned to RX - " <<
			"this is probably okay as long as it only happens on the first poll.";
			break;
		}

		// CS_NOT_VALID_YET means we are in RX, just not long enough
		case CS_NOT_VALID_YET:{
			trace() << "WARNING: Polled CCA, but radio has not been in RX long enough " <<
				"so returned CS_NOT_VALID_YET. This is probably okay as long as it only happens on the first poll.";
			break;
		}
	}

	// Check if we have registered the required number of 'busy' poll results to constitute a busy channel
	if(noOfBusyCcaResults >= minRequiredBusyCcaResults)
	{
		//plotTrace() << "#MAC_CCA_BUSY Registered the number of positive CCA results to constitute a busy medium. Signalling controller.";
		collectOutput(OUTPUT_CCA_BUSY);
		// Signal the MAC. Stop polling (don't set the poll timer)
		BoxMacControlMessage *ccaBusyMsg = new BoxMacControlMessage("MAC control message", MAC_CONTROL_COMMAND);
		ccaBusyMsg->setMacControlCommandKind(CCA_CHECK_CHANNEL_IS_BUSY);
		send(ccaBusyMsg, "toBoxMacController");
		// Stopped polling - update the power drawn
		powerChange(-pollingCcaPower);
		boxMacCcaState = BOX_MAC_CCA_STATE_IDLE;
	}
	// Check if we've reached the end of the polling period
	else if(numberOfCcaPollsMade >= maxCcaChecks)
	{
		//plotTrace() << "#MAC_CCA_CLEAR Polled " << std::to_string(maxCcaChecks) << " times without registering busy medium. Medium is clear. Signalling controller.";
		collectOutput(OUTPUT_CCA_CLEAR);
		// Signal the MAC. Stop polling (don't set the poll timer)
		BoxMacControlMessage *ccaClearMsg = new BoxMacControlMessage("MAC control message", MAC_CONTROL_COMMAND);
		ccaClearMsg->setMacControlCommandKind(CCA_CHECK_CHANNEL_IS_CLEAR);
		send(ccaClearMsg, "toBoxMacController");
		// Stopped polling - update the power drawn
		powerChange(-pollingCcaPower);
		boxMacCcaState = BOX_MAC_CCA_STATE_IDLE;
	}
	// Otherwise, we haven't finished polling
	else
	{
		// Start a timer for the next poll after the appropriate delay.
		setTimer(BOX_MAC_CCA_TIMER_POLL_DELAY, timeForOneCcaCheck);
	}
}

void BoxMacTwoCca::timerFiredCallback(int index)
{
	switch (index) {
		
		case BOX_MAC_CCA_TIMER_WAIT_FOR_RADIO_RX_TRANSITION_DELAY:
		{
			// We have finished waiting for the radio to transition to RX. Make our first poll. 
			requestCca();
			break;
		}
		
		case BOX_MAC_CCA_TIMER_POLL_DELAY: 
		{
			// We have waited for the inter-poll delay, make the next poll.
			requestCca();
			break;
		}

		default: {
			opp_error("BoxMacTwoCca: Unknown timer type");
		}
	}
}

void BoxMacTwoCca::finishSpecific()
{
	
}