#include "RandomNumberOmnetImpl.h"

int RandomNumberOmnetImpl::getRandomInt(int n)
{
	return intrand(n);
}

double RandomNumberOmnetImpl::getRandomDouble()
{
	return dblrand();
}