package node.communication.mac.ricerMac;

simple RicerMac like node.communication.mac.iMac 
{
 	parameters:
		bool collectTraceInfo = default (false);
		bool collectPlotTraceInfo = default(false);
		int macMaxPacketSize = default (0);		
		//int macPacketOverhead = default (8);	//only 2 fields are added in simple MAC frame, 8 bytes in total
		int macBufferSize = default (10);		

		int ricerAckRtrFrameSizeBits @unit(b) = default(32b); 	//4 bytes = 32 bits
		int ricerRtrFrameSizeBits @unit(b) = default(104b);		//13 bytes = 104 bits
		int ricerDataFrameSizeBits @unit(b) = default(96b);		//12 bytes = 96 bits

		// These times are based on those found in CC2420 radio parameters file
		// Ideally these should be read dynamically from radio module.
		// How long to wait after requesting the radio switches to RX, before we attempt to take CCA result
		// Note: this is 0.194ms for the transition, plus 0.128ms for RSSI integration time 
		double waitForRxTransitionDelayTime @unit(s) = default(0.323ms);
		// How long to wait after requesting the radio switches to SLEEP, before attempting to switch to back to RX
		double waitForSleepTransitionDelayTime @unit(s) = default(0.05ms);

		// Binary exponential backoff - used when attempting to send ready-to-receive beacon
		// See https://en.wikipedia.org/wiki/Exponential_backoff for algo details
		double binaryExponentialBackoffSlotDuration @unit(s) = default(20us);	//microseconds!
		int binaryExponentialBackoffMaxExponent = default(10);

		// Send-data backoff - used when attempting to send data in response to a ready-to-receive beacon
		// The minimum backoff should be at least the radio RSSI integration time, so that we get
		// a proper CCA reading after backing off
		double sendDataBackoffMin @unit(s) = default(130us);	//microseconds!
		double sendDataBackoffMax @unit(s) = default(5000us);	//microseconds!

		// This should come from the Radio module, but to maintain convention established by existing TMAC,
		// shorcut is taken to specify radio data rate here
		double phyDataRate = default(250);  //kbps

		// How long between wakeup events for sending ready-to-receive beacons
		// When the last ready-to-receive beacon has been sent (remember that
		// there may be a series of beacons if multiple packets are being sent
		// to this node), a wakeForReceiveInterval timer is set. When the node
		// has completed any sending it needs to do, it will go to sleep for
		// however long is left on the timer. Therefore we bound the max time
		// bewteen wakeup / sending ready-to-receive beacons, which we need to
		// know when calculating how long to listen for when waiting to send
		double wakeForReceiveInterval @unit(s) = default(100ms);
		// An amount to randomly jitter around the default wake for receive interval
		// Avoids the protocol from syncing with itself and causing excess collisions
		double wakeForReceiveIntervalJitter @unit(s) = default(25ms);

		int maxSendRetries = default(20);

		// How long to wait after receiving a CCA_NOT_VALID or CCA_NOT_VALID_YET result from radio
		// This can happen in the following edge case:
		// Have just sent a packet to the radio to be delivered to another node. Transmission starts.
		// It's a broadcast packet so we're not waiting for an ACK.
		// The send timeout timer happens to fire before the transmission ends (but the MAC is unaware of this)
		// State changes to sleep, but we need to send a beacon (the need-to-send-RTR flag is set).
		// State initiate-receive asks radio to go to RX, and waits, but the radio is actually still
		// transmitting. This means that when we ask for CCA its invalid. Therefore need to just wait a bit
		// until transmit has ended and radio goes back to RX
		double waitforRadioTxCompleteAfterInvalidCcaResult @unit(s) = default(0.004s);

		// We calculate how long to wait for data after sending an RTR, and how long to wait for an ACK
		// after sending data, by calcuating how long it will take to transmit various packet lengths.
		// However the actual time needed may vary depending on traffic / how many backoffs are required.
		// The node does not have knowledge of this. Therefore we need to wait 'a bit longer'. This parameter
		// controls how much longer:
		int waitForDataAndAckResponseMultiplier = default(2);

 	gates:
		output toNetworkModule;
		output toRadioModule;
		input fromNetworkModule;
		input fromRadioModule;
		input fromCommModuleResourceMgr;
}

