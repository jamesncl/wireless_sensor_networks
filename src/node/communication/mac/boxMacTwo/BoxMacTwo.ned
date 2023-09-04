package node.communication.mac.boxMacTwo;

module BoxMacTwo like node.communication.mac.iMac {

	parameters:
		// Enable / disable CastaliaModule trace collection
		bool collectTraceInfo = default(false);

		// ========= Parameters inherited from iMac NED ============
		//bool collectTraceInfo
		int macMaxPacketSize = default (0);	// in bytes
		int macBufferSize = default (32);		// in number of messages
		//int macPacketOverhead = default (9);

	gates:
	
		// ========== Gates we inherit from iMac ============
		output toNetworkModule;
		output toRadioModule;
		input fromNetworkModule;
		input fromRadioModule;
		input fromCommModuleResourceMgr;

	submodules:
		Controller: node.communication.mac.boxMacTwo.BoxMacTwoController;
		Cca: node.communication.mac.boxMacTwo.BoxMacTwoCca;
		Sender: node.communication.mac.boxMacTwo.BoxMacTwoSender;

	connections:
		// Connect the controller to the parent module's gates
		toNetworkModule <-- Controller.toNetworkModule;
		toRadioModule <-- Controller.toRadioModule;
		fromNetworkModule --> Controller.fromNetworkModule;
		fromRadioModule --> Controller.fromRadioModule;
		fromCommModuleResourceMgr --> Controller.fromCommModuleResourceMgr;

		// Connect the controller and Cca submodule
		Controller.fromBoxMacCca <-- Cca.toBoxMacController;
		Controller.toBoxMacCca --> Cca.fromBoxMacController;

		// Connect the controller and Sender submodule
		Controller.fromBoxMacSender <-- Sender.toBoxMacController;
		Controller.toBoxMacSender --> Sender.fromBoxMacController;
}