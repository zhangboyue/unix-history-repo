#include "f2c.h"

#ifdef KR_headers
extern double sin(), cos(), sinh(), cosh();

VOID c_sin(resx, z) complex *resx, *z;
#else
#undef abs
#include <math.h>

void c_sin(complex *resx, complex *z)
#endif
{
complex res;

res.r = sin(z->r) * cosh(z->i);
res.i = cos(z->r) * sinh(z->i);

resx->r = res.r;
resx->i = res.i;
}
