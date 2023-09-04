
enum CcaControlCommand_type {
	CCA_CONTROL_START_POLLING = 1;
}

message CcaControlCommand {
	int ccaControlCommandKind enum (CcaControlCommand_type);
}
