// -*- compile-command: "gcc -O2 -Wall dh.c -o dh" -*- 

/*

  schmir's daemon helper - easily start programs in background
  
  Last changed: 2009-10-09 00:52:13 by ralf

*/

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>

#define rundir "/var/log/dh/"


extern char **environ;

void usage()
{
	printf("Usage: dh [-c] [-p lockfile] command ...\n");
#ifdef DHVERSION
	printf("[%s]\n", DHVERSION);
#endif
}

int set_close_on_exec(fd)
{
	int flags = fcntl(fd, F_GETFD);
	if (flags == -1) {
		return -1;
	}

	flags |= FD_CLOEXEC;
	if (fcntl(fd, F_SETFD, flags) == -1) {
		return -1;
	}
	return 0;
}


void close_all_fds()
{
	int maxfd = sysconf(_SC_OPEN_MAX);
	if (maxfd==-1) {
		maxfd = 1024;
	}
	while (maxfd>3) {
		--maxfd;
		close(maxfd);
	}
}

void perror3(const char *msg) 
{
	perror(msg);
	dup2(3,2);
	perror(msg);
	exit(10);
}

void fork_and_exit_parent()
{
	int pid;
	pid = fork();
	if (pid==-1) {
		perror3("fork failed");
	}

	if (pid>0) {
		exit(0);
	}
}	

void print_header()
{
	char **e = environ;
	fprintf(stderr, "%d\n", getpid());
	fprintf(stderr, "%d\n", getppid());
	while (e && *e) {
		fprintf(stderr, "%s\n", *e);
		++e;
	}
}

int main(int argc, char * const argv[])
{
	int opt;
	int do_clear_environ = 0;
	const char *lockfile = 0;
	char lockfile_fp[1024];

	int lockfd;

	while ((opt = getopt(argc, argv, "+cp:h")) != -1) {
		switch (opt) {
		case 'c':
			do_clear_environ = 1;
			break;
		case 'p':
			lockfile = optarg;
			break;
		case 'h':
			usage();
			exit(0);
		default:
			usage();
			exit(1);
		}
	}
	
	if (optind>=argc) {
		fprintf(stderr, "dh: missing command parameter\n");
		exit(10);
	}



	strcpy(lockfile_fp, rundir);
	if (!lockfile) {
		strcat(lockfile_fp, basename(argv[optind]));
	} else {
		if (strchr(lockfile, '/')) {
			strcpy(lockfile_fp, lockfile);
		} else {
			strcat(lockfile_fp, lockfile);
		}
	}

	printf("dh: logging to %s\n", lockfile_fp);
	fflush(stdout);
	close_all_fds();

	// --- close stdin
	close(0);
	if(open("/dev/null", O_RDONLY)!=0) {
		perror("dh: open /dev/null failed");
		exit(10);
	}
	
	
	// --- open logfile and flock it
	close(1);
	lockfd = open(lockfile_fp, O_WRONLY | O_CREAT | O_NOFOLLOW, 0755);
	if (lockfd!=1) {
		perror("dh: could not open logfile");
		exit(10);
	}
	
	if (flock(lockfd, LOCK_EX | LOCK_NB)==-1) {
		if (errno==EWOULDBLOCK) {
			fprintf(stderr, "dh: logfile still locked\n");
			exit(11);
		} else {
			perror("dh: could not lock logfile");
			exit(10);
		}
	}
	
	
	dup2(2, 3); // save stderr
	dup2(1, 2); // redirect stderr to logfile

	if (set_close_on_exec(3)==-1) {
		perror("dh: fcntl failed");
		exit(10);
	}
	
	if (ftruncate(1, 0)==-1) {
		perror("dh: ftruncate failed");
		exit(10);
	}


	fork_and_exit_parent();
	if (setsid()==-1) {
		perror3("dh: setsid failed");
	}
	fork_and_exit_parent();


	print_header();

	// --- exec
	execvp(argv[optind], argv+optind);
	perror3("dh: exec failed");
	
	exit(10);
}
