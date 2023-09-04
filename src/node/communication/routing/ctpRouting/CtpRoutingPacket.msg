cplusplus {{
#include "RoutingPacket_m.h"
}}

class RoutingPacket;

enum ctpRoutingPacket_type {
	CTP_ROUTING_PACKET_TYPE_BEACON = 1;
	CTP_ROUTING_PACKET_TYPE_DATA = 2;
} 

packet CtpRoutingPacket extends RoutingPacket {
	int routingPacketKind enum (ctpRoutingPacket_type);
	unsigned int hopCount;

	// For beacons, this field is used to broadcast advertise the multihop ETX of the sender to all listening nodes,
	// for route table maintenance
	// This field is also used for data packets to denote multihop ETX of sender - in this case, it is used to
	// detect routing loops (if the sender MH-ETX is equal or lower than the receiver, there must be a loop) 
	double multihopEtxToRoot;

	// This is used in beacons - if set to true, the sender is requesting updated routing info
	// Receiving nodes should respond by resetting their trickle beacon sending interval 
	bool pullFlag = false;

	// This is used in beacons - used to make sure a node A does not select as a parent a node B,
	// which itself has node A as parent (i.e. two nodes making each other their parent) 
	int parentNodeId;
	
	// Origin is the node ID of the node which FIRST transmitted the packet 
	// (may be different to source address, which refers only to the current hop)
	// Note: using string data type to align with Castalia's use of strings for network level addresses
	string origin;
}
