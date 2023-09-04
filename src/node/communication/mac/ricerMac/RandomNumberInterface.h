#ifndef _RANDOMNUMBERINTERFACE_H_
#define _RANDOMNUMBERINTERFACE_H_

class RandomNumberInterface
{
	public:
		// To return random integers in the range [0, n − 1]
		virtual int getRandomInt(int n) = 0;
		// To return random double in range 0-1
		virtual double getRandomDouble() = 0;
};

#endif //_RANDOMNUMBERINTERFACE_H_