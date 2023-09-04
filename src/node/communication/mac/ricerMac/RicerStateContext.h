#ifndef _RICERSTATECONTEXT_H_
#define _RICERSTATECONTEXT_H_

#include <string>
#include <vector>
#include <map>
#include <unordered_set>
#include <cstring>
#include "RicerStateContextInterface.h"
#include "RicerMacInterface.h"
#include "RicerState.h"
#include "RicerStateSleep.h"
#include "RicerStateInitiateReceive.h"
#include "RicerStateListenForData.h"
#include "RicerStateWaitToSend.h"
#include "RicerStateSend.h"
#include "Radio.h"
#include "RadioControlMessage_m.h"
#include "RicerMacParameters.h"
#include "RicerMacTimers.h"
#include "BinaryExponentialBackoff.h"
#include "RandomNumberOmnetImpl.h"

struct BufferedMacPacketQueueItem
{
	BufferedMacPacketQueueItem() : noOfSendAttempts(0) {}
	RicerMacPacket* packet;
	int noOfSendAttempts;
	// For broadcast packets, this keeps a list of nodes have sent the broadcast to:
	std::unordered_set<int> sentBroadcastToNodes;
};

class RicerStateContext : RicerStateContextInterface
{
	private:
		RicerMacInterface *macModuleInterface;
		RicerMacParameters macParameters;
		RicerState *currentState;
		RicerStateSleep stateSleep;
		RicerStateInitiateReceive stateInitiateReceive;
		RicerStateListenForData stateListenForData;
		RicerStateWaitToSend stateWaitToSend;
		RicerStateSend stateSend;
		RandomNumberOmnetImpl randomNumberGenerator;
		BinaryExponentialBackoff binaryExponentialBackoff;

		vector<BufferedMacPacketQueueItem> m_txBuffer;
		double m_currentExponentialBackoffValue;
		int m_nodeToSendTo;
		bool m_needToSendReadyToReceiveBeacon;
		bool m_needToWakeToSendNewPacket;
		bool m_needToWakeForReceive;

		BufferedMacPacketQueueItem* getNextBroadcastOrUnicastQueueItemWaitingToSendTo(int nodeId);
		bool packetIsAddressedToNode(RicerMacPacket *macPacket, int nodeId);
		void initialisePrivateVariables();

	public:
		// Constructor
		RicerStateContext();

		void initialiseContext(RicerMacInterface *moduleInterface, RicerMacParameters parameters);
		void clearAllState();
		int howManyUnicastPacketsInBuffer();
		int howManyBroadcastPacketsInBuffer();
		std::string getCurrentStateName();
		void startup();
		void changeToStateListenForData();
		void changeToStateSleep();
		void changeToStateInitiateReceive();
		void changeToStateWaitToSend();
		void changeToStateSend();
		void log(string message);
		void timerFired(RicerMacTimer timer);
		void resetBackoff();
		double getNextBackoff();
		RicerMacParameters getMacParameters();
		void fromRadioLayer(RicerMacPacket *packet);
		bool bufferPacketFromNetLayer(RicerMacPacket *packet);
		bool hasMessagesToSend();
		bool hasNextBroadcastOrUnicastWaitingToSendTo(int nodeId);
		RicerMacPacket* getCopyOfNextBroadcastOrUnicastWaitingToSendTo(int nodeId);
		RicerMacPacket* peekAtNextBroadcastOrUnicastWaitingToSendTo(int nodeId);
		RicerMacPacket* peekAtNextUnicastPacket();
		void recordHaveSentBroadcastPacketToNode(int nodeSentTo);
		void unicastPacketHasBeenSentToNodeSoRemoveFromQueue(int nodeSentTo);
		void incrementSendAttemptsOnAllWaitingPackets();
		vector<int> dropPacketsAboveMaxSendAttempts();
		double getRandomDouble();
		void setReceivedBeaconFromNodeToSendTo(int nodeId);
		int getReceivedBeaconFromNodeToSendTo();
		void setNeedToSendReadyToReceiveBeacon(bool needToSend);
		bool getNeedToSendReadyToReceiveBeacon();
		void setNeedToWakeToSendNewPacket(bool needToWake);
		bool getNeedToWakeToSendNewPacket();
		void setNeedToWakeForReceive(bool needToWakeForReceive);
		bool getNeedToWakeForReceive();
};

#endif //_RICERSTATECONTEXT_H_