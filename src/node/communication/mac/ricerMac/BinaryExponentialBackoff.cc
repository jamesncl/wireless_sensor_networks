#include "BinaryExponentialBackoff.h"

BinaryExponentialBackoff::BinaryExponentialBackoff()
{
	currentExponent = 1;
	slotDuration = 0;
	maxExponent = 0;
}

void BinaryExponentialBackoff::initialise(double slotDur, int maxExp, RandomNumberInterface *rng) 
{
	randomNumberGenerator = rng;
	slotDuration = slotDur;
	maxExponent = maxExp;
	resetBackoff();
}

void BinaryExponentialBackoff::resetBackoff()
{
	currentExponent = 1;
}

double BinaryExponentialBackoff::getNextBackoff()
{
	if(slotDuration == 0)
	{
		throw std::runtime_error("Binary exponential backoff slot duration is zero. Has initialise been called?");
	}

	// The binary exponential backoff algorithm is:
	// After c collisions (the exponent), a random number of slot times between 0 and 2^c − 1 is chosen

	// The intrand(n) function generates random integers in the range [0, n − 1]
	int randomTimeSlot = randomNumberGenerator->getRandomInt(pow(2, currentExponent));
	double backoff = randomTimeSlot * slotDuration;

	// std::cout << "Exponent is " << std::to_string(currentExponent) << 
	// 	" intrang gen from 0 to " << std::to_string(pow(2, currentExponent)) << " -1 " <<
	// 	", randomTimeSlot is " << 
	// 	std::to_string(randomTimeSlot) << 
	// 	" slot duration is " << 
	// 	std::to_string(slotDuration) << 
	// 	" backoff is " << std::to_string(backoff) << "\n";

	// Only increment the exponent up to the max allowed.
	// i.e. the backoff is truncated once max is reached. 
	if(currentExponent < maxExponent)
	{
		currentExponent++;
	}
	
	return backoff;
}