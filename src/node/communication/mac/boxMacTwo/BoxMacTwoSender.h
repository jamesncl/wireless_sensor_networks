#ifndef _BOXMACTWOSENDER_H_
#define _BOXMACTWOSENDER_H_

#include <queue>
#include <math.h>       /* ceil */
#include "CastaliaModule.h"
#include "Radio.h"
#include "TimerService.h"
#include "SenderControlMessage_m.h"
#include "BoxMacTwoPacket_m.h"
#include "BoxMacControlMessage_m.h"

enum boxMacSenderControllerDirectiveType {
	BOX_MAC_SENDER_DIRECTIVE_OKAY_TO_SEND = 1,
	BOX_MAC_SENDER_DIRECTIVE_DO_NOT_SEND = 2
};

enum boxMacSenderSendState {
	BOX_MAC_SENDER_STATE_IDLE = 1,
	BOX_MAC_SENDER_STATE_START_NEXT_TRAIN = 2,
	BOX_MAC_SENDER_STATE_START_NEXT_MSG_IN_TRAIN = 3,
	BOX_MAC_SENDER_STATE_BACKING_OFF_INITIAL = 4,
	BOX_MAC_SENDER_STATE_BACKING_OFF_CONGESTION = 5,
	BOX_MAC_SENDER_STATE_BACKING_OFF_COMPLETE = 6,
	BOX_MAC_SENDER_STATE_WAITING_FOR_RX_TRANSITION_DELAY = 7,
	BOX_MAC_SENDER_STATE_TRANSMITTING = 8
};

enum boxMacSenderTimers {
	BOX_MAC_SENDER_TIMER_LPL_WAKE_INTERVAL = 1,
	BOX_MAC_SENDER_TIMER_INTER_TRANSMISSION_DELAY = 2,
	BOX_MAC_SENDER_TIMER_BACKOFF = 3,
	BOX_MAC_SENDER_TIMER_WAIT_FOR_RADIO_RX_TRANSITION_DELAY = 4
};

class BoxMacTwoSender : public CastaliaModule, public TimerService
{
	private:
		//=========== Private NED file parameters ============
		// See NED file for comments

		double initialBackoffMin;
		double initialBackoffRange;
		double congestionBackoffMin;
		double congestionBackoffRange;
		double transmissionTimeToOverlapLplWakeInterval;
		double interTransmissionAckReceiveDelay;
		double interTransmissionBroadcastDelay;
		std::queue<BoxMacTwoPacket*>::size_type maxMessageBufferSize;
		double waitForRxTransitionDelayTime;

		//=========== Private member variables ============
		static const char *OUTPUT_SENT_UNICAST;
		static const char *OUTPUT_SENT_BROADCAST;
		static const char *OUTPUT_BACKOFF_INITIAL;
		static const char *OUTPUT_BACKOFF_CONGESTION;
		static const char *OUTPUT_MSG_NOT_ACKED;
		static const char *OUTPUT_MESSAGES_IN_UNICAST_MESSAGE_TRAIN;
		static const char *OUTPUT_UNICAST_MESSAGE_TRAIN_DURATION;

		bool hasSendingLplWakeIntervalExpired;
		int numberOfSendsDone;
		int sendState;
		int controllerDirectiveState;
		int countNumberOfMessagesSentInTrain;
		double trainStartTime;
		std::queue<BoxMacTwoPacket*> sendQueue;

		// A pointer to the Radio module object. Used to directly call isChannelClear.
		// See comment in startup function for explanation.
		Radio *radioModule;


		//=========== Private member functions ===========
		void initialisePrivateVariables();
		void startSendingNextMessageTrainInQueue();
		void advanceMessageSendState();
		void finishedSending();
		void clearSendQueue();
		void collectUnicastMessageTrainStats();

	protected:

		// Virtual methods from CastaliaModule class
		void initialize();
		void finishSpecific();
		void handleMessage(cMessage *msg);
		void timerFiredCallback(int index);
		void changeState(int newState);
};

#endif //_BOXMACTWOSENDER_H_