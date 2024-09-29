#include "math.h"

unsigned int log2_ceil(unsigned int input)
{
    BOOL power2 = (input != 0) && ((input & (input - 1)) == 0);
    unsigned int r = 0;
    while ( input >>= 1 ) {
        ++r;
    }

    return power2 ? r : ++r;
}

unsigned int upow(unsigned int base, unsigned int exponent)
{
	unsigned int result = 1;
	for (int i = 0; i < exponent; ++i) {
		result *= base;
	}
	return result;
}

U8 num_places(unsigned int number)
{
	int r = 1;
	while (number > 9) {
		number /= 10;
		r++;
	}
	return r;
}

U8 get_digit(unsigned int number, unsigned int index)
{
	int r = number / upow(10, index);
	return r % 10;
}
