#include <assert.h>
#include <errno.h>
#include <dirent.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

const char HELP_MSG[] =
  "--help         -- print this message\n\n"
  "--force        -- if destination directory not specified - create it\n\n"
  "-d [directory] -- destination dir to store backups\n"
  "--dst             -- alias to -d\n"
  "-s [directory] -- source dir files in which to backup\n"
  "--src             -- alias to -s\n\n";

struct Flags {
  uint32_t is_force;
  uint32_t has_src;
  uint32_t has_dst;
};


void pollBackup(const char * dst_dir, const char * src_dir);
int backup(const char * dst_dir, const char * src_dir);

void archive(const char * const fpath);
void printHelp();

int isDirValid(const char * dir);

int main(int argc, char *argv[]) {
  if (argc == 1) {
    fprintf(stderr, "ERROR: invalid program parameters\n");
    printHelp();

    return 1;
  }

  Flags flags = {0};
  char dst_dir[_POSIX_ARG_MAX] = "";
  char src_dir[_POSIX_ARG_MAX] = "";

  uint32_t uargc = argc;
  for (uint32_t i = 1; i < uargc; i++) {

    if (!strcmp(argv[i], "--help")) {
      printHelp();
      return 0;
    }

    if (!strcmp(argv[i], "--force")) {
      flags.is_force = 1;
      continue;
    }

    if (!strcmp(argv[i], "-d") ||
        !strcmp(argv[i], "--dir")) {

        if (i + 1 == uargc) {
          fprintf(stderr, "ERROR: missing directory name after %s\n", argv[i]);
          printHelp();
          return 1;
        }

        if (!isDirValid(argv[i + 1])) {
          fprintf(stderr, "ERROR: invalid directory \"%s\"\n", argv[i + 1]);
          return 1;
        }

        strcpy(dst_dir, argv[i + 1]);
        flags.has_dst = 1;
        i++;

        continue;
      }

    if (!strcmp(argv[i], "-s") ||
        !strcmp(argv[i], "--src")) {

        if (i + 1 == uargc) {
          fprintf(stderr, "ERROR: missing directory name after %s\n", argv[i]);
          printHelp();
          return 1;
        }

        if (!isDirValid(argv[i + 1])) {
          fprintf(stderr, "ERROR: invalid directory \"%s\"\n", argv[i + 1]);
          return 1;
        }

        strcpy(src_dir, argv[i + 1]);
        flags.has_src = 1;
        i++;

        continue;
    }

    fprintf(stderr, "ERROR: invalid option \"%s\", see --help\n", argv[i]);
    printHelp();
    return 1;
  }

  // source dir final settings
  if (!flags.has_src) {
    char *cwd = getcwd(src_dir, _POSIX_ARG_MAX); // src_dir = cwd;
    fprintf(stderr, "LOG: source dir not specified, using cwd instead:\n"
                    "     %s\n", cwd);
  }

  // destination dir final settings
  if (!flags.has_dst && !flags.is_force) {
    fprintf(stderr, "ERROR: destination directory not specified\n"
                    "       create it manually or rerun with --force (see --help)\n");
    return 1;
  }

  if (!flags.has_dst && flags.is_force) {
    strcpy(dst_dir, src_dir);
    strcat(dst_dir, ".bak");

    fprintf(stderr, "LOG: creating destination directory (--force used)\n"
                    "     %s\n", dst_dir);

    execl("/bin/mkdir", "/bin/mkdir", dst_dir, /*sentinel*/(char *)NULL);
  }


  pollBackup(dst_dir, src_dir);

  return 0;
}

void pollBackup(const char *const dst_dir, const char *const src_dir) {
  assert(dst_dir && "dst_dir == NULL");
  assert(src_dir && "src_dir == NULL");

  while(1) {
    backup(dst_dir, src_dir);
    usleep(1 << 14);
  }
}

int backup(const char *dst_dir, const char *src_dir) {
  assert(dst_dir);
  assert(src_dir);

  DIR *curr_dir = opendir(src_dir);
  if (!curr_dir) {
    fprintf(stderr, "ERROR: opendir() failed to open \"%s\"\n", src_dir);
    return 1;
  }

  dirent *curr_dir_content = readdir(curr_dir);
  while (curr_dir_content != NULL) {
    printf("%s\n", curr_dir_content->d_name);
    curr_dir_content = readdir(curr_dir);
  }

  return 0;
}

void archive(const char *const fpath) {
  assert(fpath);

  fprintf(stderr, "Archiving %s\n", fpath);
  execl("gzip", "gzip", fpath, /*sentinel*/ (char *)NULL);
}

void printHelp() { fprintf(stdout, HELP_MSG); }

int isDirValid(const char * dir) {
  if (!dir) return 0;

  struct stat dir_stat = {};
  if (!stat(dir, &dir_stat)) return S_ISDIR(dir_stat.st_mode);

  fprintf(stderr, "%s: ", __FUNCTION__);
  perror("stat()");

  return 0;
}
