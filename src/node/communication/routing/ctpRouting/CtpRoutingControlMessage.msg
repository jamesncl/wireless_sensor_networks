
enum CtpRoutingControlMessage_type {
	
	CTP_ROUTING_MSG_UPDATE_ROUTE_INFO = 4;
}

message CtpRoutingControlMessage {
	int ctpRoutingControlMessageKind enum (CtpRoutingControlMessage_type);
	int value;
	double multihopEtx; // For use by CTP_ROUTING_MSG_UPDATE_ROUTE_INFO
}
