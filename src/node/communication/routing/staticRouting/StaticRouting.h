#ifndef _STATICROUTING_H_
#define _STATICROUTING_H_

#include "VirtualRouting.h"
#include "StaticRoutingPacket_m.h"
#include "RoutingControlMessage_m.h"

using namespace std;

class StaticRouting: public VirtualRouting 
{
	private:
		string routeToNode;
		bool isSending;
		bool implementRetries;
		int currentPacketSendingAttempts;
		int maxPacketSendRetries;
		int staticRoutingFrameSizeBits;

		void sendPackets();

 	protected:
	 	void startup();
		void fromApplicationLayer(cPacket *, const char *);
		void fromMacLayer(cPacket *, int, double, double);
		void handleNetworkControlCommand(cMessage *msg);
		void handleOutOfEnergy(cMessage *outOfEnergyMsg);
};

#endif				//STATICROUTINGMODULE
