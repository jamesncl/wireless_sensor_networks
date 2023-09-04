
enum SenderControlCommand_type {
	SENDER_CONTROL_OKAY_TO_SEND = 1;
	SENDER_CONTROL_DO_NOT_SEND = 2;
}

message SenderControlCommand {
	int senderControlCommandKind enum (SenderControlCommand_type);
}
