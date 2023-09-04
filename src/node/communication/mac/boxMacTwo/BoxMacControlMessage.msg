
enum BoxMacControlMessage_type {
	CCA_CHECK_CHANNEL_IS_BUSY = 1;
	CCA_CHECK_CHANNEL_IS_CLEAR = 2;
	SENDER_IS_SENDING = 3;
	SENDER_FINISHED_SENDING = 4;
	SENDING_FAILED_NO_ACK = 5;
}

message BoxMacControlMessage {
	int macControlCommandKind enum (BoxMacControlMessage_type);
	int value;
}
