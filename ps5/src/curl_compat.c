/* Compatibility for the static PacBrew curl/OpenSSL ports on the PS5 libc.
 * The platform layer supplies pipe(), fcntl(), and interface enumeration;
 * the unused datagram helpers fail explicitly instead of importing null APIs.
 */
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <stdarg.h>
#include <setjmp.h>
#include <unistd.h>

#include <ps5platform/libc.h>

/* libkernel exports this alias alongside fcntl; it avoids resolving back to
 * this compatibility wrapper. */
extern int _fcntl(int descriptor, int command, ...);

/* The PS5 libc rejects F_SETFD on sockets, which curl uses only to mark
 * descriptors close-on-exec. A game title never execs another process, so
 * that flag has no effect here. Forward all other commands to the kernel. */
int ps5_curl_fcntl(int descriptor, int command, ...)
{
   long argument = 0;
   va_list arguments;
   va_start(arguments, command);
   if (command != F_GETFD && command != F_GETFL)
      argument = va_arg(arguments, long);
   va_end(arguments);

   if (command == F_SETFD && (argument & ~FD_CLOEXEC) == 0)
      return 0;
   return _fcntl(descriptor, command, argument);
}

int pipe2(int descriptors[2], int flags)
{
   const int allowed = O_NONBLOCK | O_CLOEXEC;
   if (!descriptors || (flags & ~allowed) != 0) {
      errno = EINVAL;
      return -1;
   }
   if (pipe(descriptors) != 0)
      return -1;

   for (int i = 0; i < 2; ++i) {
      if ((flags & O_NONBLOCK) != 0) {
         const int old_flags = fcntl(descriptors[i], F_GETFL, 0);
         if (old_flags < 0 || fcntl(descriptors[i], F_SETFL, old_flags | O_NONBLOCK) < 0)
            goto failed;
      }
      if ((flags & O_CLOEXEC) != 0 && fcntl(descriptors[i], F_SETFD, FD_CLOEXEC) < 0)
         goto failed;
   }
   return 0;

failed: {
      const int saved_errno = errno;
      close(descriptors[0]);
      close(descriptors[1]);
      errno = saved_errno;
      return -1;
   }
}

unsigned int if_nametoindex(const char *name)
{
   return ps5_if_nametoindex(name);
}

int _setjmp(jmp_buf environment)
{
   return setjmp(environment);
}

void _longjmp(jmp_buf environment, int value)
{
   longjmp(environment, value);
}

int sendmmsg(int socket, void *messages, unsigned int count, unsigned int flags)
{
   (void)socket; (void)messages; (void)count; (void)flags;
   errno = ENOSYS;
   return -1;
}

int recvmmsg(int socket, void *messages, unsigned int count, unsigned int flags, void *timeout)
{
   (void)socket; (void)messages; (void)count; (void)flags; (void)timeout;
   errno = ENOSYS;
   return -1;
}

int fnmatch(const char *pattern, const char *value, int flags)
{
   /* The title never requests libcurl's wildcard-transfer mode; reject it
    * explicitly rather than depending on the webkit-only libc export. */
   (void)pattern; (void)value; (void)flags;
   return FNM_NOMATCH;
}

/* Zstd's trace hooks are optional instrumentation and are not enabled in this
 * title. Their weak references still need concrete symbols in a SELF image. */
void ZSTD_trace_decompress_begin(void) { }
void ZSTD_trace_decompress_end(void) { }
