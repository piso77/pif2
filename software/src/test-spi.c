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

#define RX_LEN 32

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

void print_status_reg(unsigned int status) {
	unsigned int errcode;
	char *fcstatus;

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
        printf("status: Done=%d, CfgEna=%d, Busy=%d, Fail=%d, FlashCheck=%s\n",
                ((status >>  8) & 1),
                ((status >>  9) & 1),
                ((status >> 12) & 1),
                ((status >> 13) & 1), fcstatus);
}

int main(int argc, char *argv[]) {
	uint32_t buf;
	uint64_t lbuf;
	char reg[4];
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

	buf = 0;
	lbuf = 0;
	// LSC_READ_STATUS
	reg[0] = 0x3c;
	reg[1] = 0x00;
	reg[2] = 0x00;
	reg[3] = 0x00;
	spi_xfer(fd, reg, sizeof(reg), &buf, sizeof(buf));
	buf = swap_uint32(buf);
	printf("LSC_READ_STATUS: 0x%08x\n", buf);
	print_status_reg(buf);

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

	return EXIT_SUCCESS;
}
