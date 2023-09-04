#include "MmbcrBeaconSender.h"

// Register the module with Omnet
Define_Module(MmbcrBeaconSender);

void MmbcrBeaconSender::initialize()
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
	isSink = false;
	
	// Get the Node ID - we need this for setting the origin / source of beacons
	selfNodeId = getParentModule() // Routing container module
		->getParentModule()  // Communication module
		->getParentModule()  // Node module
		->getIndex();

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

	// Get the storage module - we need this to query remaining battery charge
	supercapacitor = dynamic_cast<Supercapacitor*>(
		getParentModule()   // Containing MMBCR compound module
		->getParentModule()   // Communication module
		->getParentModule()   // Node module
		->getSubmodule("ResourceManager")
		->getSubmodule("EnergySubsystem")
		->getSubmodule("EnergyStorage")
		->getSubmodule("Supercapacitors", 0));

	if(!supercapacitor && selfNodeId != 0)
	{
		opp_error("Error finding supercap module. Currently only supporting non-sink node (ID 0) configurations with 1 supercapacitor storage module.");
	}

	// Set the timer drift for the timer service. If we do not do this all timers will return immediately!
	setTimerDrift(resMgrModule->getCPUClockDrift());
}

void MmbcrBeaconSender::handleMessage(cMessage *msg)
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
			MmbcrBeaconSenderControlMessage *controlMsg = check_and_cast <MmbcrBeaconSenderControlMessage*>(msg);
			switch(controlMsg->getBeaconSenderControlMessageKind())
			{
				case MMBCR_BEACON_SENDER_NEW_BATTERY_CAPACITIES_TO_SINK:
				{
					currentParentBatteryCapacitiesOfPathToSink = controlMsg->getBatteryCapacitiesOfPathToSink();
					updateThisNodesBatteryCapacitiesOfPathToSink();
					break;
				}

				case MMBCR_BEACON_SENDER_UPDATE_MULTIHOP_ETX_TO_ROOT: 
				{
					trace() << "New multihop ETX to root: " << controlMsg->getMultihopEtxToRoot();
					currentMultihopEtxToRoot = controlMsg->getMultihopEtxToRoot();
					break;
				}

				case MMBCR_BEACON_SENDER_NEW_PARENT: 
				{
					trace() << "Updating parent, trickle reset";
					currentParentNodeId = controlMsg->getParentNodeId();
					// Also reset trickle
					resetTrickle();
					break;
				}

				case MMBCR_BEACON_SENDER_RESET_TRICKLE: 
				{
					trace() << "Trickle reset";
					// Reset trickle send interval
					resetTrickle();
					break;
				}

				// Note: this event is called on bootup by the controller to initiate beacon sending
				case MMBCR_BEACON_SENDER_RESET_TRICKLE_AND_PULL: 
				{
					trace() << "Trickle reset and pull";

					// Indicate that the next beacon to be sent should set the pull flag
					setPullFlag = true;

					// Reset trickle send interval
					resetTrickle();

					break;
				}

				default: 
				{
					opp_error("Unknown beacon sender control message kind");
				}
			}
			break;
		}

		case NETWORK_CONTROL_COMMAND:
		{
			RoutingControlMessage *controlMsg = check_and_cast <RoutingControlMessage*>(msg);
			switch(controlMsg->getRoutingControlMessageKind())
			{
				case ROUTING_MSG_SINK_NODE_UPDATE:
				{
					// If this is sink node
					int sinkNodeId = controlMsg->getValue();
					if(sinkNodeId == selfNodeId) 
					{
						isSink = true;
						// The multihop ETX to root is always zero
						currentMultihopEtxToRoot = 0;
						// The path of battery capacities to sink is always empty for the sink
						thisNodesBatteryCapacitiesOfPathToSink.clear();
						currentParentBatteryCapacitiesOfPathToSink.clear();
					}
					break;
				}

				default: 
				{
					opp_error("Unknown network control message kind");
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
			// Empty battery capacities vector
			thisNodesBatteryCapacitiesOfPathToSink.clear();
			currentParentBatteryCapacitiesOfPathToSink.clear();
			break;
		}

		default:
		{
			opp_error("Unknown message type");
		}
	}

	cancelAndDelete(msg);
}

void MmbcrBeaconSender::updateThisNodesBatteryCapacitiesOfPathToSink()
{
	// If we have invalid parent or we are sink, there is no path to sink so clear vector
	if(currentParentNodeId == -1 || isSink)
	{
		thisNodesBatteryCapacitiesOfPathToSink.clear();
	}
	else
	{
		// Calcualte this nodes charge
		double maxPossibleUsableEnergy = supercapacitor->getMaxEnergy() - supercapacitor->getMinEnergy();
		double thisNodeBatteryCapacity = supercapacitor->getAvailableEnergy() / maxPossibleUsableEnergy;
		// Copy the parent battery capacities vector over this nodes list
		thisNodesBatteryCapacitiesOfPathToSink = currentParentBatteryCapacitiesOfPathToSink;
		thisNodesBatteryCapacitiesOfPathToSink.push_back(thisNodeBatteryCapacity);
		trace() << "Adding this node's usable battery capacity " << thisNodeBatteryCapacity 
			<< " to parents chain, chain is now:";
		for (BatteryCapacityVector_t::iterator it = thisNodesBatteryCapacitiesOfPathToSink.begin(); it != thisNodesBatteryCapacitiesOfPathToSink.end(); ++it)
		{
			trace() << std::to_string(*it);
		}
	}
	
}

void MmbcrBeaconSender::resetTrickle()
{
	// Cancel any pending beacon timer
	cancelTimer(MMBCR_BEACON_SENDER_TIMER_SEND_NEXT_BEACON);

	// Reset the sending frequency coefficient to the minimum
	trickleFrequencyCoefficientCurrent = trickleFrequencyCoefficientMin;

	// Restart sending
	sendNextBeacon();
}

void MmbcrBeaconSender::sendNextBeacon()
{
	// Update out energy level
	updateThisNodesBatteryCapacitiesOfPathToSink();

	// Calculate the next sending interval
	calculateSendingInterval();

	// Send the beacon
	
	MmbcrPacket *beacon = new MmbcrPacket("Mmbcr beacon", NETWORK_LAYER_PACKET);
	beacon->setRoutingPacketKind(MMBCR_PACKET_TYPE_BEACON);
	beacon->setDestination(BROADCAST_NETWORK_ADDRESS);
	beacon->setSource(std::to_string(selfNodeId).c_str());
	beacon->setOrigin(std::to_string(selfNodeId).c_str());
	beacon->setHopCount(0);
	beacon->setSequenceNumber(currentBeaconSequenceNumber++);
	beacon->setMultihopEtxToRoot(currentMultihopEtxToRoot);
	beacon->setBitLength(beaconFrameSizeBits);
	beacon->setBatteryCapacitiesOfPathToSink(thisNodesBatteryCapacitiesOfPathToSink);
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
	setTimer(MMBCR_BEACON_SENDER_TIMER_SEND_NEXT_BEACON, trickleSendingIntervalCurrent);

	// Set the next time interval coefficient ready for the next beacon send
	advanceNextTrickleStep();
}

void MmbcrBeaconSender::calculateSendingInterval()
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

void MmbcrBeaconSender::advanceNextTrickleStep()
{
	// The Trickle algorithm doubles the sending time coefficient after each transmission,
	// up to a maximum of trickleFrequencyCoefficientMax
	
	if (!staticFrequency)
	{
		trickleFrequencyCoefficientCurrent = std::min(trickleFrequencyCoefficientCurrent * 2, trickleFrequencyCoefficientMax);
	}
}

void MmbcrBeaconSender::timerFiredCallback(int index)
{
	switch(index) {
		case MMBCR_BEACON_SENDER_TIMER_SEND_NEXT_BEACON: {
			sendNextBeacon();
			break;
		}

		default: {
			opp_error("Unknown timer type");
		}
	}
}

void MmbcrBeaconSender::finishSpecific()
{
	
}