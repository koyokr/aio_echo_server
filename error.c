#include "error.h"
#include <stdio.h>
#include <stdlib.h>

void unix_error(char const *s)
{
	perror(s);
	exit(1);
}
