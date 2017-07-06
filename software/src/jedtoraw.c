#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define handle_error(msg) \
  do { perror(msg); exit(EXIT_FAILURE); } while (0)

int main(int argc, char *argv[]) {
	char *line = NULL, *eptr;
	int i, num = 0;
	const int CFG_PAGE_SIZE = 16;
	size_t len = 0;
	ssize_t read;
	enum state {INITIAL, INDATA} machine = INITIAL;
	FILE *fd, *fdo;

	fd = fopen("/dev/stdin", "r");
	if (fd == NULL)
		handle_error("fopen /dev/stdin");
	fdo = fopen("/dev/stdout", "w");
	if (fdo == NULL)
		handle_error("fopen /dev/stdout");

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
		//pifProgCfgPage(h, frameData);
        fwrite(frameData, sizeof(uint8_t), CFG_PAGE_SIZE, fdo);
		//if ((num % 25)==0)
		//	printf(".");
	}
	//printf("\n");
	fclose(fd);
	fclose(fdo);
}
