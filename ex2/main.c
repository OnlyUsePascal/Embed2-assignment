//------------------------------------------- main.c CODE STARTS ---------------------------------------------------------------------------
#include <stdio.h>
#include "NUC100Series.h"
#include "LCD.h"


void System_Config(void);
void SPI2_Config(void);
void ADC7_Config(void);
void GPIO_Config(void);
void sendMessage(void);
void sendCharacter(unsigned char temp);

#define HXT_STATUS 1<<0
#define PLL_STATUS 1<<2
#define SIGNAL_LENGTH 4

char s[SIGNAL_LENGTH] = "G06";

int main(void) {
    System_Config();
    SPI2_Config();
    ADC7_Config();
    GPIO_Config();

	  PC->DOUT |= (0xF<<12); // Turn off all LED
    uint32_t adc7_val;
    
    ADC->ADCR |= (1 << 11); // start ADC channel 7 conversion
    while (1) {
        while (!(ADC->ADSR & (1 << 0))); // wait until conversion is completed (ADF=1)
        ADC->ADSR |= (1 << 0); // write 1 to clear ADF
        adc7_val = ADC->ADDR[7] & 0x0000FFFF; //extract ad info
        
        //vref = 3.3v, 12bit converter => res = 3.3/(2^12 - 1) = 0.806v
		    //=> 1 adc = 0.806v, so compare adc > 2 / 0.806 ~ 2482
				if (adc7_val > 2482) {
          PC->DOUT &= ~(0b11 << 12);
          sendMessage();
				} else {
          PC->DOUT |= (0b11 << 12);
				}

        // CLK_SysTickDelay(2000000);
        CLK_SysTickDelay(10);
    }
}

//------------------------------------------------------------------------------------------------------------------------------------
// FUNCTION ZONE
//------------------------------------------------------------------------------------------------------------------------------------
void sendMessage(void) {
  for (int i = 0; i < SIGNAL_LENGTH; i++) {
    sendCharacter(s[i]);
    CLK_SysTickDelay(5e5); // Delay 0.5s
  }
}

void sendCharacter(unsigned char temp) {
    SPI2->SSR |= 1 << 0;
    SPI2->TX[0] = temp;
    SPI2->CNTRL |= 1 << 0;
    while (SPI2->CNTRL & (1 << 0));
    SPI2->SSR &= ~(1 << 0);
}


//------------------------------------------------------------------------------------------------------------------------------------
// INITIALIZE ZONE
//------------------------------------------------------------------------------------------------------------------------------------
void System_Config(void) {
    SYS_UnlockReg(); // Unlock protected registers

    //---- PLL clock config
    CLK->PWRCON |= (1 << 0);
		while(!(CLK->CLKSTATUS & HXT_STATUS));
    CLK->PLLCON &= ~(1 << 19); //0: PLL input is HXT
    CLK->PLLCON &= ~(1 << 16); //PLL in normal mode
    CLK->PLLCON &= (~(0x01FF << 0));
    CLK->PLLCON |= 48;
    CLK->PLLCON &= ~(1 << 18); //0: enable PLLOUT
    while(!(CLK->CLKSTATUS & PLL_STATUS));
    //normal mode opertation
	  CLK->PWRCON &= ~(1u << 7);
    //clock source selection
    CLK->CLKSEL0 &= (~(7 << 0));
    CLK->CLKSEL0 |= (2 << 0);
    //clock frequency division
    CLK->CLKDIV &= (~0x0F << 0);
    //----

    SYS_LockReg();  // Lock protected registers    
}

void SPI2_Config(void) {
    SYS_UnlockReg(); // Unlock protected registers

    //SPI2 clock enable
    CLK->APBCLK |= 1 << 14;
    SYS->GPD_MFP |= (1 << 0); //1: PD0 is configured for SPI2 - SS
    SYS->GPD_MFP |= (1 << 1); //1: PD1 is configured for SPI2 - SPICLK
    SYS->GPD_MFP |= (1 << 3); //1: PD3 is configured for SPI2 - MOSI

    SPI2->CNTRL &= ~(1 << 23); //0: disable variable clock feature
    SPI2->CNTRL &= ~(1 << 22); //0: disable two bits transfer mode
    SPI2->CNTRL &= ~(1 << 18); //0: select Master mode
    SPI2->CNTRL &= ~(1 << 17); //0: disable SPI interrupt   

    SPI2->CNTRL |= 1 << 11; //1: SPI clock idle high 
    SPI2->CNTRL |= (1 << 10); //1: LSB is sent first   
    SPI2->CNTRL &= ~(3 << 8); //00: one transmit/receive word will be executed in one data transfer

    SPI2->CNTRL &= ~(31 << 3); //Transmit/Receive bit length
    SPI2->CNTRL |= 8 << 3;     //8: 8 bits transmitted/received per data transfer

    SPI2->CNTRL &= ~(1 << 2);  //0: Transmit at positive edge of SPI CLK     
    SPI2->DIVIDER = 24; // SPI clock divider. SPI clock = HCLK / ((DIVIDER+1)*2). HCLK = 50 MHz => SPICLK = 1MHz

    SYS_LockReg();  // Lock protected registers    
}

void ADC7_Config(void) {
  	SYS_UnlockReg(); // Unlock protected registers
    
    CLK->CLKSEL1 &= ~(0x03 << 2); // ADC clock source is 12 MHz
    CLK->CLKDIV &= ~(0x0FF << 16);
    CLK->CLKDIV |= (0x0B << 16); // ADC clock divider is (11+1) --> ADC clock is 12/12 = 1 MHz
    CLK->APBCLK |= (0x01 << 28); // enable ADC clock
    
    PA->PMD &= ~(0b11 << 14);
    PA->PMD |= (0b01 << 14); // PA.7 is input pin
    PA->OFFD |= (0x01 << 7); // PA.7 digital input path is disabled
    SYS->GPA_MFP |= (1 << 7); // GPA_MFP[7] = 1 for ADC7
    SYS->ALT_MFP &= ~(1 << 11); //ALT_MFP[11] = 0 for ADC7

    ADC->ADCR |= (0b11 << 2);  // continuous scan mode
    ADC->ADCR &= ~(1 << 1); // ADC interrupt is disabled
    ADC->ADCR |= (0x01 << 0); // ADC is enabled

    ADC->ADCHER &= ~(0b11 << 8); // ADC7 input source is external pin
    ADC->ADCHER |= (1 << 7); // ADC channel 7 is enabled.

    SYS_LockReg();  // Lock protected registers    
}

void GPIO_Config(void) {
  	SYS_UnlockReg(); // Unlock protected registers

    // PC 12 - LED5 
	 	PC->PMD &= ~(0x02 << 24);
    PC->PMD |= 0x01 << 24;
	  // PC 13 - LED6 
	  PC->PMD &= ~(0x02 << 26);
    PC->PMD |= 0x01 << 26;

    SYS_LockReg();  // Lock protected registers    
}

//------------------------------------------- main.c CODE ENDS ---------------------------------------------------------------------------


