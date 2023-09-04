#ifndef _RICERSTATECONTEXTINTERFACE_H_
#define _RICERSTATECONTEXTINTERFACE_H_

#include <string>
#include <vector>
#include "omnetpp.h"
#include "RicerMacInterface.h"
#include "Radio.h"
#include "RadioControlMessage_m.h"
#include "RicerMacParameters.h"
#include "RicerMacTimers.h"
#include "RicerMacPacket_m.h"

class RicerStateContextInterface
{
	public:

		virtual void initialiseContext(RicerMacInterface *moduleInterface, RicerMacParameters parameters) = 0;
		virtual void clearAllState() = 0;
		virtual int howManyUnicastPacketsInBuffer() = 0;
		virtual int howManyBroadcastPacketsInBuffer() = 0;
		virtual std::string getCurrentStateName() = 0;
		virtual void startup() = 0;
		virtual void changeToStateListenForData() = 0;
		virtual void changeToStateSleep() = 0;
		virtual void changeToStateInitiateReceive() = 0;
		virtual void changeToStateWaitToSend() = 0;
		virtual void changeToStateSend() = 0;
		virtual void log(string message) = 0;
		virtual void timerFired(RicerMacTimer timer) = 0;
		virtual void resetBackoff() = 0;
		virtual double getNextBackoff() = 0;
		virtual RicerMacParameters getMacParameters() = 0;
		virtual void fromRadioLayer(RicerMacPacket *packet) = 0;
		virtual bool bufferPacketFromNetLayer(RicerMacPacket *packet) = 0;
		virtual bool hasMessagesToSend() = 0;
		virtual bool hasNextBroadcastOrUnicastWaitingToSendTo(int nodeId) = 0;
		virtual RicerMacPacket* getCopyOfNextBroadcastOrUnicastWaitingToSendTo(int nodeId) = 0;
		virtual RicerMacPacket* peekAtNextBroadcastOrUnicastWaitingToSendTo(int nodeId) = 0;
		virtual RicerMacPacket* peekAtNextUnicastPacket() = 0;
		virtual void recordHaveSentBroadcastPacketToNode(int nodeSentTo) = 0;
		virtual void unicastPacketHasBeenSentToNodeSoRemoveFromQueue(int nodeSentTo) = 0;
		virtual void incrementSendAttemptsOnAllWaitingPackets() = 0;
		virtual vector<int> dropPacketsAboveMaxSendAttempts() = 0;
		virtual double getRandomDouble() = 0;
		virtual void setReceivedBeaconFromNodeToSendTo(int nodeId) = 0;
		virtual int getReceivedBeaconFromNodeToSendTo() = 0;
		virtual void setNeedToSendReadyToReceiveBeacon(bool needToSend) = 0;
		virtual bool getNeedToSendReadyToReceiveBeacon() = 0;
		virtual void setNeedToWakeToSendNewPacket(bool needToWake) = 0;
		virtual bool getNeedToWakeToSendNewPacket() = 0;
		virtual void setNeedToWakeForReceive(bool needToWakeForReceive) = 0;
		virtual bool getNeedToWakeForReceive() = 0;
		
};

#endif //_RICERSTATECONTEXTINTERFACE_H_