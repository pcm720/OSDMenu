#include <fcntl.h>
#include <ps2sdkapi.h>

// Returns >=0 if file exists
int checkFile(char *path) {
  int res = open(path, O_RDONLY);
  if (res < 0) {
    return -1;
  }
  close(res);
  return res;
}
