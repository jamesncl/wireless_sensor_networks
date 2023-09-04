#include "CtpRoutingBeaconSender.h"

// Register the module with Omnet
Define_Module(CtpRoutingBeaconSender);

void CtpRoutingBeaconSender::initialize()
{
	// Store NED parameters
	trickleFrequencyCoefficientMax = par("trickleFrequencyCoefficientMax");
	trickleFrequencyCoefficientMin = par("trickleFrequencyCoefficientMin");
	staticFrequency = par("staticFrequency");
	beaconFrameSizeBits = par("beaconFrameSizeBits");
	
	// Initialise private variables
	currentBeaconSequenceNumber = 0;
	currentMultihopEtxToRoot = -1;	// -1 represents invalid
	currentParentNodeId = -1;		// -1 represents no parent
	setPullFlag = false;
	
	// We just need this temporarily in order to set the clock drift on the timer service for this module
	ResourceManager *resMgrModule = check_and_cast <ResourceManager*>(
		getParentModule()   // Routing compound module
		->getParentModule() // Communication module
		->getParentModule() // Node module
		->getSubmodule("ResourceManager")
		->getSubmodule("ResourceManager"));

	if (!resMgrModule) {
		opp_error("Error getting a valid reference to resource manager module");
	}

	// Set the timer drift for the timer service. If we do not do this all timers will return immediately!
	setTimerDrift(resMgrModule->getCPUClockDrift());

	// Get the Node ID - we need this for setting the origin / source of beacons
	selfNodeId = getParentModule() // Routing container module
		->getParentModule()  // Communication module
		->getParentModule()  // Node module
		->getIndex();
}

void CtpRoutingBeaconSender::handleMessage(cMessage *msg)
{
	// A message has arrived from the routing controller
	switch (msg->getKind()) {

		// To use Castalia's timer services in a module which does not extend VirtualMac, VirtualApplication etc.
		// we have to to this bit of joining up: call the timer service's handleTimerMessage function when we receive a timer message
		case TIMER_SERVICE:
		{
			handleTimerMessage(msg, false);
			break;
		}

		case BEACON_SENDER_CONTROL_COMMAND:
		{
			BeaconSenderControlMessage *controlMsg = check_and_cast <BeaconSenderControlMessage*>(msg);
			switch(controlMsg->getBeaconSenderControlMessageKind())
			{
				case BEACON_SENDER_UPDATE_MULTIHOP_ETX_TO_ROOT: {
					trace() << "New multihop ETX to root: " << controlMsg->getMultihopEtxToRoot();
					currentMultihopEtxToRoot = controlMsg->getMultihopEtxToRoot();
					break;
				}

				case BEACON_SENDER_NEW_PARENT: {
					trace() << "Updating parent, trickle reset";
					currentParentNodeId = controlMsg->getParentNodeId();
					// Also reset trickle
					resetTrickle();
					break;
				}

				case BEACON_SENDER_RESET_TRICKLE: {
					trace() << "Trickle reset";
					// Reset trickle send interval
					resetTrickle();
					break;
				}

				// Note: this event is called on bootup by the controller to initiate beacon sending
				case BEACON_SENDER_RESET_TRICKLE_AND_PULL: {
					trace() << "Trickle reset and pull";

					// Indicate that the next beacon to be sent should set the pull flag
					setPullFlag = true;

					// Reset trickle send interval
					resetTrickle();

					break;
				}

				default: {
					opp_error("Unknown control message kind");
				}
			}
			break;
		}

		case OUT_OF_ENERGY:
		{
			// We need to simulate what happens when a node runs out of energy - all state will be lost

			// Reinitialise private variables
			// DO NOT RESET currentBeaconSequenceNumber - no need, and will break duplicate packet checking when other nodes restart
			currentMultihopEtxToRoot = -1;	// -1 represents invalid
			currentParentNodeId = -1;		// -1 represents no parent
			setPullFlag = false;
			// Reset trickle coefficient
			trickleFrequencyCoefficientCurrent = trickleFrequencyCoefficientMin;
			// Cancel any pending timers
			cancelAllTimers();
			break;
		}

		default:
		{
			opp_error("Unknown message type");
		}
	}

	cancelAndDelete(msg);
}

void CtpRoutingBeaconSender::resetTrickle()
{
	// Cancel any pending beacon timer
	cancelTimer(BEACON_SENDER_TIMER_SEND_NEXT_BEACON);

	// Reset the sending frequency coefficient to the minimum
	trickleFrequencyCoefficientCurrent = trickleFrequencyCoefficientMin;

	// Restart sending
	sendNextBeacon();
}

void CtpRoutingBeaconSender::sendNextBeacon()
{
	

	// Calculate the next sending interval
	calculateSendingInterval();

	// Send the beacon
	
	CtpRoutingPacket *beacon = new CtpRoutingPacket("CTP beacon", NETWORK_LAYER_PACKET);
	beacon->setRoutingPacketKind(CTP_ROUTING_PACKET_TYPE_BEACON);
	beacon->setDestination(BROADCAST_NETWORK_ADDRESS);
	beacon->setSource(std::to_string(selfNodeId).c_str());
	beacon->setOrigin(std::to_string(selfNodeId).c_str());
	beacon->setHopCount(0);
	beacon->setSequenceNumber(currentBeaconSequenceNumber++);
	beacon->setMultihopEtxToRoot(currentMultihopEtxToRoot);
	beacon->setBitLength(beaconFrameSizeBits);
	// Set the node ID of the sending node (this node)
	// Used to make sure two nodes do not select each other as parents
	beacon->setParentNodeId(currentParentNodeId);

	// Set the pull flag if necessary
	if(setPullFlag)
	{
		beacon->setPullFlag(true);
		// Only set pull for 1 beacon, so reset the flag
		setPullFlag = false;
		trace() << "Sending beacon number " << currentBeaconSequenceNumber << " with pull flag";
		plotTrace() << "#ROU_SEND_BEACON_WITH_PULL";
	}
	else
	{
		trace() << "Sending beacon number " << currentBeaconSequenceNumber;
		plotTrace() << "#ROU_SEND_BEACON";
	}

	// Note: source will be set as SELF_NETWORK_ADDRESS by the controller (VirtualRouting base class)
	send(beacon, "toController");

	// Set the timer for the next beacon
	setTimer(BEACON_SENDER_TIMER_SEND_NEXT_BEACON, trickleSendingIntervalCurrent);

	// Set the next time interval coefficient ready for the next beacon send
	advanceNextTrickleStep();
}

void CtpRoutingBeaconSender::calculateSendingInterval()
{
	// The interval used in the Trickle algorithm is a random value
	// between x/2 and x, where x is trickleFrequencyCoefficientCurrent
	// After each transmission, x is halved
	double lower = trickleFrequencyCoefficientCurrent / 2;
	double upper = trickleFrequencyCoefficientCurrent;
	trickleSendingIntervalCurrent = dblrand() * (upper - lower) + lower; //dblrand produces random number between 0 and 1
	
	//trace() << "Trickle: sending interval max is " << upper;
	//trace() << "Trickle: sending interval min is " << lower;
	//trace() << "Trickle: random interval is " << trickleSendingIntervalCurrent;
}

void CtpRoutingBeaconSender::advanceNextTrickleStep()
{
	// The Trickle algorithm doubles the sending time coefficient after each transmission,
	// up to a maximum of trickleFrequencyCoefficientMax
	
	if (!staticFrequency)
	{
		trickleFrequencyCoefficientCurrent = std::min(trickleFrequencyCoefficientCurrent * 2, trickleFrequencyCoefficientMax);
	}
}

void CtpRoutingBeaconSender::timerFiredCallback(int index)
{
	switch(index) {
		case BEACON_SENDER_TIMER_SEND_NEXT_BEACON: {
			sendNextBeacon();
			break;
		}

		default: {
			opp_error("Unknown timer type");
		}
	}
}

void CtpRoutingBeaconSender::finishSpecific()
{
	
}