
package node.communication.mac.boxMacTwo;

simple BoxMacTwoSender {

	parameters:

		// Enable / disable CastaliaModule trace collection
		bool collectTraceInfo = default(false);
		bool collectPlotTraceInfo = default(false);
		
		// Initial backoff time
		// In CC2420 / TinyOS 2.1.2 LPL, initial backoff is defined as:
		// <Random 16 bit number> % 40 + 10 (in jiffies)
		// i.e. random 10-50 jiffies
		// 1 jiffie = 32 microsec
		// So initil backoff is random 320-1600 microsecs
		double initialBackoffMin @unit(s) = default(320us);	//microseconds!
		double initialBackoffMax @unit(s) = default(1600us);	//microseconds!

		// Congestion backoff time
		// In CC2420 / TinyOS 2.1.2 LPL, initial backoff is defined as:
		// <Random 16 bit number> % 30 + 10) jiffies
		// i.e. 320 - 1280
		double congestionBackoffMin @unit(s) = default(320us);	//microseconds!
		double congestionBackoffMax @unit(s) = default(1280us);	//microseconds!

		// Sending of a message is done as a train of messages which span at least 1 LPL wakeup period,
		// plus some padding. Default TinyOS 2.1.2 uses +20ms. 
		double lplWakeIntervalSendPadding @unit(s) = default(20ms);

		// Wait delay between unicast transmissions in a transmission train during which we listen for ACKs
		double interTransmissionAckReceiveDelay @unit(s) = default(8.45ms); // Based on TinyOS 2.1.2 / CC2420 timings
		// Wait delay between broadcast transmissions in a transmission train. We don't listen for ACK. Shorter than unicast.
		double interTransmissionBroadcastDelay @unit(s) = default(2.86ms); // Based on TinyOS 2.1.2 / CC2420 timings

		// Maximum number of messages to hold in buffer
		int maxMessageBufferSize = default(10);

		// How long to wait after requesting the radio switches to RX, before we attempt to send a message
		double waitForRxTransitionDelayTime @unit(s) = default(323us); //us = microseconds

	gates:
		output toBoxMacController;
		input fromBoxMacController;
}