package node.communication.routing.ctpRouting;

module CtpRouting like node.communication.routing.iRouting {
	
	parameters:
	 	// Inherited from iRouting:
		bool collectTraceInfo = default (false);
		int maxNetFrameSize = default (0);					// bytes, 0 means no limit
		int netBufferSize = default (32);					// number of messages
		int networkDataFrameOverheadBits @unit(b) = default(47b);

	gates:
		// Inherited from iRouting: 
		output toCommunicationModule;
		output toMacModule;
		input fromCommunicationModule;
		input fromMacModule;
		input fromCommModuleResourceMgr;

	submodules:
		Controller: node.communication.routing.ctpRouting.CtpRoutingController;
		BeaconSender: node.communication.routing.ctpRouting.CtpRoutingBeaconSender;
		LinkEstimator: node.communication.routing.ctpRouting.CtpRoutingLinkEstimator;
		TableManager: node.communication.routing.ctpRouting.CtpRoutingTableManager;

	connections:
		// Connect the controller to the container CtpRouting compound module
		toCommunicationModule <-- Controller.toCommunicationModule;
		toMacModule <-- Controller.toMacModule;
		fromCommunicationModule --> Controller.fromCommunicationModule;
		fromMacModule --> Controller.fromMacModule;
		fromCommModuleResourceMgr --> Controller.fromCommModuleResourceMgr;

		// Connect the beacon sender to the controller
		Controller.toBeaconSender --> BeaconSender.fromController;
		Controller.fromBeaconSender <-- BeaconSender.toController;

		// Connect the link estimator to the controller
		Controller.toLinkEstimator --> LinkEstimator.fromController;

		// Connect the link estimator to the table manager
		LinkEstimator.toTableManager --> TableManager.fromLinkEstimator;

		// Connect the controller to the table manager
		Controller.toTableManager --> TableManager.fromController;
		TableManager.toController --> Controller.fromTableManager;

		// Connect the table manager to the beacon sender
		TableManager.toBeaconSender --> BeaconSender.fromTableManager;
}