#ifndef _SIGNAL_H
#define _SIGNAL_H

/* Signal numbers (POSIX/Unix conventional values). */
/** Abnormal termination signal (abort). */
#define SIGABRT  6
/** Erroneous arithmetic operation. */
#define SIGFPE   8
/** Illegal instruction. */
#define SIGILL   4
/** Interactive attention signal. */
#define SIGINT   2
/** Invalid memory access. */
#define SIGSEGV 11
/** Termination request. */
#define SIGTERM 15

/** Signal handler function type. */
typedef void (*_sig_fn_t)(int);

/** Default signal action: terminate the program. */
#define SIG_DFL ((_sig_fn_t)0)
/** Ignore the signal. */
#define SIG_IGN ((_sig_fn_t)1)
/** Returned by signal() on error. */
#define SIG_ERR ((_sig_fn_t)-1)

/** Install func as the handler for signal sig; returns the previous handler or SIG_ERR.
 *  On CP/M 2.2 there is no async signal delivery, so SIG_ERR is always returned. */
void (*signal(int sig, void (*func)(int)))(int);

/** Send signal sig to the running program.
 *  SIGABRT calls abort(); all other signals are no-ops on CP/M 2.2. */
int raise(int sig);

#endif /* _SIGNAL_H */
