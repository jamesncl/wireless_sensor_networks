
enum BeaconSenderControlMessage_type {
	BEACON_SENDER_UPDATE_MULTIHOP_ETX_TO_ROOT = 1;
	BEACON_SENDER_RESET_TRICKLE = 2;
	BEACON_SENDER_RESET_TRICKLE_AND_PULL = 3;
	BEACON_SENDER_NEW_PARENT = 4;
}

message BeaconSenderControlMessage {
	int beaconSenderControlMessageKind enum (BeaconSenderControlMessage_type);
	int parentNodeId;
	double multihopEtxToRoot;
}
