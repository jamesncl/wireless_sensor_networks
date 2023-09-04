package node.communication.routing.mmbcr;

simple MmbcrTableManager {

	parameters:
		// Enable / disable CastaliaModule trace collection
		bool collectTraceInfo = default(false);
		bool collectPlotTraceInfo = default(false);
		
		// Size of node routing table
		int nodeRoutingTableMaxSize = default(10);

		// When looking to evict a node to make space in the routing table, this ETX threshold is
		// used to evict nodes which have equal or higher ETX
		double evictionEtxThreshold = default(6.5); 

		// The maximum allowed SH-ETX allowed before we consider a node unreachable
		double unreachableNodeShEtxThreshold = default(15);

		// Weighting for the MH-ETX component of the candidate parent selection calculation
		double weightingMhEtx = default(1);
		// Weighting for the MMBCR component of the candidate parent selection calculation
		double weightingMmbcr = default(1);
		// When checking for a new better parent, the new parent must have combined weighting
		// which is better than the current parent by at least this amount. This avoids
		// excessive parent switching (which leads to excessive pull requests)
		double newParentSwitchThreshold = default(0.1);

	gates:
		input fromLinkEstimator;
		input fromController;
		output toBeaconSender;
		output toController;
}