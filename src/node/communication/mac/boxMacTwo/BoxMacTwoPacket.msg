
cplusplus {{
#include "MacPacket_m.h"
}}

class MacPacket;

// Note that BOX_MAC_FRAME_TYPE_DATA refers to anything other than an ACK - it could be application data, 
// but also for example routing / beacon packets from the network layer  
enum BoxMacFrameType {
	BOX_MAC_FRAME_TYPE_DATA = 1;
	BOX_MAC_FRAME_TYPE_ACK = 2;
}

packet BoxMacTwoPacket extends MacPacket {
	int frameType enum (BoxMacFrameType);
}

