#ifndef _BOXMACTWOCONTROLLER_H_
#define _BOXMACTWOCONTROLLER_H_

// Header for the virtual base Castalia MAC module 
#include "VirtualMac.h"
#include "BoxMacControlMessage_m.h"
#include "BoxMacTwoPacket_m.h"
#include "CcaControlMessage_m.h"
#include "SenderControlMessage_m.h"
#include "RoutingControlMessage_m.h"

enum boxMacState {
	BOX_MAC_STATE_STARTUP = 1,
	BOX_MAC_STATE_STARTING_RADIO_FOR_CCA_POLL = 2,
	BOX_MAC_STATE_POLLING_CCA = 3,
	BOX_MAC_STATE_LISTENING = 4,
	BOX_MAC_STATE_WAITING_FOR_SENDER = 5,
	BOX_MAC_STATE_SLEEPING = 6,
	BOX_MAC_STATE_WAITING_FOR_SENDER_FROM_SLEEP = 7,	
	BOX_MAC_STATE_TRANSMITTING = 8
};

enum boxMacControllerTimers {
	BOX_MAC_TIMER_LPL_SLEEP = 1,
	BOX_MAC_TIMER_LISTEN_PERIOD = 2
};

class BoxMacTwoController : public VirtualMac
{
	private:
		//=========== Private NED file parameters ============
		// See NED file for comments

		double sleepTime;					// in ms
		double receivePeriodAfterCcaBusy;		// in ms
		int ackFrameSizeBits;
		int dataFrameSizeBits;

		//=========== Other private variables ============
		static const char *OUTPUT_OVERHEARD;
		static const char *OUTPUT_RECEIVED_BROADCAST;
		static const char *OUTPUT_RECEIVED_DATA;
		static const char *OUTPUT_RECEIVED_ACK;
		static const char *OUTPUT_SENT_ACK;
		static const char *OUTPUT_IDLE_LISTENING;
		static const char *OUTPUT_TOTAL_SLEEP_DURATION;

		int boxMacState;
		simtime_t sleepTimerTimeLeft;
		bool isSleepTimerPaused;
		bool idleListen;
		double sleepStartedAt;

		//=========== Private member functions ===========
		void startCcaPolling();
		void signalSenderOkayToSend();
		void signalSenderNotOkayToSend();
		void changeState(int newState);
		void recordSleepDurationStats();

	protected:

		// Virtual methods from VirtualMac / CastaliaModule classes 
		void startup();
		void finishSpecific();
		void fromNetworkLayer(cPacket *netPkt, int destination);
		void fromRadioLayer(cPacket * pkt, double rssi, double lqi);
		int handleControlCommand(cMessage *msg);
		void timerFiredCallback(int index);
		void handleOutOfEnergy(cMessage *outOfEnergyMsg);
};

#endif //_BOXMACTWOCONTROLLER_H_