package node.communication.routing.staticRouting;

simple StaticRouting like node.communication.routing.iRouting

{
	parameters:
		bool collectTraceInfo = default (false);
		bool collectPlotTraceInfo = default(false);
		
		int maxNetFrameSize = default (0);	// bytes
		int netBufferSize = default (32);	// number of messages
		int networkDataFrameOverheadBits @unit(b) = default(17b); // same as CTP / MMBCR to make comparable

		// Which node THIS node is to route to
		string routeToNode; // MUST be set for each node

		int staticRoutingFrameSizeBits @unit(b) = default(31b); // To match CTP

		// Whether or not to retry message sending if not acknowledged.
		// Currently ONLY COMPATIBLE WITH BoX-MAC, RicerMac and TMac which send NETWORK_CONTROL_COMMANDs to
		// the network layer to inform if ACK or no ACK received.
		// Therefore if using any other MAC, set to false, otherwise CTP routing will
		// not work properly
		bool implementRetries = default(true);
		int maxPacketSendRetries = default(10); // The max number of times to attempt retry sending before dropping packet

	gates:
		output toCommunicationModule;
		output toMacModule;
		input fromCommunicationModule;
		input fromMacModule;
		input fromCommModuleResourceMgr;
}

