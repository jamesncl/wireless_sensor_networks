cplusplus {{
	#include <vector>
	typedef std::vector<double> BatteryCapacityVector_t;
}}

class noncobject BatteryCapacityVector_t;

enum MmbcrBeaconSenderControlMessage_type {
	MMBCR_BEACON_SENDER_UPDATE_MULTIHOP_ETX_TO_ROOT = 1;
	MMBCR_BEACON_SENDER_RESET_TRICKLE = 2;
	MMBCR_BEACON_SENDER_RESET_TRICKLE_AND_PULL = 3;
	MMBCR_BEACON_SENDER_NEW_PARENT = 4;
	MMBCR_BEACON_SENDER_NEW_BATTERY_CAPACITIES_TO_SINK = 5;
}

message MmbcrBeaconSenderControlMessage {
	int beaconSenderControlMessageKind enum (MmbcrBeaconSenderControlMessage_type);
	int parentNodeId;
	BatteryCapacityVector_t batteryCapacitiesOfPathToSink;
	double multihopEtxToRoot;
}
