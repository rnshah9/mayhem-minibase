#include <sys/file.h>
#include <sys/fpath.h>

#include <string.h>
#include <util.h>

#include "msh.h"
#include "msh_cmd.h"

int cmd_cd(CTX)
{
	char* dir;

	if(shift_str(ctx, &dir))
		return -1;
	if(moreleft(ctx))
		return -1;

	return fchk(sys_chdir(dir), ctx, dir);
}

int cmd_exit(CTX)
{
	int code = 0;

	if(!numleft(ctx))
		;
	else if(shift_int(ctx, &code))
		return -1;

	exit(ctx, code);
}

static int print(CTX, int fd)
{
	char* msg;

	if(shift_str(ctx, &msg))
		return -1;
	if(moreleft(ctx))
		return -1;

	int len = strlen(msg);
	msg[len] = '\n';
	sys_write(fd, msg, len+1);
	msg[len] = '\0';

	return 0;
}

int cmd_echo(CTX)
{
	return print(ctx, STDOUT);
}

int cmd_warn(CTX)
{
	return print(ctx, STDERR);
}

int cmd_die(CTX)
{
	cmd_warn(ctx);
	exit(ctx, 0xFF);
}

/* This assigns executable to be exec()ed into in case of error.

   Current implementation is extremely crude, but so far there is
   exactly one use for this, invoking /sbin/system/reboot in pid 1
   scripts, so anything more that this would be an overkill. */

int cmd_onexit(CTX)
{
	char* arg;

	if(shift_str(ctx, &arg))
		return -1;
	if(moreleft(ctx))
		return -1;

	uint len = strlen(arg);

	if(len == 1 && arg[0] == '-') {
		memzero(ctx->trap, sizeof(ctx->trap));
		return 0;
	} else if(len + 1 > sizeof(ctx->trap)) {
		return error(ctx, "command too long", NULL, 0);
	}

	memcpy(ctx->trap, arg, len);

	return 0;
}
