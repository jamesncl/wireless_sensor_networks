#ifndef _RICERMACINTERFACE_H_
#define _RICERMACINTERFACE_H_

#include <string>
#include "RicerMacTimers.h"
#include "Radio.h"
#include "RadioControlMessage_m.h"
#include "RicerMacPacket_m.h"

class RicerMacInterface
{
	public:
		virtual void log(std::string message) = 0;
		virtual void logPlotTrace(std::string message) = 0;
		virtual void collectStats(const char *outputName) = 0;
		virtual void collectStats(const char *outputName, const char *outputLabel) = 0;
		virtual void collectStats(const char *outputName, const char *outputLabel, double value) = 0;
		virtual double getCurrentSimulationTime() = 0;
		virtual CCA_result getCcaResultFromRadio() = 0;
		virtual void startTimer(RicerMacTimer timer, double timerDuration) = 0;
		virtual void stopTimer(RicerMacTimer timer) = 0;
		virtual bool isTimerRunning(RicerMacTimer timer) = 0;
		virtual void pauseTimer(RicerMacTimer timer) = 0;
		virtual void resumeTimer(RicerMacTimer timer) = 0;
		virtual bool isTimerPaused(RicerMacTimer timer) = 0;
		virtual void removePausedTimer(RicerMacTimer timer) = 0;
		virtual double getTimerTimeLeft(RicerMacTimer timer) = 0;
		virtual void setRadioState(BasicState_type radioState) = 0;
		virtual void decapsulateAndPassToNetLayer(RicerMacPacket *packet) = 0;
		virtual void sendAckReadyToReceiveTo(int nodeIdToAck) = 0;
		virtual void sendReadyToReceiveBeacon() = 0;
		virtual void sendData(RicerMacPacket* packet) = 0;
		virtual void cleanUpAndRemoveMessage(cPacket *packet) = 0;
		virtual void reportSendingFailedToNode(int nodeIdSendFailedTo) = 0;
		virtual void reportSendingSucceededToNode(int nodeIdSentTo) = 0;
};

#endif //_RICERMACINTERFACE_H_