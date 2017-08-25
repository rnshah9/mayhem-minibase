#include <sys/file.h>
#include <sys/fpath.h>

#include <string.h>
#include <util.h>

#include "msh.h"
#include "msh_cmd.h"

int cmd_cd(struct sh* ctx)
{
	char* dir;

	if(shift_str(ctx, &dir))
		return -1;
	if(moreleft(ctx))
		return -1;

	return fchk(sys_chdir(dir), ctx, dir);
}

int cmd_exec(struct sh* ctx)
{
	if(noneleft(ctx))
		return -1;

	char** argv = argsleft(ctx);

	return fchk(execvpe(*argv, argv, ctx->envp), ctx, *argv);
}

int cmd_exit(struct sh* ctx)
{
	int code = 0;

	if(!numleft(ctx))
		;
	else if(shift_int(ctx, &code))
		return -1;

	_exit(code);
}

int cmd_invoke(struct sh* ctx)
{
	int nargs = numleft(ctx);
	int norig = ctx->topargc - ctx->topargp;

	if(!nargs)
		return error(ctx, "too few arguments", NULL, 0);

	char** args = ctx->argv + ctx->argp;
	char** orig = ctx->topargv + ctx->topargp;

	char* argv[nargs + norig + 1];

	memcpy(argv, args, nargs*sizeof(char*));
	memcpy(argv + nargs, orig, norig*sizeof(char*));
	argv[nargs+norig] = NULL;

	return fchk(execvpe(*argv, argv, ctx->envp), ctx, *argv);
}

static int print(struct sh* ctx, int fd)
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

int cmd_echo(struct sh* ctx)
{
	return print(ctx, STDOUT);
}

int cmd_warn(struct sh* ctx)
{
	return print(ctx, STDERR);
}

int cmd_die(struct sh* ctx)
{
	cmd_warn(ctx);
	_exit(0xFF);
}
