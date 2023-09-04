package node.communication.routing.ctpRouting;

simple CtpRoutingTableManager {

	parameters:
		// Enable / disable CastaliaModule trace collection
		bool collectTraceInfo = default(false);
		bool collectPlotTraceInfo = default(false);
		
		// Size of node routing table
		int nodeRoutingTableMaxSize = default(10);

		// When looking to evict a node to make space in the routing table, this ETX threshold is
		// used to evict nodes which have equal or higher ETX
		double evictionEtxThreshold = default(6.5); 

		// When checking for a new better parent, the new parent must have a multihop ETX
		// *plus this parameter* lower than the existing parent
		double newParentSwitchAdditionalMhEtx = default(1.5);

		// The maximum allowed SH-ETX allowed before we consider a node unreachable
		double unreachableNodeShEtxThreshold = default(15);

	gates:
		input fromLinkEstimator;
		input fromController;
		output toBeaconSender;
		output toController;
}