#ifndef _MMBCRLINKESTIMATOR_H_
#define _MMBCRLINKESTIMATOR_H_

#include <map>
#include <numeric>
#include "CastaliaModule.h"
#include "TimerService.h"
#include "ResourceManager.h"
#include "MmbcrPacket_m.h"
#include "RoutingControlMessage_m.h"
#include "MmbcrTableManagerControlMessage_m.h"


class MmbcrLinkEstimator : public CastaliaModule, public TimerService
{
	private:
		// NED file parameters:
		double inLqSmoothingConst;
		double etxSmoothingConst;
		unsigned int inBeaconWindowSize;
		unsigned int outMessageWindowSize;

		// Other private variables:
		// Output names:
		static const char *OUTPUT_LINK_QUALITY;

		// For storing the last beacon seqNo we receive from each node - used to detect and drop duplciates
		std::map<int, int> lastSeqNoReceivedFromNode;

		// For calculating incoming link quality based on beacons received:
		std::map<int, vector<int> > inWindowOfBeaconSeqNos;
		std::map<int, double> previousInLqs;
		std::map<int, double> previousEtxs;
		// For calculating outgoing link quality based on messages sent / not ACKed
		std::map<int, vector<int> > outWindowOfMessagesSent;
		int noUnsuccessfulDeliveriesSinceLastSuccessful; // Used to calculate outgoin LQ if we get zero ACKs in a window

		// Private member functions:
		void updateIncomingLinkQuality(int nodeId, unsigned int seqNo);
		void updateOutgoingLinkQuality(int nodeId, bool wasAcked);
		void updateEtx(int nodeId, double newEtx);

	protected:
		
		// Virtual methods from CastaliaModule / TimerService class
		void initialize();
		void finishSpecific();
		void handleMessage(cMessage *msg);
		void timerFiredCallback(int index);
};

#endif //_MMBCRLINKESTIMATOR_H_