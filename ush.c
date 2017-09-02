#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <getopt.h>
#include <err.h>
#include <errno.h>

#define MAX 64
#define QUOTE(x) #x
#define STR(x) QUOTE(x)

struct cmd {
	char **argv;
	int argc, sin, sout;
	pid_t pid;
};

static int
parse_fd(const char *arg)
{
	char *end;
	long fd = strtol(arg, &end, 10);
	if (*end != '\0' || fd < 0 || fd > INT_MAX) {
		fprintf(stderr, "ush: invalid file descriptor %s\n", arg);
		exit(1);
	}
	return (int)fd;
}

static int
dial(const char *net, int type, int oflag)
{
	const char *servname = strchr(net, '/');
	if (servname == NULL) { errx(1, "service/port name missing"); }
	if (servname - net > 255) { errx(1, "host name too long"); }

	char hostname[256];
	memcpy(hostname, net, servname - net);
	hostname[servname - net] = '\0';
	servname++;

	int fd = -1, s = -1;
	struct addrinfo hints, *res;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = type;
	hints.ai_flags = oflag & O_WRONLY ? 0 : AI_PASSIVE;

	int ec = getaddrinfo(hostname, servname, &hints, &res);
	if (ec) { errx(1, "%s", gai_strerror(ec)); }

	const char *source = "unknown";

	for (struct addrinfo *r = res; res; close(s), res = res->ai_next) {
		s = socket(r->ai_family, r->ai_socktype, r->ai_protocol);
		if (s < 0) { err(1, "failed to create socket"); }

		if (oflag & O_WRONLY) {
			if (connect(s, r->ai_addr, r->ai_addrlen) < 0) {
				ec = errno;
				source = "connect";
				continue;
			}
			fd = s;
		}
		else {
			if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int)) < 0) {
				ec = errno;
				source = "set SO_REUSEADDR";
				continue;
			}
			if (bind(s, r->ai_addr, r->ai_addrlen) < 0) {
				ec = errno;
				source = "bind";
				continue;
			}
			if (listen(s, 1) < 0) {
				ec = errno;
				source = "listen";
				continue;
			}

			struct sockaddr_storage addr;
			socklen_t len = sizeof(addr);
			fd = accept(s, (struct sockaddr *)&addr, &len);
			if (fd < 0) {
				ec = errno;
				source = "accept";
				continue;
			}
			close(s);
		}
		break;
	}
	freeaddrinfo(res);
	if (fd < 0) {
		errno = ec;
		err(1, "failed to %s", source);
	}
	return fd;
}

static void
redirect(int dst, const char *arg, int oflag)
{
	int fd = -1;
	int exclusive = 0;

	if (*arg == '@') {
		fd = parse_fd(arg+1);
	}
	else if (strncmp(arg, "/dev/", 5) == 0 && access(arg, F_OK) < 0) {
		const char *dev = arg + 5;
		if (strncmp(dev, "fd/", 3) == 0) { fd = parse_fd(arg+3); }
		else if (strcmp(dev, "stdin") == 0) { fd = STDIN_FILENO; }
		else if (strcmp(dev, "stdout") == 0) { fd = STDOUT_FILENO; }
		else if (strcmp(dev, "stderr") == 0) { fd = STDERR_FILENO; }
		else {
			int type = -1;
			if (strncmp(dev, "tcp/", 4) == 0) { type = SOCK_STREAM; }
			else if (strncmp(dev, "udp/", 4) == 0) { type = SOCK_DGRAM; }
			if (type != -1) {
				fd = dial(dev + 4, type, oflag);
				exclusive = 1;
			}
		}
	}

	if (fd < 0) {
		fd = open(arg, oflag, 0644);
		if (fd < 0) {
			err(1, "failed to open %s", arg);
		}
		exclusive = 1;
	}

	close(dst);
	if (dup2(fd, dst) < 0) {
		err(1, "failed to duplicate file descriptor %d to %d", fd, dst);
	}
	if (exclusive) {
		close(fd);
	}
}

static int
duplicate(const char *arg, int move)
{
	char *end;
	long fd1 = strtol(arg, &end, 10), fd2 = -1;
	if (*end == ',') { fd2 = strtol(end+1, &end, 10); }
	if (*end != '\0' || fd1 < 0 || fd1 > INT_MAX || fd2 < 0 || fd2 > INT_MAX) {
		fprintf(stderr, "ush: invalid file descriptor pair %s\n", arg);
		exit(1);
	}
	if (dup2((int)fd1, (int)fd2) < 0) {
		err(1, "failed to duplicate file descriptor %ld to %ld", fd1, fd2);
	}
	if (move) {
		close(fd1);
	}
	return (int)fd2;
}

int
main(int argc, char **argv)
{
	struct cmd cmd[MAX], *c;
	int ncmd = 0, i, start, ch, oflag = O_TRUNC;

	while ((ch = getopt(argc, argv, "+:hai:o:e:d:c:m:")) != -1) {
		switch (ch) {
		case 'a': oflag = O_APPEND; continue;
		case 'i': redirect(STDIN_FILENO, optarg, O_RDONLY); break;
		case 'o': redirect(STDOUT_FILENO, optarg, O_WRONLY|O_CREAT|oflag); break;
		case 'e': redirect(STDERR_FILENO, optarg, O_WRONLY|O_CREAT|oflag); break;
		case 'd': duplicate(optarg, 0); break;
		case 'c': close(parse_fd(optarg)); break;
		case 'm': duplicate(optarg, 1); break;
		case 'h':
			if (access(STR(PREFIX) "/share/man/man3/ush.3", F_OK) == 0) {
				execl("/usr/bin/man", "man", STR(PREFIX) "/share/man/man3/ush.3", NULL);
			}
			fprintf(stderr, "usage: ush "
					"[-ha] "
					"[-i IN] [-o OUT] [-e ERR] [-d FD,FD2] [-c FD] [-m FD,FD2] "
					"CMD1 [ARGS1...] [\\| CMD2 [ARGS2...]...]\n");
			return 0;
		case '?': errx(1, "invalid option: -%c", optopt);
		case ':': errx(1, "missing argument for option: -%c", optopt);
		}
		oflag = O_TRUNC;
	}
	argc -= optind;
	argv += optind;

	for (i = 0, start = 0; i <= argc; i++) {
		if (argv[i] == NULL || strcmp(argv[i], "|") == 0) {
			if (ncmd == MAX) { errx(1, "too many commands (> %d", MAX); }

			struct cmd *c = &cmd[ncmd++];
			c->argv = &argv[start];
			c->argc = i - start;
			if (c->argc == 0) { errx(1, "invalid pipe"); }

			argv[i] = NULL;
			start = i + 1;
		}
	}

	cmd[0].sin = STDIN_FILENO;
	for (i = 1; i < ncmd; i++) {
		int fds[2];
		if (pipe(fds) < 0) { err(1, "pipe failed"); }
		if (fcntl(fds[0], F_SETFD, FD_CLOEXEC) < 0) { err(1, "fcntl failed"); }
		if (fcntl(fds[1], F_SETFD, FD_CLOEXEC) < 0) { err(1, "fcntl failed"); }
		cmd[i-1].sout = fds[1];
		cmd[i].sin = fds[0];
	}
	cmd[i-1].sout = STDOUT_FILENO;

	for (i = 0, c = cmd; i < ncmd; i++, c++) {
		c->pid = fork();
		if (c->pid < 0) {
			warn("fork failed");
			break;
		}
		if (c->pid == 0) {
			dup2(c->sin, STDIN_FILENO);
			dup2(c->sout, STDOUT_FILENO);
			if (execvp(c->argv[0], c->argv) < 0) { err(1, "exec failed"); }
		}
	}

	for (ncmd = i, i = 0, c = cmd; i < ncmd; i++, c++) {
		int stat;
		if (waitpid(c->pid, &stat, 0) == c->pid) {
			close(c->sin);
			close(c->sout);
		}
	}
}

