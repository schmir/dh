/*

    Define author
        John Kelly, October 6, 2007

    Define copyright
        Copyright John Kelly, 2007. All rights reserved.

    Define license
        Licensed under the Apache License, Version 2.0 (the "License");
        you may not use this work except in compliance with the License.
        You may obtain a copy of the License at:
        http://www.apache.org/licenses/LICENSE-2.0

    Define symbols and (words)
        Np ...........  North pipe
        Sp ...........  South pipe
        Es ...........  Epoch seconds
        fd ...........  file descriptor
        lockbase .....  lock file
        lockdir ......  lock directory
        lockfile .....  lock absolute path
        lockname .....  lock file or path (command line input)
        tn ...........  temporary integer
        ts ...........  temporary string
        tt ...........  temporary time
        tu ...........  temporary size_t
        tz ...........  temporary ssize_t

*/

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <fcntl.h>
#include <libgen.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define INTERVAL_LIMIT 256
#define INTERVAL_TIME 1048576
#define LOGTIME(tt) logtime ((tt), sizeof ((tt)))

extern char **environ;

static void
trim (char **ts)
{
    unsigned char *exam;
    unsigned char *keep;

    exam = (unsigned char *) *ts;
    while (*exam && isspace (*exam)) {
        ++exam;
    }
    *ts = (char *) exam;
    if (!*exam) {
        return;
    }
    keep = exam;
    while (*++exam) {
        if (!isspace (*exam)) {
            keep = exam;
        }
    }
    if (*++keep) {
        *keep = '\0';
    }
}

static void
logtime (char *tt, size_t tu)
{
    time_t Es;
    struct tm *dt;

    if (tu < 2) {
        errno = EINVAL;
        printf ("logtime: %s\n", strerror (errno));
    } else {
        Es = time (NULL);
        if (!(dt = localtime (&Es))) {
            tt[0] = '?';
            tt[1] = '\0';
            errno = EPROTO;
            printf ("logtime: %s\n", strerror (errno));
        } else if (strftime (tt, tu, "%a %b %e %H:%M:%S %Z %Y", dt) == 0) {
            tt[0] = '?';
            tt[1] = '\0';
            errno = EOVERFLOW;
            printf ("logtime: %s\n", strerror (errno));
        }
    }
}

static void
clear_environment()
{
    char *path =
        "/sbin:/bin:/usr/sbin:/usr/bin:/usr/local/sbin:/usr/local/bin";

    *environ = NULL;
    if (setenv ("SHELL", "/bin/sh", 1) == -1) {
	printf ("failure setting SHELL: %s\n", strerror (errno));
	exit (EXIT_FAILURE);
    }
    if (setenv ("PATH", path, 1) == -1) {
	printf ("failure setting PATH: %s\n", strerror (errno));
	exit (EXIT_FAILURE);
    }
}

int
main (int argc, char **argv)
{
    int Np[2];
    int Sp[2];
    int fd;
    int flags;
    int open_max;
    int tn;
    int usage;
    size_t tu;
    ssize_t tz;
    mode_t mode;
    pid_t pid, ppid;
    struct sigaction signow;
    struct timespec interval;
    char *copy;
    char *lockbase;
    char *lockdir;
    char *lockfile;
    char *lockname;
    char *rundir = "/var/run/dh";
    char *this;
    char **that;
    char ts[512];
    char tt[128];
    int clear;

    errno = 0;
    opterr = 0;
    usage = 0;
    lockname = NULL;
    clear=0;
    while (!usage && (tn = getopt (argc, argv, "+p:c")) != -1) {
        switch (tn) {
        case 'p':
            lockname = optarg;
            trim (&lockname);
            if (!*lockname) {
                usage = 1;
            }
            break;
	case 'c':
	    clear = 1;
	    break;
        default:
            usage = 1;
        }
    }
    if (!usage) {
        if (argc - optind < 1) {
            usage = 1;
        } else {
            trim (&(argv[optind]));
            if (!*argv[optind]) {
                usage = 1;
            }
        }
    }
    if (usage) {
        fprintf (stderr, "Usage: %s [-c] [-p lock.pid] daemon args\n",
            basename (*argv));
        exit (EXIT_FAILURE);
    }
    if (lockname && strlen (lockname) > sizeof (ts) / 2 - 1) {
        errno = EINVAL;
        fprintf (stderr, "lock.pid: %s (too long)\n", strerror (errno));
        exit (EXIT_FAILURE);
    }
    argv += optind;
    if (strlen (*argv) > sizeof (ts) / 2 - 1) {
        errno = EINVAL;
        fprintf (stderr, "daemon: %s (too long)\n", strerror (errno));
        exit (EXIT_FAILURE);
    }
    (void) sigfillset (&signow.sa_mask);
    signow.sa_flags = 0;
    signow.sa_handler = SIG_IGN;
    if (sigaction (SIGINT, &signow, NULL) == -1) {
        perror ("failure initializing SIGINT action");
        exit (EXIT_FAILURE);
    }
    if (sigaction (SIGQUIT, &signow, NULL) == -1) {
        perror ("failure initializing SIGQUIT action");
        exit (EXIT_FAILURE);
    }
    if (sigaction (SIGTSTP, &signow, NULL) == -1) {
        perror ("failure initializing SIGTSTP action");
        exit (EXIT_FAILURE);
    }
    if (sigaction (SIGTTIN, &signow, NULL) == -1) {
        perror ("failure initializing SIGTTIN action");
        exit (EXIT_FAILURE);
    }
    if (sigaction (SIGTTOU, &signow, NULL) == -1) {
        perror ("failure initializing SIGTTOU action");
        exit (EXIT_FAILURE);
    }
    signow.sa_handler = SIG_DFL;
    if (sigaction (SIGHUP, &signow, NULL) == -1) {
        perror ("failure initializing SIGHUP action");
        exit (EXIT_FAILURE);
    }
    if (sigaction (SIGILL, &signow, NULL) == -1) {
        perror ("failure initializing SIGILL action");
        exit (EXIT_FAILURE);
    }
    if (sigaction (SIGABRT, &signow, NULL) == -1) {
        perror ("failure initializing SIGABRT action");
        exit (EXIT_FAILURE);
    }
    if (sigaction (SIGBUS, &signow, NULL) == -1) {
        perror ("failure initializing SIGBUS action");
        exit (EXIT_FAILURE);
    }
    if (sigaction (SIGFPE, &signow, NULL) == -1) {
        perror ("failure initializing SIGFPE action");
        exit (EXIT_FAILURE);
    }
    if (sigaction (SIGSEGV, &signow, NULL) == -1) {
        perror ("failure initializing SIGSEGV action");
        exit (EXIT_FAILURE);
    }
    if (sigaction (SIGPIPE, &signow, NULL) == -1) {
        perror ("failure initializing SIGPIPE action");
        exit (EXIT_FAILURE);
    }
    if (sigaction (SIGALRM, &signow, NULL) == -1) {
        perror ("failure initializing SIGALRM action");
        exit (EXIT_FAILURE);
    }
    if (sigaction (SIGTERM, &signow, NULL) == -1) {
        perror ("failure initializing SIGTERM action");
        exit (EXIT_FAILURE);
    }
    if (sigaction (SIGUSR1, &signow, NULL) == -1) {
        perror ("failure initializing SIGUSR1 action");
        exit (EXIT_FAILURE);
    }
    if (sigaction (SIGUSR2, &signow, NULL) == -1) {
        perror ("failure initializing SIGUSR2 action");
        exit (EXIT_FAILURE);
    }
    if (sigaction (SIGCHLD, &signow, NULL) == -1) {
        perror ("failure initializing SIGCHLD action");
        exit (EXIT_FAILURE);
    }
    if (sigaction (SIGCONT, &signow, NULL) == -1) {
        perror ("failure initializing SIGCONT action");
        exit (EXIT_FAILURE);
    }
    if ((open_max = (int) sysconf (_SC_OPEN_MAX)) == -1) {
        fprintf (stderr, "failure querying _SC_OPEN_MAX: %s\n",
            strerror (errno));
        exit (EXIT_FAILURE);
    }
    for (fd = 3; fd < open_max; fd++) {
        if (close (fd) == -1) {
            if (errno == EBADF) {
                continue;
            }
            fprintf (stderr, "failure closing file descriptor %u: %s\n",
                (unsigned int) fd, strerror (errno));
            exit (EXIT_FAILURE);
        }
    }
    errno = 0;
    if (fcntl (STDIN_FILENO, F_GETFD) == -1 && errno == EBADF) {
        if (open ("/dev/null", O_RDONLY) != STDIN_FILENO) {
            perror ("failure opening STDIN placeholder");
            exit (EXIT_FAILURE);
        }
    }
    if (fcntl (STDOUT_FILENO, F_GETFD) == -1 && errno == EBADF) {
        if (open ("/dev/null", O_WRONLY) != STDOUT_FILENO) {
            perror ("failure opening STDOUT placeholder");
            exit (EXIT_FAILURE);
        }
    }
    if (fcntl (STDERR_FILENO, F_GETFD) == -1 && errno == EBADF) {
        if (open ("/dev/null", O_WRONLY) != STDERR_FILENO) {
            perror ("failure opening STDERR placeholder");
            exit (EXIT_FAILURE);
        }
    }
    if (pipe (Np) == -1) {
        perror ("failure creating North pipe");
        exit (EXIT_FAILURE);
    }
    if (pipe (Sp) == -1) {
        perror ("failure creating South pipe");
        exit (EXIT_FAILURE);
    }
    pid = fork ();
    if (pid < 0) {
        perror ("failure creating 1st child");
        exit (EXIT_FAILURE);
    } else if (pid > 0) {
        if (close (Np[1]) == -1) {
            perror ("failure closing North pipe upper[1]");
            exit (EXIT_FAILURE);
        }
        if (close (Sp[0]) == -1) {
            perror ("failure closing South pipe upper[0]");
            exit (EXIT_FAILURE);
        }
        if (waitpid (pid, NULL, 0) == -1) {
            perror ("failure waiting for 1st child");
            exit (EXIT_FAILURE);
        }
        if (close (Sp[1]) == -1) {
            perror ("failure closing South pipe upper[1]");
            exit (EXIT_FAILURE);
        }
        if (sizeof (ts) < 3) {
            errno = EOVERFLOW;
            perror ("failure specifying North pipe storage");
            exit (EXIT_FAILURE);
        }
        tn = 0;
        this = ts;
        tu = sizeof (ts) - 1;
        while ((tz = read (Np[0], this, tu)) > 0) {
            tu -= tz;
            tn += tz;
            this += tz;
        }
        if (tz == -1) {
            perror ("failure reading North pipe upper[0]");
            exit (EXIT_FAILURE);
        }
        if (tn < 1) {
            exit (EXIT_FAILURE);
        } else if (tn > 1) {
            *this = '\0';
            this = ts;
            fprintf (stderr, "%s", ++this);
            exit (EXIT_FAILURE);
        }
        exit (EXIT_SUCCESS);
    } else {
        if (setsid () == -1) {
            perror ("failure starting new session");
            exit (EXIT_FAILURE);
        }
        pid = fork ();
        if (pid < 0) {
            perror ("failure creating 2nd child");
            exit (EXIT_FAILURE);
        } else if (pid > 0) {
            exit (EXIT_SUCCESS);
        } else {
            if (close (Np[0]) == -1) {
                perror ("failure closing North pipe lower[0]");
                exit (EXIT_FAILURE);
            }
            if (close (Sp[1]) == -1) {
                perror ("failure closing South pipe lower[1]");
                exit (EXIT_FAILURE);
            }
            tz = read (Sp[0], ts, 1);
            if (tz == -1) {
                perror ("failure reading South pipe lower[0]");
                exit (EXIT_FAILURE);
            } else if (tz > 0) {
                errno = EPROTO;
                perror ("failure reading South pipe lower[0]");
                exit (EXIT_FAILURE);
            }
            if (close (Sp[0]) == -1) {
                perror ("failure closing South pipe lower[0]");
                exit (EXIT_FAILURE);
            }
            tn = 0;
            interval.tv_sec = 0;
            interval.tv_nsec = INTERVAL_TIME;
            while ((ppid = getppid ()) != 1 && tn < INTERVAL_LIMIT) {
                ++tn;
                if (nanosleep (&interval, NULL) == -1 && errno != EINTR) {
                    perror ("failure waiting for init");
                    exit (EXIT_FAILURE);
                }
            }
            if (ppid != 1) {
                fprintf (stderr,
                    "failure waiting for init after %d intervals\n", tn);
                exit (EXIT_FAILURE);
            }
            (void) umask (S_IWGRP | S_IRWXO);
            if (chdir ("/") == -1) {
                perror ("failure changing working directory");
                exit (EXIT_FAILURE);
            }
            if ((lockdir = strdup (rundir)) == NULL) {
                perror ("failure allocating rundir");
                exit (EXIT_FAILURE);
            }
            if (lockname) {
                if ((copy = strdup (lockname)) == NULL) {
                    perror ("failure copying lockbase");
                    exit (EXIT_FAILURE);
                }
                if ((this = basename (copy)) == NULL) {
                    errno = EPROTO;
                    perror ("failure deriving lockbase");
                    exit (EXIT_FAILURE);
                }
                if (!strcmp (this, ".") || !strcmp (this, "/")) {
                    lockname = NULL;
                } else {
                    if ((lockbase = strdup (this)) == NULL) {
                        perror ("failure allocating lockbase");
                        exit (EXIT_FAILURE);
                    }
                    if ((copy = strdup (lockname)) == NULL) {
                        perror ("failure copying lockdir");
                        exit (EXIT_FAILURE);
                    }
                    if ((this = dirname (copy)) == NULL) {
                        errno = EPROTO;
                        perror ("failure deriving lockdir");
                        exit (EXIT_FAILURE);
                    }
                    if (!strcmp (this, ".") || !strcmp (this, "/")) {
                    } else {
                        if ((lockdir = strdup (this)) == NULL) {
                            perror ("failure allocating lockdir");
                            exit (EXIT_FAILURE);
                        }
                    }
                }
            }
            if (!lockname) {
                if ((copy = strdup (*argv)) == NULL) {
                    perror ("failure copying execbase");
                    exit (EXIT_FAILURE);
                }
                if ((this = basename (copy)) == NULL) {
                    errno = EPROTO;
                    perror ("failure deriving execbase");
                    exit (EXIT_FAILURE);
                }
                tn = snprintf (ts, sizeof (ts), "%s.pid", this);
                if (tn < 0 || tn >= (int) (sizeof (ts))) {
                    errno = EOVERFLOW;
                    perror ("failure suffixing execbase");
                    exit (EXIT_FAILURE);
                }
                if ((lockbase = strdup (ts)) == NULL) {
                    perror ("failure allocating execbase");
                    exit (EXIT_FAILURE);
                }
            }
            tn = snprintf (ts, sizeof (ts), "%s/%s", lockdir, lockbase);
            if (tn < 0 || tn >= (int) (sizeof (ts))) {
                errno = EOVERFLOW;
                perror ("failure deriving lockfile");
                exit (EXIT_FAILURE);
            }
            if ((lockfile = strdup (ts)) == NULL) {
                perror ("failure allocating lockfile");
                exit (EXIT_FAILURE);
            }
            if (close (STDIN_FILENO) == -1 && errno != EBADF) {
                perror ("failure closing STDIN");
                exit (EXIT_FAILURE);
            }
            if ((tn = open ("/dev/null", O_RDONLY)) != STDIN_FILENO) {
                if (tn != -1) {
                    errno = EPROTO;
                }
                perror ("failure opening STDIN to /dev/null");
                exit (EXIT_FAILURE);
            }
            if (close (STDOUT_FILENO) == -1) {
                perror ("failure closing STDOUT");
                exit (EXIT_FAILURE);
            }
            flags = O_WRONLY | O_CREAT;
            mode = S_IRUSR | S_IWUSR | S_IRGRP;
            if ((tn = open (lockfile, flags, mode)) != STDOUT_FILENO) {
                if (tn != -1) {
                    errno = EPROTO;
                }
                fprintf (stderr, "failure opening STDOUT to %s: %s\n",
                    lockfile, strerror (errno));
                exit (EXIT_FAILURE);
            }
            if (flock (STDOUT_FILENO, LOCK_EX | LOCK_NB) == -1) {
                fprintf (stderr, "failure locking %s: %s\n", lockfile,
                    strerror (errno));
                exit (EXIT_FAILURE);
            }
            if (ftruncate (STDOUT_FILENO, 0) == -1) {
                perror ("failure clearing lock file");
                exit (EXIT_FAILURE);
            }
            if (lseek (STDOUT_FILENO, 0, SEEK_SET) != 0) {
                perror ("failure initializing lock file");
                exit (EXIT_FAILURE);
            }
            if (!fdopen (STDOUT_FILENO, "a")) {
                perror ("failure opening STDOUT stream");
                exit (EXIT_FAILURE);
            }
            if (close (STDERR_FILENO) == -1) {
                printf ("failure closing STDERR: %s\n", strerror (errno));
                exit (EXIT_FAILURE);
            }
            if ((tn = dup2 (STDOUT_FILENO, STDERR_FILENO)) != STDERR_FILENO) {
                if (tn != -1) {
                    errno = EPROTO;
                }
                printf ("failure opening STDERR to %s: %s\n", lockfile,
                    strerror (errno));
                exit (EXIT_FAILURE);
            }
            if (!fdopen (STDERR_FILENO, "a")) {
                printf ("failure opening STDERR stream: %s\n",
                    strerror (errno));
                exit (EXIT_FAILURE);
            }
	    if (clear) {
		clear_environment();
	    }
            printf ("%d\n", getpid ());
            printf ("PGID=%d\n", getpgrp ());
            for (that = environ; that && *that; that++) {
                printf ("%s\n", *that);
            }
            LOGTIME (tt);
            printf ("%s -> start daemon -> ", tt);
            for (that = argv; that && *that; that++) {
                printf ("%s", *that);
                if (*(that + 1)) {
                    printf (" ");
                }
            }
            printf ("\n");
            if (fflush (NULL) == EOF) {
                printf ("failure flushing STDIO stream: %s\n",
                    strerror (errno));
                exit (EXIT_FAILURE);
            }
            if (fcntl (Np[1], F_SETFD, FD_CLOEXEC) == -1) {
                printf ("failure setting North pipe close on exec: %s\n",
                    strerror (errno));
                exit (EXIT_FAILURE);
            }
            if (write (Np[1], "", 1) == -1) {
                printf ("failure writing error protocol flag: %s\n",
                    strerror (errno));
                exit (EXIT_FAILURE);
            }
            (void) execvp (*argv, argv);
            tn = snprintf (ts, sizeof (ts), "failure starting %s: %s\n",
                *argv, strerror (errno));
            if (tn < 0 || tn >= (int) (sizeof (ts))) {
                errno = EOVERFLOW;
                printf ("failure deriving exec error message: %s\n",
                    strerror (errno));
                exit (EXIT_FAILURE);
            }
            if (write (Np[1], ts, tn) == -1) {
                printf ("failure writing exec error message: %s\n",
                    strerror (errno));
                exit (EXIT_FAILURE);
            }
            if (unlink (lockfile) == -1) {
                printf ("failure removing %s: %s\n", lockfile,
                    strerror (errno));
                exit (EXIT_FAILURE);
            }
        }
    }
    exit (EXIT_FAILURE);
}
