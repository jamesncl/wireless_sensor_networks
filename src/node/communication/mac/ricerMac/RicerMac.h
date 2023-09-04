#ifndef _RICERMAC_H_
#define _RICERMAC_H_

#include <omnetpp.h>
#include <string>
#include "VirtualMac.h"
#include "RicerStateContext.h"
#include "Radio.h"
#include "RicerMacParameters.h"
#include "RicerMacPacket_m.h"
#include "CastaliaMessages.h"
#include "RicerMacTimers.h"
#include "RoutingControlMessage_m.h"

class RicerMac : public VirtualMac, public RicerMacInterface
{
	private:
		RicerStateContext macContext;
		RicerMacParameters macParameters;

		// Map to hold paused timers. The key (int) is the timer ID (from the RicerMacTimer enum)
		// the value is the remaining time left on the paused timer.
		// If a timer does not exist in the map, it is not paused.
		std::map<int, double> pausedTimers;

	protected:
		// Methods we are overriding from VirtualMac
		void startup();
		void fromRadioLayer(cPacket *pkt, double rssi, double lqi);
		void fromNetworkLayer(cPacket *netPkt, int destination);
		void handleOutOfEnergy(cMessage *msg);
		int handleControlCommand(cMessage *msg);
		int handleRadioControlMessage(cMessage *msg);
		void timerFiredCallback(int index);
		void finishSpecific();

		// RicerMacInterface
		void log(std::string message);
		void logPlotTrace(std::string message);
		void collectStats(const char *outputName);
		void collectStats(const char *outputName, const char *outputLabel);
		void collectStats(const char *outputName, const char *outputLabel, double value);
		double getCurrentSimulationTime();
		CCA_result getCcaResultFromRadio();
		void startTimer(RicerMacTimer timer, double timerDuration);
		void stopTimer(RicerMacTimer timer);
		bool isTimerRunning(RicerMacTimer timer);
		void pauseTimer(RicerMacTimer timer);
		void resumeTimer(RicerMacTimer timer);
		bool isTimerPaused(RicerMacTimer timer);
		void removePausedTimer(RicerMacTimer timer);
		double getTimerTimeLeft(RicerMacTimer timer);
		void setRadioState(BasicState_type radioState);
		void decapsulateAndPassToNetLayer(RicerMacPacket *packet);
		void sendAckReadyToReceiveTo(int nodeIdToAck);
		void sendReadyToReceiveBeacon();
		void sendData(RicerMacPacket* packet);
		void cleanUpAndRemoveMessage(cPacket *packet);
		void reportSendingFailedToNode(int nodeIdSendFailedTo);
		void reportSendingSucceededToNode(int nodeIdSentTo);

	public:
		
		static const char *OUTPUT_RICER_CCA_CLEAR_FOR_DATA;
		static const char *OUTPUT_RICER_CCA_BUSY_FOR_DATA;
};

#endif
