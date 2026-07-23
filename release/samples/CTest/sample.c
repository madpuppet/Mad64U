#include <c64.h>

void main()
{
	char x = 0;
	while (1)			// some comment
	{
		for (x=0; x<100; x++)
		{
			VIC.bgcolor0++;
		}
		VIC.bgcolor1++;
	}
}


