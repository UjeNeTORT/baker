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
#include <sys/wait.h>
#include <unistd.h>

const char HELP_MSG[] =
  "--help         -- print this message\n\n"
  "--force        -- if destination directory not specified - create it\n\n"
  "-d [directory] -- destination dir to store backups\n"
  "--dst             -- alias to -d\n"
  "-s [directory] -- source dir files in which to backup\n"
  "--src             -- alias to -s\n\n";

const char BACKUP_FILE_POSTFIX[] = ".bak";
const char *const IGNORED_DIRS[] = {
  ".",
  "..",
};

struct Flags {
  uint32_t is_force;
  uint32_t has_src;
  uint32_t has_dst;
};

void pollBackup(const char * dst_dir, const char * src_dir);
int backup(const char * dst_dir, const char * src_dir);
int backupFile(const char *dst_dir, const char *src_dir, const char *filename);
void archive(const char * fpath);
void printHelp();

int isDirIgnored(const char *dir_name);
int isDirValid(const char * dir);
int createDir(const char *dst_dir);

int main(int argc, char *argv[]) {
  if (argc == 1) {
    fprintf(stderr, "ERROR: invalid program parameters\n");
    printHelp();

    return 1;
  }

  Flags flags = {0};
  char dst_dir[PATH_MAX] = "";
  char src_dir[PATH_MAX] = "";

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
        !strcmp(argv[i], "--dst")) {

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
    strcat(dst_dir, src_dir);
    strcat(dst_dir, BACKUP_FILE_POSTFIX);

    fprintf(stderr, "LOG: creating destination directory (--force used)\n"
                    "     %s\n", dst_dir);

    createDir(dst_dir);
  }

  if (flags.has_dst && flags.is_force)
    createDir(dst_dir);

  pollBackup(dst_dir, src_dir);

  return 0;
}

void pollBackup(const char *const dst_dir, const char *const src_dir) {
  assert(dst_dir && "dst_dir == NULL");
  assert(src_dir && "src_dir == NULL");

  while(1) {
    backup(dst_dir, src_dir);
    usleep(1'000'000);
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

  createDir(dst_dir);

  const char *file_format = NULL; // for error handling
  dirent *curr_dir_content = readdir(curr_dir);

  while (curr_dir_content != NULL) {
    switch (curr_dir_content->d_type) {
      case DT_REG:
      {
        // bak_name = file_name + '/' bak_postfix
        const char *file_name = curr_dir_content->d_name;

        backupFile(dst_dir, src_dir, file_name);
        break;
      }

      case DT_DIR:
      {
        char *dir_name = curr_dir_content->d_name;
        if (isDirIgnored(dir_name)) break;

        // dst_dir += sub_dir
        char next_dst_dir[PATH_MAX] = "";
        strcat(next_dst_dir, dst_dir); strcat(next_dst_dir, dir_name);

        // src_dir += sub_dir
        char next_src_dir[PATH_MAX] = "";
        strcat(next_src_dir, src_dir); strcat(next_src_dir, dir_name);

        strcat(next_dst_dir, "/"); strcat(next_src_dir, "/");

        backup(next_dst_dir, next_src_dir);
        break;
      }

      case DT_BLK:     file_format = "block device";
      case DT_CHR:     file_format = "character device";
      case DT_FIFO:    file_format = "named pipe (FIFO)";
      case DT_LNK:     file_format = "symbolic link";
      case DT_SOCK:    file_format = "UNIX domain socket";
      case DT_UNKNOWN: file_format = "unknown";

      default:
        fprintf(stderr, "ERROR: unsupported file format: %s\n",
                          file_format ? file_format : "unsupported dirent d_type");
        break;
    }

    curr_dir_content = readdir(curr_dir);
  }

  closedir(curr_dir);

  return 0;
}

int backupFile(const char *dst_dir, const char *src_dir, const char *filename) {
  assert(dst_dir); assert(src_dir);

  char src_path[PATH_MAX] = ""; strcat(src_path, src_dir);
  strcat(src_path, "/"); strcat (src_path, filename);

  char dst_path[PATH_MAX] = ""; strcat(dst_path, dst_dir);
  strcat(dst_path, "/"); strcat (dst_path, filename);
  strcat(dst_path, BACKUP_FILE_POSTFIX);

  char dst_compressed_path[PATH_MAX] = "";
  strcat(dst_compressed_path, dst_path);
  strcat(dst_compressed_path, ".gz");

  // check if modified
  struct stat src_attr = {};
  stat(src_path, &src_attr);

  struct stat dst_attr = {};
  stat(dst_compressed_path, &dst_attr);

  if ((uint64_t) dst_attr.st_mtim.tv_sec >= (uint64_t) src_attr.st_mtim.tv_sec) return 0;

  fprintf(stderr, "FILE MODIFIED: %s\n", src_path);

  // backup if modified
  int status = 0;
  if (fork() == 0) exit(execl("/bin/cp", "/bin/cp", src_path, dst_path, /*sentinel*/(char *)NULL));
  waitpid(-1, &status, 0);

  archive(dst_path);

  return 0;
}

void archive(const char * fpath) {
  assert(fpath);

  // fprintf(stderr, "ARCHIVING: %s\n", fpath);

  pid_t pid = fork();
  if (pid == 0) exit(execl("/bin/gzip", "/bin/gzip", "-v", fpath, /*sentinel*/ (char *)NULL));

  int status = 0;
  waitpid(pid, &status, 0);
}

void printHelp() { fprintf(stdout, HELP_MSG); }

int isDirIgnored(const char *dir_name) {
  assert(dir_name);

  for (uint32_t i = 0; i < sizeof(IGNORED_DIRS) / sizeof(*IGNORED_DIRS); i++)
    if (!strcmp(dir_name, IGNORED_DIRS[i])) return 1;

  return 0;
}

int isDirValid(const char * dir) {
  if (!dir) return 0;

  struct stat dir_stat = {};
  if (!stat(dir, &dir_stat)) return S_ISDIR(dir_stat.st_mode);

  return 0;
}

int createDir(const char *dst_dir) {
  assert(dst_dir);

    if (!isDirValid(dst_dir)) { // directory does not exist
      if (!fork()) exit(execl("/bin/mkdir", "/bin/mkdir", dst_dir, /*sentinel*/(char *)NULL));

      int status = 0;
      while(wait(&status) > 0) ;
    }

    return 0;
}
