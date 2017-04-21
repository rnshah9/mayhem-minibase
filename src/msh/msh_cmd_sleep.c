#include <sys/nanosleep.h>

#include <format.h>
#include <null.h>

#include "msh.h"
#include "msh_cmd.h"

#define NANOFRAC 1000000000 /* nanoseconds in a second */

static int parsetime(struct sh* ctx, struct timespec* sp, char* str)
{
	unsigned long sec = 0;
	unsigned long nsec = 0;
	char *p, *q;

	if(!*str)
		goto inval;
	if(!(p = parseulong(str, &sec)))
		goto inval;
	if(!*p)
		goto out;
	if(*p++ != '.')
		goto inval;
	if(!(q = parseulong(p, &nsec)) || *q)
		goto inval;

	unsigned long nmul = NANOFRAC;
	for(; p < q; p++) nmul /= 10;
out:
	sp->tv_sec = sec;
	sp->tv_nsec = nsec*nmul;

	return 0;
inval:
	return error(ctx, "invalid time spec", str, 0);
}

int cmd_sleep(struct sh* ctx)
{
	struct timespec sp;
	int ret;

	if(noneleft(ctx))
		return -1;
	if((ret = parsetime(ctx, &sp, shift(ctx))))
		return ret;
	if(moreleft(ctx))
		return -1;
	if((ret = sysnanosleep(&sp, NULL)))
		return error(ctx, "sleep", NULL, ret);

	return 0;
}
