
enum MmbcrControlMessage_type 
{	
	MMBCR_MSG_UPDATE_ROUTE_INFO = 4;
}

message MmbcrControlMessage {
	int mmbcrControlMessageKind enum (MmbcrControlMessage_type);
	int value;
	double multihopEtx; // For use by MMBCR_MSG_UPDATE_ROUTE_INFO
}
