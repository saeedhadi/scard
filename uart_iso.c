/*
 * uart_iso.c
 *
 *  Created on: 16 ���. 2015 �.
 *      Author: RLeonov
 */

#include "uart_iso.h"

ISO7816_SC* pScard;

/* Initially, UART speed is 10752.69 bit/s (f=4MHz, F=372, D=1)
 * "The etu initially used by the card shall be equal to 372 clock cycles (i.e., during the answer to reset, the values
 * of the transmission parameters are the default values Fd = 372 and Dd = 1)." Page 15.
 * 372 clock/symbol makes bitrate about 10753.  */
#define DEFAULT_BAUDRATE    10753
#define SET_BAUD(A)         regVal = LPC_SYSCON->UARTCLKDIV; \
Fdiv = (((38000000/LPC_SYSCON->SYSAHBCLKDIV)/regVal)/16)/A ; \
CARD_UART->DLM = Fdiv / 256; \
CARD_UART->DLL = Fdiv % 256;

static void card_gpio_init() {
//    gpio_enable_pin(CARD_PWR_PIN, GPIO_MODE_OUT);
    PinSetupOut(1, 22); // PIO0_22
    PWR_OFF();
//    gpio_enable_pin(CARD_RST_PIN, GPIO_MODE_OUT);
    PinSetupOut(1, 24);
    RST_LO();
}

// Enable Timer to provide 4 MHz clock source to the card
static void card_clock_init() {
    // To get the 3 MHz clock freq use PR = 3, MR3 = 3, MR1 = 2;
    // To get the 4 MHz clock freq use PR = 2, MR3 = 3, MR1 = 2;
    LPC_SYSCON->SYSAHBCLKCTRL |= (1 << 8); // Enable the CT16B1 timer peripherals
    CARD_CLK_TMR->TCR = 2; // Reset the timer counter and prepare to change settings
    CARD_CLK_TMR->PR = 2; // PreScaler value, at 48MHz, a PR value of 3 (PR + 1)! gives us a 24MHz clock.
    CARD_CLK_TMR->PWMC = 0x0002;    // Enable PWM mode for Match 1 output.
    CARD_CLK_TMR->MCR = (1 << 10); // Reset on MR3.
    CARD_CLK_TMR->MR3 = 2;     // Set the period, the timer will be reset to zero once it reaches this value from 0 to MR3
    CARD_CLK_TMR->MR1 = 1;     // Match Register 1 set to 2 counts, giving us 50% duty // Here output 4 MHz
    CARD_CLK_IO_CON &= ~0x07;
    CARD_CLK_IO_CON = (2 | (1 << 3)); // Port 0 pin 22
    CARD_CLK_TMR->TCR = 0;     // Take timer out of reset
    // Then the timer is started by executing macro CLK_ON() and swithced off bye the CLK_OFF() macro
}

static void card_data_init() {
    uint32_t Fdiv, regVal;
    NVIC_DisableIRQ(UART_IRQn);
    IO_RESET();
    IO_ENABLE();
    LPC_SYSCON->SYSAHBCLKCTRL |= (1<<12); // Enable Uart Clock
    LPC_SYSCON->UARTCLKDIV = 1;     // not divided
    CARD_UART->LCR = 0x9B;          // 8 bits, Even Parity, Parity Enable, 1 Stop bit
    SET_BAUD(DEFAULT_BAUDRATE);     // Setup baudrate
    CARD_UART->DLL = (uint8_t)(Fdiv & 0xFF); // setup divider
    CARD_UART->DLM = (uint8_t)(Fdiv >> 8); // setup divider
    CARD_UART->LCR &= ~0x80;      // DLAB = 0
    CARD_UART->FCR = 0x07;        // Enable and reset TX and RX FIFO
    CARD_UART->SCICTRL = 0x01;    // Enable Smartcard interface
    while (CARD_UART->LSR & LSR_RDR) {
      regVal = CARD_UART->RBR;    // Dump data from RX FIFO
    }
    CARD_UART->IER = IER_RBR;   // Enable UART interrupt
    NVIC_EnableIRQ(UART_IRQn); // Enable IRQ
}

void card_lld_init(ISO7816_SC* scard) {
    card_gpio_init();
    card_clock_init();
    card_data_init();
    pScard = scard;
    pScard->dLen = 0;
    pScard->TSreceived = false;
}

void UART_IRQHandler() {
    uint8_t IIRValue;
    uint8_t RxByte;
    IIRValue = LPC_USART->IIR;
    IIRValue >>= 1;           /* skip pending bit in IIR */
    IIRValue &= 0x07;         /* check bit 1~3, interrupt identification */
    if (IIRValue == IIR_RDA) /* Receive Data Available */
    {
        RxByte = LPC_USART->RBR;
        if(pScard->TSreceived) pScard->dBuf[pScard->dLen++] = RxByte;
        else {
            if((RxByte == 0x3B) || (RxByte == 0x3F)) { // Check TS
                pScard->dBuf[pScard->dLen++] = RxByte;
                pScard->TSreceived = !pScard->TSreceived;
                return;
            } // TS correct
        } // // Receive data
    } // Rx Irq
    return;
}
