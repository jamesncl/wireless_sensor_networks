package node.communication.routing.mmbcr;

module Mmbcr like node.communication.routing.iRouting {
	
	parameters:
	 	// Inherited from iRouting:
		bool collectTraceInfo = default (false);
		int maxNetFrameSize = default (0);					// bytes, 0 means no limit
		int netBufferSize = default (32);					// number of messages
		int networkDataFrameOverheadBits @unit(b) = default(17b); // bits

	gates:
		// Inherited from iRouting: 
		output toCommunicationModule;
		output toMacModule;
		input fromCommunicationModule;
		input fromMacModule;
		input fromCommModuleResourceMgr;

	submodules:
		Controller: node.communication.routing.mmbcr.MmbcrController;
		BeaconSender: node.communication.routing.mmbcr.MmbcrBeaconSender;
		LinkEstimator: node.communication.routing.mmbcr.MmbcrLinkEstimator;
		TableManager: node.communication.routing.mmbcr.MmbcrTableManager;

	connections:
		// Connect the controller to the container Mmbcr compound module
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