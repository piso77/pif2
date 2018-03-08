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
#ifdef __BIG_ENDIAN
#define BUSY    20
#define DONE    16
#define DVER    3
#define ENAB    17
#define ERRBIT0 15
#define ERRBIT1 0
#define ERRBIT2 1
#define FAIL    21
#else
#define BUSY    12
#define DONE    8
#define DVER    27
#define ENAB    9
#define ERRBIT0 23
#define ERRBIT1 24
#define ERRBIT2 25
#define FAIL    13
#endif
#define ERRMASK 7

#define ENOERR  0 /* no error */
#define EID     1
#define ECMD    2
#define ECRC    3
#define EPREAM  4 /* preamble error */
#define EABRT   5 /* abort error */
#define EOVERFL 6 /* overflow error */
#define ESDMEOF 7 /* SDM EOF */

#define RX_LEN 32

#ifdef CONFIG_64BIT
#define BITS_PER_LONG 64
#else
#define BITS_PER_LONG 32
#endif /* CONFIG_64BIT */

#define BIT_WORD(nr)            ((nr) / BITS_PER_LONG)
static inline int test_bit(int nr, const volatile unsigned long *addr)
{
    return 1UL & (addr[BIT_WORD(nr)] >> (nr & (BITS_PER_LONG-1)));
}

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

int spi_xfer(int fd, char *reg, size_t sreg, void *buf, size_t sbuf) {
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
	if (ret < 0) {
		perror("ioctl() failed\n");
		exit(EXIT_FAILURE);
	}
	return ret;
}

static inline uint8_t get_err(long unsigned int *status) {
        uint8_t errbit0, errbit1, errbit2;

        errbit0 = (*status >> ERRBIT0) & 0x01;
        errbit1 = (*status >> ERRBIT1) & 0x01;
        errbit2 = (*status >> ERRBIT2) & 0x01;
        return ((errbit2 << 2) | (errbit1 << 1) | errbit0) & ERRMASK;
}

static void print_status_reg(long unsigned int *status)
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


void dump_status(int fd) {
	long unsigned int buf = 0;
	char reg[4];

	// LSC_READ_STATUS
	reg[0] = 0x3c;
	reg[1] = 0x00;
	reg[2] = 0x00;
	reg[3] = 0x00;
	spi_xfer(fd, reg, sizeof(reg), &buf, sizeof(buf));
	buf = swap_uint32(buf);
	printf("LSC_READ_STATUS: 0x%08lx\n", buf);
	print_status_reg(&buf);
}

static void get_info(int fd) {
	uint32_t buf;
	uint64_t lbuf;
	char reg[4];

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

	dump_status(fd);

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

int main(int argc, char *argv[]) {
	int fd, speed = 10000;

	fd = open(argv[1], O_RDWR);
	if (fd < 0) {
		perror("can't open spidev");
		exit(EXIT_FAILURE);
	}


	if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, & speed) < 0) {
		perror("SPI_IOC_WR_MAX_SPEED_HZ");
		exit(EXIT_FAILURE);
	}


	get_info(fd);
	return EXIT_SUCCESS;
}
