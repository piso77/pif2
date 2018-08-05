#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "pifwrap.h"

/* command line tool to operate the fpga: status, erase, etc */
const char *version = (const char *)("pifcli build: " __DATE__ " - " __TIME__);

#define handle_error(msg) \
  do { perror(msg); exit(EXIT_FAILURE); } while (0)

struct opt { const char *name; void (*func)(pifHandle); };

static int mcp(pifHandle h) {
	uint8_t r = 0;

	pifMcpRead(h, 9, &r);
	return r;
}

static int INITn(pifHandle h) {
	int r = mcp(h);

	return (r >> 6) & 1;
}

static bool showDeviceID(pifHandle h) {
	uint32_t v = 0x12345678;
	bool res;

	res = pifGetDeviceIdCode(h, &v);
	printf("result=%d, ID code=%08x\n", res, v);
	return res;
}

static bool showTraceID(pifHandle h) {
	uint8_t buff[8] = {1,2,3,4,5,6,7,8};
	bool res = pifGetTraceId(h, buff);
	unsigned int i;

	printf("result=%d, Trace ID code= ", res);
	for (i=0; i<sizeof(buff); i++) {
		printf("%02x", buff[i]);
		switch(i) {
			case 3 : printf("_");  break;
			case 7 : printf("\n"); break;
			default: printf(".");  break;
		}
	}
	return res;
}

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0') 

static void enablejtag(pifHandle h) {
	uint64_t frow;
	uint16_t fbits;

	pifEnableCfgInterfaceTransparent(h);
	pifGetFeatureBits(h, &fbits);
	pifGetFeatureRow(h, &frow);
	fbits &= 0xfeff;
	pifDisableCfgInterface(h);

	pifWaitUntilNotBusy(h, -1);
	//cfgstatus(h);
	pifEnableCfgInterfaceOffline(h);
	//cfgstatus(h);

	printf("erasing all...\n");
	//pifEraseFeature(h);
	printf("done...\n");
	//pifSetFeatureRow(h, frow);
	// delay 200us
	//pifGetFeatureRow(h, &frow);
	//pifSetFeatureBits(h, fbits);
	// delay 200us
	//pifGetFeatureBits(h, &fbits);

	pifProgDone(h);
	// delay 200us
	pifRefresh(h);
	pifDisableCfgInterface(h);

}

static void cfgstatus(pifHandle h) {
	uint32_t errcode, status=0;
	int init, mcp23008;
	char *fcstatus;
	uint16_t fbits;
	uint64_t frow;
	uint8_t fb0, fb1;

	pifGetStatusReg(h, &status);
	pifEnableCfgInterfaceTransparent(h);
	pifGetFeatureBits(h, &fbits);
	pifGetFeatureRow(h, &frow);
	pifDisableCfgInterface(h);
	init = INITn(h);
	mcp23008 = mcp(h);
	printf("*** status = %8x, fbits = %2x frow = %16llx INITn = %d", status, fbits, frow, init);

	errcode = (status >> 23) & 7;
	switch (errcode) {
		case 0: fcstatus = "No Error";      break;
		case 1: fcstatus = "ID ERR";        break;
		case 2: fcstatus = "CMD ERR";       break;
		case 3: fcstatus = "CRC ERR";       break;
		case 4: fcstatus = "Preamble ERR";  break;
		case 5: fcstatus = "Abort ERR";     break;
		case 6: fcstatus = "Overflow ERR";  break;
		case 7: fcstatus = "SDM EOF";       break;
	}
	printf("  Done=%d, CfgEna=%d, Busy=%d, Fail=%d, FlashCheck=%s\n",
		((status >>  8) & 1),
		((status >>  9) & 1),
		((status >> 12) & 1),
		((status >> 13) & 1), fcstatus);
	printf("mcp23008: "BYTE_TO_BINARY_PATTERN"\n", BYTE_TO_BINARY(mcp23008));
	fb0 = ((uint8_t *)&fbits)[0];
	fb1 = ((uint8_t *)&fbits)[1];
	printf("fbits: "BYTE_TO_BINARY_PATTERN"|"BYTE_TO_BINARY_PATTERN"\n", BYTE_TO_BINARY(fb1), BYTE_TO_BINARY(fb0));
}

static void erase(pifHandle h) {
	printf("erase()\n");
    pifWaitUntilNotBusy(h, -1);
    pifDisableCfgInterface(h);
    //cfgstatus(h);
    pifEnableCfgInterfaceOffline(h);
    //cfgstatus(h);

    printf("erasing configuration memory..\n");
    pifEraseCfg(h);
    printf("erased..\n");

	pifProgDone(h);
	pifRefresh(h);
	pifDisableCfgInterface(h);
	//cfgstatus(h);
	printf("erase done\n");
}

static void configureXO2(pifHandle h, FILE *fd) {
  char *line = NULL, *eptr;
  int i, num = 0;
  const int CFG_PAGE_SIZE = 16;
  size_t len = 0;
  ssize_t read;
  enum state {INITIAL, INDATA} machine = INITIAL;

  printf("\n----------------------------\n");

  pifWaitUntilNotBusy(h, -1);

  pifDisableCfgInterface(h);
  //cfgstatus(h);
  pifEnableCfgInterfaceOffline(h);

  //cfgstatus(h);
  printf("erasing configuration memory..\n");
  pifEraseCfg(h);
  printf("erased..\n");

  pifInitCfgAddr(h);
  //cfgstatus(h);
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
    for (i=0; i < CFG_PAGE_SIZE; i++) {
      tmp = strndup(&line[i*8], 8);
      c = (uint8_t)strtol(tmp, &eptr, 2);
      //printf("tmp: %s hex: 0x%02x\n", tmp, c);
      frameData[i] = c;
      free(tmp);
    }
    pifProgCfgPage(h, frameData);
    if ((num % 25)==0)
      printf(".");
  }
  printf("\n");

  //cfgstatus(h);

  printf("programmed. transferring..\n");
  pifProgDone(h);
  pifRefresh(h);

  pifDisableCfgInterface(h);
  //cfgstatus(h);
  printf("configuration done\n");
}

static void load(pifHandle h) {
	FILE *fd;

	fd = fopen("/dev/stdin", "r");
	if (fd == NULL)
		handle_error("fopen");

	printf("\n================== loader =========================\n");

	showDeviceID(h);
	showTraceID(h);
	//  showUsercode(h);
	configureXO2(h, fd);
	fclose(fd);
}

struct opt opts[] = {
	{ "erase", erase },
	{ "load", load },
	{ "status", cfgstatus },
	{ "jtag", enablejtag },
	{ NULL, NULL }
};

static void help() {
	int optind = 0;
	printf("%s\n\n", version);

	printf("Available functions: \n");
	while(opts[optind].name) {
		printf("\t - %s\n", opts[optind].name);
		optind++;
	}
}

int main(int argc, char *argv[]) {
	int optind = 0;
	pifHandle h;

	if (argc < 2)
		help();

	while (opts[optind].name) {
		if (strcmp(argv[1], opts[optind].name) == 0) {
			printf("found %s\n", opts[optind].name);
			h = pifInit();
			if (h) {
				opts[optind].func(h);
			}
			break;
		}
		optind++;
	}
	exit(EXIT_SUCCESS);
}
