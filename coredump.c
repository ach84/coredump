#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>

#define APP_NAME "coredump"

static struct tm *ptm = NULL;
static int log_fd = -1;
static pid_t pid = 0;
static char data[0x100000];
static unsigned int max_core_size = 2048; // 2GB core size limit [MB]
static unsigned int max_app_limit = 2049; // 2GB + 1 per app limit [MB]
static const char *default_name = "corefile";
static const char *default_path = "/opt/coredump";
static const char *default_log = "/var/log/coredump.log";

static void debug(const char *cmd, ...)
{
	char hdr[128];
	char msg[128];
	va_list va;
	struct timeval tv;

	if (log_fd == -1 || !ptm)
		return; 

	va_start(va, cmd);
	vsnprintf(msg, sizeof(msg), cmd, va);
	va_end(va);

	gettimeofday(&tv, NULL);

	snprintf(hdr, sizeof(hdr),
		"[%04d/%02d/%02d %02d:%02d:%02d %02d.%06d] %s[%d]: ",
		ptm->tm_year+1900,
		ptm->tm_mon+1,
		ptm->tm_mday,
		ptm->tm_hour,
		ptm->tm_min,
		ptm->tm_sec,
		(unsigned char)tv.tv_sec, (int)tv.tv_usec,
		APP_NAME, pid);

	write(log_fd, hdr, strlen(hdr));
	write(log_fd, msg, strlen(msg));
}

static int rotate(char *path, char *name)
{
	int i, n, res, rotate_idx = 0;
	struct dirent **dlist;
	struct dirent *pent;
	struct stat st;
	u_int64_t total = 0, max_len;
	char corename[PATH_MAX];
	char rotate_file[PATH_MAX];

	if ((n = scandir(path, &dlist, 0, alphasort)) < 0) {
		free(dlist);
		return 1;
	}

	max_len = (u_int64_t)max_app_limit * 1024 * 1024;
	for (i = 0; i < n; i++) {
		pent = dlist[i];

		if (pent->d_type != DT_REG)
			continue;

		if ((res = strncmp(pent->d_name, name, strlen(name))) != 0)
			continue;

		snprintf(corename, sizeof(corename),
			"%s/%s", path, pent->d_name);

		if ((res = stat(corename, &st)) != 0)
			continue;

		if (rotate_idx == 0)
			rotate_idx = i;

		total += st.st_size;

		debug("%s file: %s size: %d total: %lu\n",
			name, pent->d_name, st.st_size, total);

		if (total >= max_len) {
			snprintf(rotate_file, sizeof(rotate_file),
				"%s/%s", path, dlist[rotate_idx]->d_name);
			res = unlink(rotate_file);
			debug("%s rotate_file: %s result: %d\n", name, rotate_file, res);
			rotate_idx ++;
		}
	}
	free(dlist);

	return 0;
}

int usage(char *name)
{
	printf("\nUsage: %s [-epacdh]\n",name);
	printf("\nOptions:\n");
	printf("   -e name (default:%s)\n", default_name);
	printf("   -p path (default:%s)\n", default_path);
	printf("   -a application limit [MB] (default:%u)\n", max_app_limit);
	printf("   -c core size limit [MB] (default:%u)\n", max_core_size);
	printf("   -d log enable (%s)\n", default_log);
	printf("\n");
	return -1;
}

int main(int argc, char *argv[])
{
	int fd, c, size, res;
	u_int64_t total = 0, max_size;
	char *path = NULL;
	char *exe  = NULL;
	char filename[PATH_MAX];
	char name[64];
	time_t now;
	struct stat st;

	while ((c = getopt(argc, argv, "e:p:a:c:dh")) != -1) {
		switch (c) {
		case 'e':		
			exe = strdup(optarg);
			break;
		case 'p':
			path = strdup(optarg);
			break;
		case 'a':
			max_app_limit = strtoul(optarg, NULL, 10);
			break;
		case 'c':
			max_core_size = strtoul(optarg, NULL, 10);
			break;
		case 'd':
			log_fd = open(default_log, O_RDWR|O_CREAT|O_APPEND, 0644);
			break;
		case 'h':
		case '?':
		default:
			return usage(argv[0]);
		}
	}

	if (!exe)
		exe = (char *)default_name;

	if (!path)
		path = (char *)default_path;

	now = time(NULL);
	ptm = (struct tm *)malloc(sizeof(struct tm));
	localtime_r(&now, ptm);
	pid = getpid();

	debug("init args: %s,%s limits: %u,%u [MB]\n",
		path, exe, max_core_size, max_app_limit);

	snprintf(name, sizeof(name), "%s", exe);
	snprintf(filename, sizeof(filename),
		"%s/%s-%04d%02d%02d%02d%02d%02d.core",
		path, name,
		ptm->tm_year+1900,
		ptm->tm_mon+1,
		ptm->tm_mday,
		ptm->tm_hour,
		ptm->tm_min,
		ptm->tm_sec);

	if ((res = stat(filename, &st)) == 0)
		return -EBUSY;

	if ((res = rotate(path, name)) != 0)
		return -ENOENT;

	if ((fd = open(filename, O_CREAT|O_WRONLY|O_TRUNC, 0644)) < 0)
		return fd;

	max_size = (u_int64_t)max_core_size * 1024 * 1024;
	while ((size = fread(data, 1, sizeof(data), stdin)) > 0) {
		res = write(fd, data, size);
		total += size;
		debug("%s res: %d size: %d total: %lu\n",
			name, res, size, total);
		if (total >= max_size || res < 0)
			break;
	}

	debug("done res: %d\n", size);
	close(fd);

	if (log_fd >=  0)
		close(log_fd);

	free(ptm);
	return 0;
}
