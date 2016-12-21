#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pifwrap.h"

/* command line tool to operate the fpga: status, erase, etc */

#define handle_error(msg) \
  do { perror(msg); exit(EXIT_FAILURE); } while (0)

pifHandle h;

struct opt { const char *name; void (*func)(void); };

static int mcp(pifHandle h) {
	uint8_t r = 0;

	pifMcpRead(h, 9, &r);
	return r;
}

static int INITn(pifHandle h) {
	int r = mcp(h);

	return (r >> 6) & 1;
}

static void cfgstatus(void) {
	uint32_t errcode, status=0;
	int init;
	char *fcstatus;

	pifGetStatusReg(h, &status);

	init = INITn(h);
	printf("*** status = %8x, INITn = %d", status, init);

	if (!init)
		handle_error("INITn failed");

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
}

static void erase(void) {
	printf("erase()\n");
    pifWaitUntilNotBusy(h, -1);
    pifDisableCfgInterface(h);
    //showCfgStatus(h);
    pifEnableCfgInterfaceOffline(h);
    //showCfgStatus(h);

    printf("erasing configuration memory..\n");
    pifEraseCfg(h);
    printf("erased..\n");

	pifProgDone(h);
	pifRefresh(h);
	pifDisableCfgInterface(h);
	//showCfgStatus(h);
	printf("erase done\n");
}

struct opt opts[] = {
	{ "erase", erase },
	{ "status", cfgstatus },
	{ NULL, NULL }
};

static void help() {
	handle_error("Help me Popeye!\n");
}

int main(int argc, char *argv[]) {
	int optind = 0;

	if (argc < 2)
		help();

	while (opts[optind].name) {
		if (strcmp(argv[1], opts[optind].name) == 0) {
			printf("found %s\n", opts[optind].name);
			h = pifInit();
			if (h)
				opts[optind].func();
			break;
		}
		optind++;
	}
	exit(EXIT_SUCCESS);
}
