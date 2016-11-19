using namespace std;

#include <string>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "pifwrap.h"

#define CFG_PAGE_SIZE           16
#define UFM_PAGE_SIZE           16

static const int MICROSEC = 1000;              // nanosecs
static const int MILLISEC = 1000 * MICROSEC;   // nanosecs

static bool showDeviceID(pifHandle h) {
  uint32_t v = 0x12345678;
  bool res = pifGetDeviceIdCode(h, &v);
  printf("result=%d, ID code=%08x\n", res, v);
  return res;
}

static bool showTraceID(pifHandle h) {
  uint8_t buff[8] = {1,2,3,4,5,6,7,8};
  bool res = pifGetTraceId(h, buff);
  printf("result=%d, Trace ID code= ", res);
  for (unsigned i=0; i<sizeof(buff); i++) {
    printf("%02x", buff[i]);
    switch(i) {
      case 3 : printf("_");  break;
      case 7 : printf("\n"); break;
      default: printf(".");  break;
      }
    }
  return res;
}

static int mcp(pifHandle h) {
  uint8_t r = 0;
  pifMcpRead(h, 9, &r);
  return r;
  }

static int INITn(pifHandle h) {
  int r = mcp(h);
  return (r >> 6) & 1;
  }

static void showCfgStatus(pifHandle h) {
  uint32_t status=0;
  pifGetStatusReg(h, &status);

  int init = INITn(h);
  printf("*** status = %8x, INITn = %d", status, init);

  string fcStatus;
  uint32_t errCode = (status >> 23) & 7;
  switch (errCode) {
    case 0: fcStatus = "No Error";      break;
    case 1: fcStatus = "ID ERR";        break;
    case 2: fcStatus = "CMD ERR";       break;
    case 3: fcStatus = "CRC ERR";       break;
    case 4: fcStatus = "Preamble ERR";  break;
    case 5: fcStatus = "Abort ERR";     break;
    case 6: fcStatus = "Overflow ERR";  break;
    case 7: fcStatus = "SDM EOF";       break;
    }
  printf("  Done=%d, CfgEna=%d, Busy=%d, Fail=%d, FlashCheck=%s\n",
    ((status >>  8) & 1),
    ((status >>  9) & 1),
    ((status >> 12) & 1),
    ((status >> 13) & 1), fcStatus.c_str());
}


enum state {INITIAL, INDATA};

static void configureXO2(pifHandle h, FILE *fd) {
  char *line = NULL, *eptr;
  int num = 0;
  size_t len = 0;
  ssize_t read;
  enum state machine = INITIAL;

  printf("\n----------------------------\n");

  pifWaitUntilNotBusy(h, -1);

  pifDisableCfgInterface(h);
  showCfgStatus(h);
  pifEnableCfgInterfaceOffline(h);

  showCfgStatus(h);
  printf("erasing configuration memory..\n");
  pifEraseCfg(h);
  printf("erased..\n");

  pifInitCfgAddr(h);
  showCfgStatus(h);
  printf("programming configuration memory..\n"); // up to 2.2 secs in a -7000

  while ((read = getline(&line, &len, fd)) != -1) {
    uint8_t frameData[CFG_PAGE_SIZE];
    uint8_t c;
    char *tmp;
    //printf("Retrieved line[%d] of length: %zu\n", num, read);
    num++;
    //printf("%s", line);
    if (line[0] != '0' && line [0] != '1') {
      if (machine == INITIAL)
        continue;
      if (machine == INDATA)
        break;
    }
    machine = INDATA;
    for (int i=0; i < CFG_PAGE_SIZE; i++) {
      tmp = strndup(&line[i*8], 8);
      c = (uint8_t)strtol(tmp, &eptr, 2);
      printf("tmp: %s hex: 0x%02x\n", tmp, c);
      frameData[i] = c;
      free(tmp);
    }
    pifProgCfgPage(h, frameData);
    if ((num % 25)==0)
      printf(".");
  }
  printf("\n");

  showCfgStatus(h);

  printf("programmed. transferring..\n");
  pifProgDone(h);
  pifRefresh(h);

  pifDisableCfgInterface(h);
  showCfgStatus(h);
  printf("configuration done\n");
}

#define handle_error(msg) \
  do { perror(msg); exit(EXIT_FAILURE); } while (0)

int main(int argc, char *argv[]) {
  FILE *fd;

  if (argc < 2) {
    fprintf(stderr, "%s file\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  fd = fopen(argv[1], "r");
  if (fd == NULL)
    handle_error("fopen");

  printf("\n================== loader =========================\n");
  char buff[200];
  pifVersion(buff, sizeof(buff));
  printf("%s\n", buff);

  pifHandle h = NULL;
  h = pifInit();
  if (h) {
    showDeviceID(h);
    showTraceID(h);
    //  showUsercode(h);
    configureXO2(h, fd);
    pifClose(h);
  }

  printf("==================== bye ==========================\n");
  return 0;
}
