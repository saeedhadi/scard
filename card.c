/*
 * card.c
 *
 *  Created on: 07 ���. 2015 �.
 *      Author: RLeonov
 */
#include "card.h"
#include "delay.h"

uint8_t CardBuf[256];
uint32_t Cnt;
bool Busy;
CardState_t State;

inline uint8_t ReadByte(uint8_t *AByte);

void Activation() {
    RST_LO();
    PWR_ON();
    CLK_ON();
}

void Deactivation() {
    RST_LO();
    CLK_OFF();
    // Uart disable
    PWR_OFF();
    State = CRD_Off;
}

void ColdReset() {
    UartSW_Printf("1\r");
    Deactivation();
    _delay_ms(4);
    UartSW_Printf("2\r");
    Activation();
    _delay_us(207);     // Wait more than 400 clock cycles
    UartSW_Printf("3\r");
    RST_HI();
}

void WarmReset() {
    RST_LO();
    _delay_us(207);     // Wait more than 400 clock cycles
    RST_HI();
}

void PwrRst_Init() {
    PinSetupOut(CARD_PWR_PORT, CARD_PWR_PIN);
    PWR_OFF();
    PinSetupOut(CARD_RST_PORT, CARD_RST_PIN);
    RST_LO();
}

void ClockInit() {
    // PR 3, MR3 3, MR1 2  = 3MHz
    LPC_SYSCON->SYSAHBCLKCTRL |= (1 << 8); // Enable the CT16B1 timer peripherals
    CARD_CLK_TMR->TCR = 2;
    CARD_CLK_TMR->PR = 3; // PreScaler value, at 48000000Hz, a PR value of 3 gives us a 12000000Hz clock
    CARD_CLK_TMR->PWMC = 0x0002;     // Enable PWM mode for Match 1.
    CARD_CLK_TMR->MCR = (1 << 10); // Reset on MR3
    CARD_CLK_TMR->MR3 = 3;     // Set the period, the timer will be reset to zero once it reaches this value
    CARD_CLK_TMR->MR1 = 2;     // Match Register 0 set to 50 counts, giving us 50% duty // Here output 4 MHz
    CARD_CLK_IO_CON &= ~0x07;
    CARD_CLK_IO_CON = (2 | (1 << 3)); // Port 0 pin 22
    CARD_CLK_TMR->TCR = 0;     // Take timer out of reset
}

void IO_Init() {
    NVIC_DisableIRQ(UART_IRQn);
    CARD_UART_IO_CON &= ~0x07;  // Clean
    CARD_UART_IO_CON |= 0x01;   // Port 0 Pin 19 is UART IO

    LPC_SYSCON->UARTCLKDIV = 1;             // divided
    CARD_UART->OSR = (uint32_t)(371 << 14); // Oversampling by 372
    CARD_UART->DLL = 0x01; // }
    CARD_UART->DLM = 0x00; // } pass the USART clock through without division
    CARD_UART->LCR = 0x03; // 8 bit
    CARD_UART->LCR |= (1 << 3); // Parity Enable
    CARD_UART->LCR |= (0x01 << 5); // Even Parity
    CARD_UART->SCICTRL = 0x01; // Enable SC mode
    LPC_SYSCON->SYSAHBCLKCTRL |= (1<<12);   // Enable Uart Clock
    CARD_UART->IER = CARD_UART_IR_RDA | ; // Enable Rx Irq
    NVIC_EnableIRQ(UART_IRQn);
    UartSW_Printf("Card Init\r");
}

void Card_Init() {
    UartSW_Printf("CRst\r");
    PwrRst_Init();
    IO_Init();
    ClockInit();
    Busy = false;
    // ColdReset
    ColdReset();
    // Get ATR
    uint8_t b;
    if(ReadByte(&b)) {
        UartSW_Printf("TS fail\r");
        return;
    }
}


// ========================== Low-level ========================================
inline uint8_t ReadByte(uint8_t *AByte) {
   for(uint16_t i=0; i<9999; i++) {
   }
    // Timeout, get out
    return 1;
}

void Card_IrqRx() {
    UartSW_Printf("i\r");
    uint8_t IIRValue = CARD_UART->IIR;
    IIRValue >>= 1;
    if (IIRValue == CARD_UART_RDA)
        CardBuf[Cnt++] = LPC_USART->RBR;
}

void Uart_IRQHandler(void) {
    Card_IrqRx();
}
