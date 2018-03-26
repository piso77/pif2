#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>

/* command line tool to operate the fpga: status, erase, etc */
const char *version = (const char *)("test-spi build: " __DATE__ " - " __TIME__);

struct opt { const char *name; void (*func)(int, char *); };

#define handle_error(msg) \
  do { perror(msg); exit(EXIT_FAILURE); } while (0)

/* MachXO2 Programming Guide - sysCONFIG Programming Commands */

#define IDCODE_PUB              {0xe0, 0x00, 0x00, 0x00}
#define ISC_ENABLE              {0xc6, 0x08, 0x00, 0x00}
#define ISC_ERASE               {0x0e, 0x04, 0x00, 0x00}
#define ISC_PROGRAMDONE         {0x5e, 0x00, 0x00, 0x00}
#define LSC_INITADDRESS         {0x46, 0x00, 0x00, 0x00}
#define LSC_PROGINCRNV          {0x70, 0x00, 0x00, 0x01}
#define LSC_READ_STATUS         {0x3c, 0x00, 0x00, 0x00}
#define LSC_REFRESH             {0x79, 0x00, 0x00, 0x00}

/*
 * Max CCLK in Slave SPI mode according to 'MachXO2 Family Data
 * Sheet' sysCONFIG Port Timing Specifications (3-36)
 */
#define MACHXO2_MAX_SPEED       66000000

#define MACHXO2_LOW_DELAY       5       /* us */
#define MACHXO2_HIGH_DELAY      200     /* us */
#define MACHXO2_REFRESH         4800    /* us */
#define MACHXO2_MAX_BUSY_LOOP   32

#define MACHXO2_PAGE_SIZE       16

/* Status register bits and error codes */
#define BUSY    12
#define DONE    8
#define DVER    27
#define ENAB    9
#define ERRBIT0 23
#define ERRBIT1 24
#define ERRBIT2 25
#define ERRMASK 7
#define FAIL    13

#define ENOERR  0 /* no error */
#define EID     1
#define ECMD    2
#define ECRC    3
#define EPREAM  4 /* preamble error */
#define EABRT   5 /* abort error */
#define EOVERFL 6 /* overflow error */
#define ESDMEOF 7 /* SDM EOF */

#define RX_LEN 32

#define BITS_PER_LONG (sizeof(long) * 8)

#define BIT_WORD(nr)            ((nr) / BITS_PER_LONG)
static inline int test_bit(int nr, const volatile unsigned long *addr)
{
	return (*addr >> nr) & 0x01;
}

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
//! Byte swap unsigned short
uint16_t swap_uint16( uint16_t val ) 
{
	return (val << 8) | (val >> 8 );
}

//! Byte swap unsigned int
uint32_t swap_uint32( uint32_t val )
{
	val = ((val << 8) & 0xFF00FF00 ) | ((val >> 8) & 0xFF00FF ); 
	return (val << 16) | (val >> 16);
}

uint64_t swap_uint64( uint64_t val )
{
	val = ((val << 8) & 0xFF00FF00FF00FF00ULL ) | ((val >> 8) & 0x00FF00FF00FF00FFULL );
	val = ((val << 16) & 0xFFFF0000FFFF0000ULL ) | ((val >> 16) & 0x0000FFFF0000FFFFULL );
	return (val << 32) | (val >> 32);
}
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
// in BE we don't need to swap the values read
// from machxo2, so make these null
uint16_t swap_uint16( uint16_t val ) { return val; }
uint32_t swap_uint32( uint32_t val ) { return val; }
uint64_t swap_uint64( uint64_t val ) { return val; }
#else
#error __BYTE_ORDER__ undefined???
#endif

int spi_xfer(int fd, uint8_t *reg, size_t sreg, void *buf, size_t sbuf) {
	struct spi_ioc_transfer xfer[2];
	int ret, len = 1;

	memset(xfer, 0, sizeof xfer);

	xfer[0].tx_buf = (unsigned long)reg;
	xfer[0].len = sreg;
	if (buf) {
		memset(buf, 0, sbuf);
		xfer[1].rx_buf = (unsigned long)buf;
		xfer[1].len = sbuf;
		len = 2;
	}
	
	ret = ioctl(fd, SPI_IOC_MESSAGE(len), xfer);
	if (ret < 0)
		handle_error("ioctl() failed\n");
	return ret;
}

static inline uint8_t get_err(long unsigned int *status) {
        uint8_t errbit0, errbit1, errbit2;

        errbit0 = (*status >> ERRBIT0) & 0x01;
        errbit1 = (*status >> ERRBIT1) & 0x01;
        errbit2 = (*status >> ERRBIT2) & 0x01;
        return ((errbit2 << 2) | (errbit1 << 1) | errbit0) & ERRMASK;
}

static void parse_status(long unsigned int *status)
{
        char const *ferr;

        switch (get_err(status)) {
                case ENOERR: ferr = "No Error";         break;
                case EID: ferr = "ID ERR";              break;
                case ECMD: ferr = "CMD ERR";            break;
                case ECRC: ferr = "CRC ERR";            break;
                case EPREAM: ferr = "Preamble ERR";     break;
                case EABRT: ferr = "Abort ERR";         break;
                case EOVERFL: ferr = "Overflow ERR";    break;
                case ESDMEOF: ferr = "SDM EOF";         break;
                default: ferr = "0xDEADBEF0";
        }
        printf("machxo2 status: 0x%08lX - done=%d, cfgena=%d, busy=%d, fail=%d,"
               " devver=%d, err=%s\n",
                *status, test_bit(DONE, status), test_bit(ENAB, status),
                test_bit(BUSY, status), test_bit(FAIL, status),
                test_bit(DVER, status), ferr);
}


long unsigned int get_status(int fd) {
	long unsigned int buf = 0;
	uint8_t reg[4];

	// LSC_READ_STATUS
	reg[0] = 0x3c;
	reg[1] = 0x00;
	reg[2] = 0x00;
	reg[3] = 0x00;
	spi_xfer(fd, reg, sizeof(reg), &buf, sizeof(buf));
	buf = swap_uint32(buf);
	//printf("LSC_READ_STATUS: 0x%08lx\n", buf);
	return buf;
}

void dump_status(int fd, char *foobar) {
	long unsigned int status;

	status = get_status(fd);
	parse_status(&status);
}

static void get_info(int fd, char *foobar) {
	uint32_t buf;
	uint64_t lbuf;
	uint8_t reg[4];
	long unsigned int status;

#if 0
	// ISC_ENABLE_X
	reg[0] = 0x74;
	reg[1] = 0x08;
	reg[2] = 0x00;
	reg[3] = 0x00;
	spi_xfer(fd, reg, sizeof(reg), NULL, 0);
	sleep(1);
#endif

	buf = 0;
	// IDCODE_PUB
	reg[0] = 0xe0;
	reg[1] = 0x00;
	reg[2] = 0x00;
	reg[3] = 0x00;
	spi_xfer(fd, reg, sizeof(reg), &buf, sizeof(buf));
	printf("IDCODE_PUB: 0x%08x\n", swap_uint32(buf));

	status = get_status(fd);
	parse_status(&status);

	buf = 0;
	lbuf = 0;
	// USERCODE
	reg[0] = 0xc0;
	reg[1] = 0x00;
	reg[2] = 0x00;
	reg[3] = 0x00;
	spi_xfer(fd, reg, sizeof(reg), &buf, sizeof(buf));
	buf = swap_uint32(buf);
	printf("USERCODE: 0x%08x\n", buf);

	buf = 0;
	lbuf = 0;
	// LSC_READ_FEATURE
	reg[0] = 0xe7;
	reg[1] = 0x00;
	reg[2] = 0x00;
	reg[3] = 0x00;
	spi_xfer(fd, reg, sizeof(reg), &lbuf, sizeof(lbuf));
	lbuf = swap_uint64(lbuf);
	printf("LSC_READ_FEATURE: 0x%016llx\n", lbuf);

	buf = 0;
	lbuf = 0;
	// LSC_READ_FEABITS
	reg[0] = 0xfb;
	reg[1] = 0x00;
	reg[2] = 0x00;
	reg[3] = 0x00;
	spi_xfer(fd, reg, sizeof(reg), &buf, 2);
	buf = swap_uint32(buf);
	printf("LSC_READ_FEABITS: 0x%08x\n", buf);

	buf = 0;
	lbuf = 0;
	// LSC_READ_OTP
	reg[0] = 0xfa;
	reg[1] = 0x00;
	reg[2] = 0x00;
	reg[3] = 0x00;
	spi_xfer(fd, reg, sizeof(reg), &buf, 1);
	buf = swap_uint32(buf);
	printf("LSC_READ_OTP: 0x%08x\n", buf);

	buf = 0;
	lbuf = 0;
	// UIDCODE_PUB
	reg[0] = 0x19;
	reg[1] = 0x00;
	reg[2] = 0x00;
	reg[3] = 0x00;
	spi_xfer(fd, reg, sizeof(reg), &lbuf, sizeof(lbuf));
	lbuf = swap_uint64(lbuf);
	printf("UIDCODE_PUB: 0x%016llx\n", lbuf);

#if 0
	// ISC_DISABLE
	reg[0] = 0x26;
	reg[1] = 0x00;
	reg[2] = 0x00;
	spi_xfer(fd, reg, sizeof(reg)-1, NULL, 0);
	// ISC_NOOP
	reg[0] = 0xff;
	reg[1] = 0xff;
	reg[2] = 0xff;
	reg[3] = 0xff;
	spi_xfer(fd, reg, sizeof(reg), NULL, 0);
#endif
}

#define MAXLOOP 4096
void wait_busy(int fd) {
	int cnt = 0;
	long unsigned int status;

	do {
		if (cnt == MAXLOOP)
			handle_error("still BUSY, bailing out");
		cnt++;
		status = get_status(fd);
	} while (test_bit(BUSY, &status));
}

static void erase(int fd, char *foobar) {
	//char reg[4];
	uint8_t enable[] = ISC_ENABLE;
	uint8_t erase[] = ISC_ERASE;
	long unsigned int status;

	dump_status(fd, NULL);
	// ISC_ENABLE
	spi_xfer(fd, enable, sizeof(enable), NULL, 0);
	// delay 5us or read_busy and(???) LSC_READ_STATUS and check busy
	dump_status(fd, NULL);
	sleep(1);
	dump_status(fd, NULL);
	// ISC_ERASE
	spi_xfer(fd, erase, sizeof(erase), NULL, 0);
	dump_status(fd, NULL);
	// LSC_READ_STATUS and wait_not_busy()
	wait_busy(fd);
	// LSC_READ_STATUS and check fail
	status = get_status(fd);
	if (test_bit(FAIL, &status))
		handle_error("failed to erase flash");
}

static void load(int fd, char *bitstream) {
	//char reg[4];
	int bstream, len;
    uint8_t initaddr[] = LSC_INITADDRESS;
    #define LINE 16
    #define OP 4
    uint8_t buf[LINE+OP] = LSC_PROGINCRNV;

	printf("program(): %s\n", bitstream);
	bstream = open(bitstream, O_RDONLY);
	if (bstream < 0)
		handle_error("can't open bitstream file");

	// LSC_INITADDRESS
    spi_xfer(fd, initaddr, sizeof(initaddr), NULL, 0);
	// LSC_PROGINCRNV + 128bits
    do {
       len = read(bstream, &buf[OP], LINE);
        if (len == 0) // EOF?
            break;
        if (len < LINE) {
            close(bstream);
            handle_error("malformed bistream file");
        }
        spi_xfer(fd, buf, LINE+OP, NULL, 0);
        wait_busy(fd);
    } while(1);
    close(bstream);
}

#define MACHXO2_MAX_REFRESH_LOOP 1024
static void done(int fd, char *foobar) {
	//char reg[4];
	uint8_t pdone[] = ISC_PROGRAMDONE;
	uint8_t refresh[] = LSC_REFRESH;
    long unsigned int status, refreshloop = 0;

	// ISC_PROGRAMDONE
    spi_xfer(fd, pdone, sizeof(pdone), NULL, 0);
    sleep(1);
	dump_status(fd, NULL);
	// LSC_READ_STATUS and check done
    status = get_status(fd);
	if (!test_bit(DONE, &status))
		handle_error("done bit not set");
    do {
        // LSC_REFRESH
        spi_xfer(fd, refresh, sizeof(refresh), NULL, 0);
        // wait tRefresh
        sleep(5);
        // LSC_READ_STATUS and check success (and loop)
        status = get_status(fd);
        if (!test_bit(BUSY, &status) && test_bit(DONE, &status) &&
            get_err(&status) == ENOERR)
                break;
        if (++refreshloop == MACHXO2_MAX_REFRESH_LOOP)
            handle_error("refresh failed");
    } while (1);
    dump_status(fd, NULL);
}

static void spi_info(int file, char *foobar) {
    __u8    mode, lsb, bits;
    __u32 speed=2500000;

    ///////////////
        // Verifications
        ///////////////
        //possible modes: mode |= SPI_LOOP; mode |= SPI_CPHA; mode |= SPI_CPOL;
        //mode |= SPI_LSB_FIRST; mode |= SPI_CS_HIGH; mode |= SPI_3WIRE; mode |=
        //SPI_NO_CS; mode |= SPI_READY;
        //multiple possibilities using |
        /*
            if (ioctl(file, SPI_IOC_WR_MODE, &mode)<0) {
                perror("can't set spi mode");
                return;
                }
        */

            if (ioctl(file, SPI_IOC_RD_MODE, &mode) < 0)
                {
                perror("SPI rd_mode");
                return;
                }
            if (ioctl(file, SPI_IOC_RD_LSB_FIRST, &lsb) < 0)
                {
                perror("SPI rd_lsb_fist");
                return;
                }
        //sunxi supports only 8 bits
        /*
            if (ioctl(file, SPI_IOC_WR_BITS_PER_WORD, (__u8[1]){8})<0)
                {
                perror("can't set bits per word");
                return;
                }
        */
            if (ioctl(file, SPI_IOC_RD_BITS_PER_WORD, &bits) < 0)
                {
                perror("SPI bits_per_word");
                return;
                }
        /*
            if (ioctl(file, SPI_IOC_WR_MAX_SPEED_HZ, &speed)<0)
                {
                perror("can't set max speed hz");
                return;
                }
        */
            if (ioctl(file, SPI_IOC_RD_MAX_SPEED_HZ, &speed) < 0)
                {
                perror("SPI max_speed_hz");
                return;
                }

    printf("spi mode %d, %d bits %sper word, %d Hz max\n", mode, bits, lsb ? "(lsb first) " : "", speed);
}

struct opt opts[] = {
    { "info", get_info },
	{ "erase", erase },
    { "load", load },
    { "done", done },
    { "status", dump_status },
    { "spi", spi_info },
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
	int fd, speed = 10000;
	int optind = 0;

	if (argc < 3) {
                help();
		exit(EXIT_FAILURE);
	}

	fd = open(argv[1], O_RDWR);
	if (fd < 0)
		handle_error("can't open spidev");

	if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, & speed) < 0)
		handle_error("SPI_IOC_WR_MAX_SPEED_HZ");

	while (opts[optind].name) {
		if (strcmp(argv[2], opts[optind].name) == 0) {
			printf("found %s\n", opts[optind].name);
			if (argc == 4)
				opts[optind].func(fd, argv[3]);
			else
				opts[optind].func(fd, NULL);
			break;
		}
		optind++;
	}
	close(fd);
	exit(EXIT_SUCCESS);
}
