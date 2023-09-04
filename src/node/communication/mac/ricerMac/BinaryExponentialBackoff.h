#ifndef _BINARYEXPONENTIALBACKOFF_H_
#define _BINARYEXPONENTIALBACKOFF_H_

// For pow():
#include <cmath>
// For runtime exception:
#include <stdexcept>
#include "RandomNumberInterface.h"

class BinaryExponentialBackoff
{
	private:
		RandomNumberInterface *randomNumberGenerator;
		double slotDuration;
		int maxExponent;
		int currentExponent;

	public:
		BinaryExponentialBackoff();
		void initialise(double slotDur, int maxExp, RandomNumberInterface *randomNumberGenerator);
		void resetBackoff();
		double getNextBackoff();		
};

#endif //_BINARYEXPONENTIALBACKOFF_H_