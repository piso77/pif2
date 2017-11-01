//---------------------------------------------------------------------
// piffind.cpp

using namespace std;

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "lowlevel.h"
#include "bcm2835.h"

#define XO2_I2C_CLOCK_SPEED     (400 * 1000)

#define MCP_FPGA_TDO            (1 << 0)
#define MCP_FPGA_TDI            (1 << 1)
#define MCP_FPGA_TCK            (1 << 2)
#define MCP_FPGA_TMS            (1 << 3)
#define MCP_FPGA_JTAGENn        (1 << 4)  // JTAG enable when Lo
#define MCP_FPGA_PROGn          (1 << 5)
#define MCP_FPGA_INITn          (1 << 6)
#define MCP_FPGA_DONE           (1 << 7)

int Fi2cSlaveAddr;
int FlastResult;

static void _setI2Caddr(int AslaveAddr) {
  if (Fi2cSlaveAddr != AslaveAddr)
    bcm2835_i2c_setSlaveAddress(AslaveAddr);
  Fi2cSlaveAddr = AslaveAddr;
}

static bool i2cWrite(int AslaveAddr, const uint8_t *pWrData, size_t AwrLen) {
  _setI2Caddr(AslaveAddr);
  FlastResult = bcm2835_i2c_write((char *)pWrData, AwrLen);
  return (FlastResult==BCM2835_I2C_REASON_OK);
}

//---------------------------------------------------------------------
int main() {
  int res = bcm2835_init();

  // h = pifInit();
  if (res == 1) {
	printf("init cont\n");
    // i2c initialise
    //bcm2835_set_debug(10);
    bcm2835_i2c_begin();
    bcm2835_i2c_set_baudrate(XO2_I2C_CLOCK_SPEED);

    // MCP23008 bits
    _setI2Caddr(MCP23008_ADDR);
    TllWrBuf oBuf;
    oBuf.clear().byte(6).byte(0xff);                            // all pullups
    i2cWrite(MCP23008_ADDR, oBuf.data(), oBuf.length());
    oBuf.clear().byte(9).byte(0xf7);                            // output reg
    i2cWrite(MCP23008_ADDR, oBuf.data(), oBuf.length());
    oBuf.clear().byte(0).byte(0xe1);                            // set inputs
    i2cWrite(MCP23008_ADDR, oBuf.data(), oBuf.length());

    _setI2Caddr(I2C_APP_ADDR);

    // spi initialise
    bcm2835_spi_begin();
    bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);      // default
    bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);                   // default
//  bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_65536); // default
    bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_32);    // 8MHz
    bcm2835_spi_chipSelect(BCM2835_SPI_CS0);                      // default
    bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);      // default
  }

  // pifClose(h);
  if (res == 1) {
	printf("init done\n");
    bcm2835_spi_end();
    bcm2835_i2c_end();
    bcm2835_close();
  }
}
