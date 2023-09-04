package node.communication.routing.ctpRouting;

simple CtpRoutingBeaconSender {

	parameters:
		// Enable / disable CastaliaModule trace collection
		bool collectTraceInfo = default(false);
		bool collectPlotTraceInfo = default(false);
		
		double trickleFrequencyCoefficientMax @unit(s) = default(500s);
		double trickleFrequencyCoefficientMin @unit(s) = default(500ms); // Note: ms!
		// Flag to effectively disable Trickle - makes the beacon sending
		// interval static, set at the trickleFrequencyCoefficientMin
		bool staticFrequency = default(false);
		int beaconFrameSizeBits @unit(b) = default(63b);

	gates:
		output toController;
		input fromController;
		input fromTableManager;
}