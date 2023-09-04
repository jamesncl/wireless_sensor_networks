package node.communication.mac.boxMacTwo;

simple BoxMacTwoCca {

	parameters:
		// Enable / disable CastaliaModule trace collection
		bool collectTraceInfo = default(false);
		bool collectPlotTraceInfo = default(false);
		
		// How often to poll the radio CCA
		int maxCcaChecks = default(6);

		// CCA polling timing
		// To ensure accuracy according to empirical data, using timings from real traces
		// Note: for CC2420 / TinyOS 2.1.2 LPL, 1600 checks takes ~=11.4ms, therefore 1 check ~= 0.007125ms
		double timeForOneCcaCheck @unit(s) = default(7.125us);	//us = microseconds!

		// How many CCA busy CCA checks required to trigger busy channel
		int minRequiredBusyCcaResults = default(3);

		// How long to wait after requesting the radio switches to RX, before we attempt to take CCA result
		double waitForRxTransitionDelayTime @unit(s) = default(323us); //us = microseconds

		// How much additional power polling CCA consumes (on top of the normal radio RX power consumption)
		double pollingCcaPower = default(0);	//in mW - empirical data indicates 5.3

	gates:
		output toBoxMacController;
		input fromBoxMacController;
}

