#ifndef _BOXMACTWOCCA_H_
#define _BOXMACTWOCCA_H_

#include <string>
#include "Radio.h"
#include "CcaControlMessage_m.h"
#include "BoxMacControlMessage_m.h"
#include "CastaliaModule.h"
#include "TimerService.h"
#include "CastaliaMessages.h"

enum boxMacCcaTimers {
	BOX_MAC_CCA_TIMER_POLL_DELAY = 1,
	BOX_MAC_CCA_TIMER_WAIT_FOR_RADIO_RX_TRANSITION_DELAY = 2
};

enum boxMacCcaState {
	BOX_MAC_CCA_STATE_IDLE = 1,
	BOX_MAC_CCA_STATE_POLLING = 2
};

class BoxMacTwoCca : public CastaliaModule, public TimerService
{
	private:
		//=========== Private NED file parameters ============
		// See NED file for comments

		int maxCcaChecks;
		double timeForOneCcaCheck;
		int minRequiredBusyCcaResults;
		double waitForRxTransitionDelayTime;
		double pollingCcaPower;

		//=========== Other private member variables ============
		int boxMacCcaState;
		int numberOfCcaPollsMade;
		int noOfBusyCcaResults;
		static const char *OUTPUT_CCA_BUSY;
		static const char *OUTPUT_CCA_CLEAR;


		// A pointer to the Radio module object. Used to directly call isChannelClear.
		// See comment in startup function for explanation.
		Radio *radioModule;

		//=========== Private member functions ===============
		void initialiseCcaPolling();
		void requestCca();

	protected:

		// Virtual methods from CastaliaModule class
		void initialize();
		void finishSpecific();
		void handleMessage(cMessage *msg);
		void timerFiredCallback(int index);
};

#endif //_BOXMACTWOCCA_H_