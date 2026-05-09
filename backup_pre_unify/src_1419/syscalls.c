/* =========================================================================
 *  syscalls.c — stubs for libc functions that RELIC pulls in
 *
 *  This file is ONLY included when building the RELIC benchmark target,
 *  not the AmorE target. AmorE uses a smaller libc surface and doesn't
 *  need these stubs.
 * ========================================================================= */

#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>

extern void LED_Set(uint32_t pattern);

int _write(int fd, const char *buf, int len) {
    (void)fd; (void)buf;
    return len;
}

int _read(int fd, char *buf, int len) {
    (void)fd; (void)buf; (void)len;
    return 0;
}

int _close(int fd) { (void)fd; return -1; }

int _lseek(int fd, int ptr, int dir) {
    (void)fd; (void)ptr; (void)dir;
    return 0;
}

int _fstat(int fd, struct stat *st) {
    (void)fd;
    if (st) st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int fd) { (void)fd; return 1; }
int _getpid(void)   { return 1; }

int _kill(int pid, int sig) {
    (void)pid; (void)sig;
    errno = EINVAL;
    return -1;
}

void _exit(int status) {
    (void)status;
    LED_Set(0x6);
    while (1) { __asm__("nop"); }
}

extern char _end;
extern char _estack;

void *_sbrk(int incr) {
    static char *heap_ptr = 0;
    if (heap_ptr == 0) heap_ptr = &_end;

    char *prev = heap_ptr;
    char *new_ptr = heap_ptr + incr;

    /* Reserve 8KB for stack — compare via uintptr_t to silence warnings */
    uintptr_t stack_top  = (uintptr_t)(&_estack);
    uintptr_t heap_limit = stack_top - 0x2000u;

    if ((uintptr_t)new_ptr > heap_limit) {
        errno = ENOMEM;
        return (void *)-1;
    }
    heap_ptr = new_ptr;
    return prev;
}
