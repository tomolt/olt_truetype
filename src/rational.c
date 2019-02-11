/*

Copyright 2019 Thomas Oltmann

---- LICENSE ----

Permission is hereby granted, free of charge, to any person obtaining a copy of this
software and associated documentation files (the "Software"), to deal in the Software
without restriction, including without limitation the rights to use, copy, modify,
merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be included in all copies
or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#include "rational.h"

#define sign(x) ((x) >= 0 ? 1 : -1)

static unsigned int greatest_common_divisor(Rational rat)
{
	unsigned int a = abs(rat.numer), b = rat.denom;
	do {
		if (a > b) a -= b;
		else       b -= a;
	} while (b != 0);
	return a;
}

static Rational reduce(Rational rat)
{
	int ratSign = sign(rat.numer);
	unsigned int div = greatest_common_divisor(rat);
	return (Rational) { rat.numer * ratSign / div, rat.denom / div };
}

Rational olt_INTERN_add_rational(Rational a, Rational b)
{
	int numer1 = a.numer * b.denom;
	int numer2 = b.numer * a.denom;
	unsigned int denom = a.denom * b.denom;
	return reduce((Rational) { numer1 + numer2, denom });
}

Rational olt_INTERN_mul_rational(Rational a, Rational b)
{
	// TODO think about integer overflow
	int numer = a.numer * b.numer;
	unsigned int denom = a.denom * b.denom;
	return reduce((Rational) { numer, denom });
}