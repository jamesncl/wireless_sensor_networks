
cplusplus {{
#include "MacPacket_m.h"
}}

class MacPacket;

enum RicerMacPacketType {
	RICER_MAC_FRAME_TYPE_DATA = 1;
	RICER_MAC_FRAME_TYPE_ACK_RTR_BEACON = 2;
	RICER_MAC_FRAME_TYPE_RTR_BEACON = 3;
}

packet RicerMacPacket extends MacPacket {
	int frameType enum (RicerMacPacketType);
	int ackForNode;

	// In this MAC, broadcasts are handled as a series of unicasts to any node which sends a RTR beacon.
	// Therefore, when receiving a data packet, we cannot rely on the destination node ID on the packet
	// being == BROADCAST_MAC_ADDRESS to determine if it was sent to broadcast address.
	// Instead, we use this flag to set / get whether this is sent to broadcast or not
	// RTR and ACK/RTR beacons are ALWAYS broadcast so flag is not used for these packet types
	bool isDataForBroadcast;
}