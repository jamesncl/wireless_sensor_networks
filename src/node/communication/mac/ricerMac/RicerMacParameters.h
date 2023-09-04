#ifndef _RICERMACPARAMETERS_H_
#define _RICERMACPARAMETERS_H_

struct RicerMacParameters {

	double waitForRxTransitionDelayTime;
	double waitForSleepTransitionDelayTime;
	double binaryExponentialBackoffSlotDuration;
	double binaryExponentialBackoffMaxExponent;
	double sendDataBackoffMin;
	double sendDataBackoffMax;
	double phyDataRate;
	int selfNodeId;
	int macBufferSize;
	double wakeForReceiveInterval;
	double wakeForReceiveIntervalJitter;
	int maxSendRetries;
	double waitforRadioTxCompleteAfterInvalidCcaResult;
	int ricerAckRtrFrameSizeBits;
	int ricerRtrFrameSizeBits;
	int ricerDataFrameSizeBits;
	int phyFrameOverheadBytes;
	int networkDataFrameOverheadBits;
	int applicationPacketOverheadBytes;
	int waitForDataAndAckResponseMultiplier;

	double listenForDataTotalDwellTime()
	{
		// Max possible time it takes to receive a data packet back from a node after sending a ready-to-receive beacon is:
		return sendDataBackoffMax + // The total max time it may have backed off
			(((totalDataFrameLengthBits() / 8)  // Plus add up the total bytes for data packet 
			 +(totalRicerBeaconFrameLengthBits() / 8))  //and beacon
										// and add on how long it takes in seconds to transmit these bytes
			/ (1000*phyDataRate/8.0))	// PhyDataRate is in kilobits per second (hence 1000* and divide by 8 to get bytes)
			* waitForDataAndAckResponseMultiplier;						// Plus extra for turnaround time, radio state transitions etc.
	}

	double waitForAckTime()
	{
		// Time it takes to receive an ACK in response to a data packet is:
		return sendDataBackoffMax + // The total max time it may have backed off
			(((totalDataFrameLengthBits() / 8)  // Plus add up the total bytes for data packet 
			 +(totalRicerAckFrameLengthBits() / 8))  //and beacon
										// and add on how long it takes in seconds to transmit these bytes
			/ (1000*phyDataRate/8.0))	// PhyDataRate is in kilobits per second (hence 1000* and divide by 8 to get bytes)
			* waitForDataAndAckResponseMultiplier;						// Plus extra for turnaround time, radio state transitions etc.
	}

	int totalDataFrameLengthBits()
	{
		return 
			(phyFrameOverheadBytes * 8) // Physical layer
			+ ricerDataFrameSizeBits // MAC layer
			+ networkDataFrameOverheadBits // ROuting layer
			+ (applicationPacketOverheadBytes * 8); // Application layer
	}

	int totalRicerBeaconFrameLengthBits()
	{
		return 
			(phyFrameOverheadBytes * 8) // Physical layer
			+ ricerRtrFrameSizeBits; // MAC layer
	}

	int totalRicerAckFrameLengthBits()
	{
		return 
			(phyFrameOverheadBytes * 8) // Physical layer
			+ ricerAckRtrFrameSizeBits; // MAC layer
	}
};

#endif //_RICERMACPARAMETERS_H_