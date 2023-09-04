
enum MmbcrTableManagerControlMessage_type {
	MMBCR_TABLE_MANAGER_UPDATE_NODE_ETX = 1;
	MMBCR_TABLE_MANAGER_UPDATE_REMOTE_NODE_ROUTING_TABLE_INFO = 2;
}

message MmbcrTableManagerControlMessage {
	int tableManagerControlMessageKind enum (MmbcrTableManagerControlMessage_type);
	int nodeId;
	double value;
	int parentNodeId;
}
