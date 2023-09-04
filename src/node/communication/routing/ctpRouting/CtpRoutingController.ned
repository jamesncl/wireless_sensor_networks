package node.communication.routing.ctpRouting;

simple CtpRoutingController {

	parameters:
		// Inherited from iRouting
		// Note: also in CtpRouting / iRouting. TODO: modify iRouting / VirtualRouting to avoid this duplication
		bool collectTraceInfo = default (false);
		bool collectPlotTraceInfo = default(false);
		int maxNetFrameSize = default (0);
		int netBufferSize = default (12);	// Routing buffer size used in TinyOS 2.1.2 CTP is 12
		//int netDataFrameOverhead = default (8); // In bytes.

		

		// How long to not send messages for while a loop is repaired
		double repairLoopWaitTimeMin @unit(s) = default(28ms);	// in ms!
		double repairLoopWaitTimeMax @unit(s) = default(56ms);	// in ms!

		// Whether or not to retry message sending if not acknowledged.
		// Currently ONLY COMPATIBLE WITH BoX-MAC, RicerMac and TMac which send NETWORK_CONTROL_COMMANDs to
		// the network layer to inform if ACK or no ACK received.
		// Therefore if using any other MAC, set to false, otherwise CTP routing will
		// not work properly
		bool implementRetries = default(true);
		int maxPacketSendRetries = default(20); // The max number of times to attempt retry sending before dropping packet

		// An optional delay before initiating route discovery and propagation
		// This may be useful e.g. for TMAC, to allow the MAC layer to sync
		// before bombarding it with net packets
		double delayBeforeRouteDiscoveryPropagation @unit(s) = default(0ms);  // in ms. 0 means no delay

	gates: 
		// These gates connect the controller module to the container CtpRouting compound module 
		output toCommunicationModule;
		output toMacModule;
		input fromCommunicationModule;
		input fromMacModule;
		input fromCommModuleResourceMgr;

		// These gates connect controller to beacon sender
		input fromBeaconSender;
		output toBeaconSender;

		// These gates connect controller to link estimator
		output toLinkEstimator;

		// These gates connect controller to table manager
		output toTableManager;
		input fromTableManager;
}
