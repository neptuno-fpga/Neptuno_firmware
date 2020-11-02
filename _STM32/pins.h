#define SPI_FPGA 2
#define SPI_SD 1
#define PIN_TCK PB0
#define PIN_TDI PB1
#define PIN_TMS PB10
#define PIN_TDO PB11
#define PIN_LED PC13
#define PIN_nWAIT PA15
#define PIN_NSS PB12
#define PIN_CSSD PA4
#define EXTENSION "NP1"
#define SPLASH "       N  E  P  T  U  N  O"

void SPI_SELECTED (void)
{
  GPIOB->regs->BRR = (1U << 12);
}

void SPI_DESELECTED (void)
{
  GPIOB->regs->BSRR = (1U << 12);
}
