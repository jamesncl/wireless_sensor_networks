package node.communication.mac.boxMacTwo;

simple BoxMacTwoController {

	parameters:

		// Enable / disable CastaliaModule trace collection
		bool collectTraceInfo = default(false);
		bool collectPlotTraceInfo = default(false);

		// ========= Parameters used by VirtualMac ============
		// Note: these are also in BoxMacTwo.ned
		// TODO: modify iMac to avoid this duplication
		int macMaxPacketSize = default (0);	// in bytes
		int macBufferSize = default (32);		// in number of messages
		//int macPacketOverhead = default (12); // Bytes

		// How long to sleep for in between wakeups
		double sleepTime @unit(s) = default(100ms);

		// How long to leave the radio on Rx listening for messages after busy CCA result
		double receivePeriodAfterCcaBusy @unit(s) = default(50ms);

		// Size of MAC frames, in bytes
		int ackFrameSizeBits @unit(b) = default(32b);  	//4 bytes = 32 bits
		int dataFrameSizeBits @unit(b) = default(96b); 	//12 bytes = 96 bits

	gates:

		// Gates to/from the parent BoxMacTwo compound module
		output toNetworkModule;
		output toRadioModule;
		input fromNetworkModule;
		input fromRadioModule;
		input fromCommModuleResourceMgr;

		// Gates to/from the CCA module
		input fromBoxMacCca;
		output toBoxMacCca;

		// Gates to/from the Sender module
		input fromBoxMacSender;
		output toBoxMacSender;
}