#ifndef _RANDOMNUMBEROMNETIMPL_H_
#define _RANDOMNUMBEROMNETIMPL_H_

// For intrand(): 
#include <omnetpp.h>
#include "RandomNumberInterface.h"

class RandomNumberOmnetImpl : public RandomNumberInterface
{
	public:
		// To return random integers in the range [0, n − 1]
		int getRandomInt(int n);
		// To return random double in range 0-1
		double getRandomDouble();
};

#endif //_RANDOMNUMBEROMNETIMPL_H_