#include "RicerStateContext.h"
#include "RicerMac.h"

RicerStateContext::RicerStateContext() 
	: currentState(&stateInitiateReceive) // Current state initialised as Receive
{
	initialisePrivateVariables();
}

void RicerStateContext::initialisePrivateVariables()
{
	m_currentExponentialBackoffValue = 0;	
	m_nodeToSendTo = -1;
	m_needToSendReadyToReceiveBeacon = false;
	m_needToWakeToSendNewPacket = false;
	m_needToWakeForReceive = false;
}

void RicerStateContext::initialiseContext(RicerMacInterface *moduleInterface, RicerMacParameters parameters)
{
	macModuleInterface = moduleInterface;
	macParameters = parameters;
	binaryExponentialBackoff.initialise(
		parameters.binaryExponentialBackoffSlotDuration, 
		parameters.binaryExponentialBackoffMaxExponent,
		&randomNumberGenerator);
	//macModuleInterface->log("Initialised context");
}

void RicerStateContext::clearAllState()
{
	while(!m_txBuffer.empty()) 
	{
		BufferedMacPacketQueueItem pktAndRetries = m_txBuffer.back();
		m_txBuffer.pop_back();
		macModuleInterface->cleanUpAndRemoveMessage(pktAndRetries.packet);
	}

	resetBackoff();
	currentState = &stateInitiateReceive;
	initialisePrivateVariables();
}

int RicerStateContext::howManyUnicastPacketsInBuffer()
{
	int count = 0;
	vector<BufferedMacPacketQueueItem>::iterator it;

	for(it = m_txBuffer.begin(); it != m_txBuffer.end(); it++)
	{
		if(!(*it).packet->getIsDataForBroadcast())
		{
			count++;
		}
	}

	return count;
}

int RicerStateContext::howManyBroadcastPacketsInBuffer()
{
	int count = 0;
	vector<BufferedMacPacketQueueItem>::iterator it;

	for(it = m_txBuffer.begin(); it != m_txBuffer.end(); it++)
	{
		if((*it).packet->getIsDataForBroadcast())
		{
			count++;
		}
	}

	return count;
}

RicerMacParameters RicerStateContext::getMacParameters()
{
	return macParameters;
}

std::string RicerStateContext::getCurrentStateName()
{
	return currentState->getCurrentStateName();
}

void RicerStateContext::changeToStateListenForData()
{
	currentState = &stateListenForData;
	currentState->start(this, macModuleInterface);
}

void RicerStateContext::changeToStateSleep()
{
	currentState = &stateSleep;
	currentState->start(this, macModuleInterface);
}

void RicerStateContext::changeToStateInitiateReceive()
{
	currentState = &stateInitiateReceive;
	currentState->start(this, macModuleInterface);
}

void RicerStateContext::changeToStateSend()
{
	currentState = &stateSend;
	currentState->start(this, macModuleInterface);
}

void RicerStateContext::changeToStateWaitToSend()
{
	currentState = &stateWaitToSend;
	currentState->start(this, macModuleInterface);
}

void RicerStateContext::startup()
{
	//macModuleInterface->log("Context startup");
	currentState->start(this, macModuleInterface);
}

void RicerStateContext::timerFired(RicerMacTimer timer)
{
	currentState->timerFired(this, macModuleInterface, timer);
}

void RicerStateContext::log(string message)
{
	macModuleInterface->log("*" + getCurrentStateName() + "* " + message);
}

void RicerStateContext::resetBackoff()
{
	//macModuleInterface->log("Resetting backoff");
	binaryExponentialBackoff.resetBackoff();
}

double RicerStateContext::getNextBackoff()
{
	return binaryExponentialBackoff.getNextBackoff();
}

void RicerStateContext::fromRadioLayer(RicerMacPacket *packet)
{
	currentState->fromRadioLayer(this, macModuleInterface, packet);
}

bool RicerStateContext::bufferPacketFromNetLayer(RicerMacPacket *packet)
{
	if (m_txBuffer.size() >= macParameters.macBufferSize) 
	{
		log("WARNING - MAC buffer full");
		macModuleInterface->collectStats("Ricer buffer overflow");
		return false;
	} 
	else 
	{
		BufferedMacPacketQueueItem packetAndRetries;
		packetAndRetries.noOfSendAttempts = 0;
		packetAndRetries.packet = packet;
		m_txBuffer.push_back(packetAndRetries);

		log("Packet buffered from network layer addressed to " + std::to_string(packet->getDestination()) + ", buffer size " + std::to_string(m_txBuffer.size()));
		currentState->packetFromNetLayerHasBeenBuffered(this, macModuleInterface);
		
		return true;
	}
}

bool RicerStateContext::hasMessagesToSend()
{
	return m_txBuffer.size() > 0;
}

bool RicerStateContext::hasNextBroadcastOrUnicastWaitingToSendTo(int nodeId)
{
	BufferedMacPacketQueueItem *queueItem = getNextBroadcastOrUnicastQueueItemWaitingToSendTo(nodeId);
	if(queueItem == nullptr)
	{
		//macModuleInterface->log("No message waiting for node " + std::to_string(nodeId));
		return false; 
	}
	return true;
}

RicerMacPacket* RicerStateContext::getCopyOfNextBroadcastOrUnicastWaitingToSendTo(int nodeId)
{
	BufferedMacPacketQueueItem *queueItem = getNextBroadcastOrUnicastQueueItemWaitingToSendTo(nodeId);
	if(queueItem == nullptr)
	{
		// Should never get to here because should always check hasPacketWaitingFor before calling this function 
		throw std::runtime_error("Couldn't find packet for node requested");
	}
	else
	{
		//macModuleInterface->log("Found message waiting for node " + std::to_string(nodeId) + ", returning duplicate");
		return queueItem->packet->dup();
	}
}

RicerMacPacket* RicerStateContext::peekAtNextBroadcastOrUnicastWaitingToSendTo(int nodeId)
{
	BufferedMacPacketQueueItem *queueItem = getNextBroadcastOrUnicastQueueItemWaitingToSendTo(nodeId);
	if(queueItem == nullptr)
	{
		// Should never get to here because should always check hasPacketWaitingFor before calling this function 
		throw std::runtime_error("Couldn't find packet for node requested");
	}
	else
	{
		//macModuleInterface->log("Found message waiting for node " + std::to_string(nodeId) + ", returning duplicate");
		return queueItem->packet;
	}
}

// Note: this is a private function
BufferedMacPacketQueueItem* RicerStateContext::getNextBroadcastOrUnicastQueueItemWaitingToSendTo(int nodeId)
{
	vector<BufferedMacPacketQueueItem>::iterator it;

	for(it = m_txBuffer.begin(); it != m_txBuffer.end(); it++)
	{
		// If there is a broadcast packet in the queue
		// AND we have not already sent the broadcast to the specified node,
		if(((*it).packet->getIsDataForBroadcast() &&
			(*it).sentBroadcastToNodes.find(nodeId) == (*it).sentBroadcastToNodes.end()) ||
		// or there is a unicast packet addressed to the specified node
		   packetIsAddressedToNode((*it).packet, nodeId))
		{
			return &(*it);
		}
	}
	return nullptr;
}

RicerMacPacket* RicerStateContext::peekAtNextUnicastPacket()
{
	vector<BufferedMacPacketQueueItem>::iterator it;

	for(it = m_txBuffer.begin(); it != m_txBuffer.end(); it++)
	{
		if(!(*it).packet->getIsDataForBroadcast())
		{
			return (*it).packet;
		}
	}

	opp_error("Asked to peek at unicast packet but there are no unicast packets");
	return NULL;
}

void RicerStateContext::recordHaveSentBroadcastPacketToNode(int nodeSentTo)
{
	vector<BufferedMacPacketQueueItem>::iterator it;

	for(it = m_txBuffer.begin(); it != m_txBuffer.end(); it++)
	{
		// If there is a broadcast packet in the queue
		// AND we have not already recorded it as sent to the specified node,
		if((*it).packet->getIsDataForBroadcast() &&
			(*it).sentBroadcastToNodes.find(nodeSentTo) == (*it).sentBroadcastToNodes.end())
		{
			// Record it as sent to the specified node
			(*it).sentBroadcastToNodes.insert(nodeSentTo);
			//macModuleInterface->log("Recorded that we have sent broadcast packet to node " + std::to_string(nodeSentTo));
			return;
		}
	}

	throw std::runtime_error("Asked to record that broadcast packet has been sent to node but couldn't find the packet");
}

void RicerStateContext::unicastPacketHasBeenSentToNodeSoRemoveFromQueue(int nodeSentTo)
{
	vector<BufferedMacPacketQueueItem>::iterator it;

	for(it = m_txBuffer.begin(); it != m_txBuffer.end(); )
	{
		if(packetIsAddressedToNode((*it).packet, nodeSentTo))
		{
			RicerMacPacket *pktToRemove = (*it).packet;
			it = m_txBuffer.erase(it);
			macModuleInterface->cleanUpAndRemoveMessage(pktToRemove);
			//macModuleInterface->log("Removed unicast packet from queue");
			return;
		}
		else
		{
			++it;
		}
	}
	throw std::runtime_error("Asked to remove unicast packet from queue but couldn't ffind it");
}

void RicerStateContext::incrementSendAttemptsOnAllWaitingPackets()
{
	vector<BufferedMacPacketQueueItem>::iterator it;

	for(it = m_txBuffer.begin(); it != m_txBuffer.end(); it++) // Note: no it++ because we need to handle removing elements, see below
	{
		// Increment the number of send attempts
		// Note: noOfSendAttempts starts at zero, and we increment BEFORE sending
        (*it).noOfSendAttempts = (*it).noOfSendAttempts + 1;

		// if(isBroadcastNetPacket((*it).packet))
		// {
		// 	macModuleInterface->log("Incremented retries on broadcast packet to " + std::to_string((*it).noOfSendAttempts));
		// }
		// else
		// {
		// 	macModuleInterface->log("Incremented retries on unicast packet to " + std::to_string((*it).noOfSendAttempts));
		// }
    }
}

// Returns a list of node IDs for any nodes who we have dropped unicast packets for
vector<int> RicerStateContext::dropPacketsAboveMaxSendAttempts()
{
	vector<int> nodeIdsOfDroppedUnicastPackets;
	vector<BufferedMacPacketQueueItem>::iterator it;

	for(it = m_txBuffer.begin(); it != m_txBuffer.end(); ) // Note: no it++ because we need to handle removing elements, see below
	{
		// We only send broadcasts once. Therefore, if the packet is broadcast,
		// and the noOfSendAttempts is greater than 1, drop it
		if((*it).packet->getIsDataForBroadcast())
		{
			if((*it).noOfSendAttempts > 0)
			{
				RicerMacPacket *pktToRemove = (*it).packet;
				it = m_txBuffer.erase(it); // also increments ++it
				macModuleInterface->cleanUpAndRemoveMessage(pktToRemove);
				macModuleInterface->log("Dropped broadcast which has already been sent");
			}
			else
			{
				++it;
			}
		}
		// Drop unicast packets if the number of send attempts is over the max set in parameters
		else
		{
			if((*it).noOfSendAttempts >= getMacParameters().maxSendRetries)
			{
				RicerMacPacket *pktToRemove = (*it).packet;
				nodeIdsOfDroppedUnicastPackets.push_back((*it).packet->getDestination());
				it = m_txBuffer.erase(it); // also increments ++it
				macModuleInterface->cleanUpAndRemoveMessage(pktToRemove);
				macModuleInterface->log("Dropped unicast packet which reached max send retries of " + std::to_string(getMacParameters().maxSendRetries));
				macModuleInterface->collectStats("Ricer dropped packet");
			}
			else
			{
				++it;
			}
		}
    }

	return nodeIdsOfDroppedUnicastPackets;
}

bool RicerStateContext::packetIsAddressedToNode(RicerMacPacket *macPacket, int nodeId)
{
	return macPacket->getDestination() == nodeId;
}

double RicerStateContext::getRandomDouble()
{
	return randomNumberGenerator.getRandomDouble();
}

void RicerStateContext::setReceivedBeaconFromNodeToSendTo(int nodeId)
{
	m_nodeToSendTo = nodeId;
}

int RicerStateContext::getReceivedBeaconFromNodeToSendTo()
{
	return m_nodeToSendTo;
}

void RicerStateContext::setNeedToSendReadyToReceiveBeacon(bool needToSend)
{
	m_needToSendReadyToReceiveBeacon = needToSend;
}

bool RicerStateContext::getNeedToSendReadyToReceiveBeacon()
{
	return m_needToSendReadyToReceiveBeacon;
}

void RicerStateContext::setNeedToWakeToSendNewPacket(bool needToWake)
{
	m_needToWakeToSendNewPacket = needToWake;
}
		
bool RicerStateContext::getNeedToWakeToSendNewPacket()
{
	return m_needToWakeToSendNewPacket;
}

void RicerStateContext::setNeedToWakeForReceive(bool needToWakeForReceive)
{
	m_needToWakeForReceive = needToWakeForReceive;
}

bool RicerStateContext::getNeedToWakeForReceive()
{
	return m_needToWakeForReceive;
}