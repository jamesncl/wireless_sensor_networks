package node.communication.routing.ctpRouting;

simple CtpRoutingLinkEstimator {

	parameters:
		// Enable / disable CastaliaModule trace collection
		bool collectTraceInfo = default(false);

		// Exponential smoothing constants for link quality estimation
		double inLqSmoothingConst = default(0.9);
		double etxSmoothingConst = default(0.9);
		// Number of incoming beacons over which to calculate beacons received:broadcast ratio for incoming link quality
		int inBeaconWindowSize = default(3);
		// Number of outgoing messages over which to calculate message ACKed:noAcked ration for outgoing link quality
		int outMessageWindowSize = default(5);
		
	gates:
		input fromController;
		output toTableManager;
}