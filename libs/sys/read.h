#include <bits/syscall.h>
#include <syscall3.h>

inline static long sysread(int fd, char* buf, unsigned long len)
{
	return syscall3(__NR_read, fd, (long)buf, len);
}
