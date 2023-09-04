
enum TableManagerControlMessage_type {
	TABLE_MANAGER_UPDATE_NODE_ETX = 1;
	TABLE_MANAGER_UPDATE_REMOTE_NODE_ROUTING_TABLE_INFO = 2;
}

message TableManagerControlMessage {
	int tableManagerControlMessageKind enum (TableManagerControlMessage_type);
	int nodeId;
	double value;
	int parentNodeId;
}
