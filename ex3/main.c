////------------------------------------------- main.c CODE STARTS ---------------------------------------------------------------------------
#include <stdio.h>
#include "NUC100Series.h"
#include "LCD.h"

#define HXT_STATUS 1<<0
#define PLL_STATUS 1<<2
#define LXT_STATUS 1 << 1  	// 32.768 kHz
#define LIRC_STATUS 1 << 3 	// 10 kHz
#define HIRC_STATUS 1 << 4 	// 22.1184 MHz
#define LED_SWEEP_RATE 50000 // ~120 HZ

// Key
#define C3_pressed (!(PA->PIN & (1<<0)))		
#define C2_pressed (!(PA->PIN & (1<<1)))
#define C1_pressed (!(PA->PIN & (1<<2)))
// System
void System_Config(void);

// UART
void UART0_Config(void);
void UART02_IRQHandler(void);


// BUZZER
void buzzer_config(void);
void buzzer_beep(int beep_time);


// LEDs
void led_config(void);
void LED_flash(void);

// 7-segment LEDs
void seven_seg_config(void);
void display_LED(int num, int digit);
void timer3_config(int count_value); // Timer for multiplexing
void TMR3_IRQHandler(void);


// LCD
void SPI3_Config(void);
void LCD_start(void);
void LCD_command(unsigned char temp);
void LCD_data(unsigned char temp);
void LCD_clear(void);
void LCD_SetAddress(uint8_t PageAddr, uint8_t ColumnAddr);

// External Interrupt
void EXINT1_config(void);
void EXINT1_debounce_config(void);
void EINT1_IRQHandler(void);


// KEY MATRIX
void key_config(void);
void GPAB_IRQHandler(void);
void key_debounce_config(void);

// Gampeplay
void update_map(char map[][9]);
void reload(char map[][9]);
 
 

	
//--------------GLOBAL VARIABLE----------------------
// Load map
volatile	char map[8][9]; // store map
volatile	int  index = 0, row =0; // to access map (2D array)
volatile	int  finish_load_map = 0; // raise if map successfully loaded
volatile	int  map_loading = 0; // enable/disable map-loading function

// 7-segment LEDs
volatile int digit[4] = {0,0,0,0}; // store values shown in 7-segment LEDs
volatile int current_digit = 0; // for multiplexing

// External Interrupt 
volatile int EINT1_flag = 0; // flag raise if GP15 is triggered

// KEY MATRIX
volatile int input_location = 0; // value from keyboard
volatile int change_axis = 0;    // switch from set x to set y or vice versa
volatile int key_pressed = 0;		 //  flag raise if a key is triggered => only capture input value 1 time after a key is trigger

// FSM states
typedef enum { welcome_screen, map_ready, get_x_location, get_y_location, shoot, game_over, reload_map } STATES;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ----------------------------------------------------------------START----------------------------------------------------------------//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main(void)
{
// System
	System_Config();
	
// UART
	UART0_Config();


// LCD
	SPI3_Config();
	LCD_start();
	LCD_clear();

// BUZZER
	buzzer_config();	
	
// LEDs
	led_config();
	
	
// 7 Segments LED
	seven_seg_config();
	timer3_config(LED_SWEEP_RATE); // For Multiplexing

// External Interrupt
	EXINT1_config();
	EXINT1_debounce_config();	

// FSM
	STATES game_state = welcome_screen;
// XY location		
 int x_location = 0, y_location = 0;

// total number of accurate shot => 10 shots = 5 ships => win game 
 int hit = 0;
 
// total number shot => >16 shots => lose game
 int no_shoot = 0;
 
// enable/disble buzzer
 int buzzer_on = 0;
 
// Key
	key_config();
	key_debounce_config();
//Turn all Rows to LOW 
	PA->DOUT &= ~(1<<3);
	PA->DOUT &= ~(1<<4);
	PA->DOUT &= ~(1<<5);
	

	while (1) {
	
		switch (game_state) {
		case welcome_screen:
			// print welcome message
				LCD_clear();
				printS(30,5, "BATTLESHIP");
				printS_5x7(5,25, "GP15: Shoot");
				printS_5x7(5,35, "Key 0-8: xy coordinates");
				printS_5x7(5,45, "Key 9: Change axis");
				printS_5x7(5,55, "Please load map!!!!");
			// reset all game stats
				hit = 0;
				no_shoot = 0;
				x_location = 0;
				y_location = 0;
				buzzer_on = 0;
			// Allow user load map
				map_loading = 1;
				CLK_SysTickDelay(8000);
			// if map-loading process is finish => move on
				if(finish_load_map == 1){
					game_state = map_ready;
					finish_load_map = 0; // clear flag
					map_loading = 0;		 // disable map-loading function => no map modification unless reset
				}
			break;

		case map_ready:
			// Print message
				LCD_clear();
				printS(30,5, "BATTLESHIP");
				printS_5x7(5,25, "MAP LOADED SUCCESSFULLY");
				printS_5x7(5,55, "Press GP15 to start!!!!");
				CLK_SysTickDelay(8000);
			// Press GP15 => start game
				if(EINT1_flag == 1){
					EINT1_flag = 0;
					game_state = get_x_location;
				}
			break;

		case get_x_location:
				LCD_clear();
			// Get x coordinate
				if(key_pressed){
					key_pressed = 0;
					x_location = input_location;
				}
			// constantly update map
				update_map(map);
			// if press 9 => change to y
				if(change_axis == 1){
					change_axis = 0;
					game_state = get_y_location;
				}
			// Press GP15 => shoot
				if(EINT1_flag == 1){
					EINT1_flag = 0;
					game_state = shoot;
				}

				CLK_SysTickDelay(8000);

			break;
		case get_y_location:
				LCD_clear();
			// Get y coordinate
				if(key_pressed){
					key_pressed = 0;
					y_location = input_location;
				}
			// constantly update map
				update_map(map);
				if(change_axis == 1){
					change_axis = 0;
					game_state = get_x_location;
				}
			// Press GP15 => shoot
				if(EINT1_flag == 1){
					EINT1_flag = 0;
					game_state = shoot;
				}
			CLK_SysTickDelay(8000);
			break;

		case shoot:
			// compare map and input location
			// if the location is 1 => mark it as 'X' => LCD display 'X' and ignore hit twice later on
				if(map[y_location-1][x_location-1] == '1'){ 
					map[y_location-1][x_location-1] = 'X';
					hit++; // increase NO of accurate shot
					LCD_clear();
					update_map(map); // update map
					LED_flash();     // LEDs flash 3 times with duration of 0.5s
				}
				no_shoot++;        // update NO of  shot
				// total number shot > 16 shots => lose game
				if(no_shoot > 16 && hit < 10){
					game_state = game_over;	
					buzzer_on = 1;
				}
				// total number of accurate shot = 10 shots = 5 ships => win game 
				else if(hit >= 10){
					game_state = game_over;	
					buzzer_on = 1;
				}
				else{
					game_state = get_x_location; // go back to get x 
				
				}

			break;
		case game_over:
				LCD_clear();
			// print message display the result (win/lose)
				if(no_shoot > 16 && hit < 10){
					printS(30,5, "LOSERRRRRR");		
					printS_5x7(10,50, "Press GP15 to restart");	
				}
				else if(hit >= 10){
					printS(30,5, "WINNERRRRR");		
					printS_5x7(10,50, "Press GP15 to restart");		
				}
			// buzzer beeps 5 times
				if(buzzer_on){
					buzzer_beep(5);
					buzzer_on = 0;
				}
			// Press GP15 => restart 
				if(EINT1_flag == 1){
					EINT1_flag = 0;
					game_state = reload_map;
				}				
			CLK_SysTickDelay(8000);
				
			break;
			case reload_map:
				// reset all game stats
				hit = 0;
				no_shoot = 0;
				x_location = 0;
				y_location = 0;
				buzzer_on = 0;
				// Reload map 
				reload(map);
				game_state = map_ready;
				break;
		default: break;
		
		}
		
		// Assign values shown on 7-segment LEDs
		digit[0] = x_location;
		digit[1] = y_location;
		digit[2] = no_shoot/10;
		digit[3] = no_shoot%10;

		
	}
	
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ----------------------------------------------------------------END------------------------------------------------------------------//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// -------------------------------------------------------SYSTEM CONFIGURATION----------------------------------------------------------//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void System_Config(void) {
	SYS_UnlockReg(); // Unlock protected registers

	//////////Enabling Clock Sources//////////////
	CLK->PWRCON |= (1 << 0); // 12MHZ
  while(!(CLK->CLKSTATUS & HXT_STATUS));
	CLK->PWRCON |= 1 << 1; // 32.768 kHz
	while (!(CLK->CLKSTATUS & LXT_STATUS));
	CLK->PWRCON |= (1 << 2);// 22.1184 MHz
	while (!(CLK->CLKSTATUS & HIRC_STATUS));	
	CLK->PWRCON |= 1 << 3; // 10 kHz
	while (!(CLK->CLKSTATUS & LIRC_STATUS));
	//PLL configuration starts
	CLK->PLLCON &= ~(1 << 19); //0: PLL input is HXT
	CLK->PLLCON &= ~(1 << 16); //PLL in normal mode
	CLK->PLLCON &= (~(0x01FF << 0));
	CLK->PLLCON |= 48;
	CLK->PLLCON &= ~(1 << 18); //0: enable PLLOUT
	while (!(CLK->CLKSTATUS & PLL_STATUS));
	//PLL configuration ends
	// Fin = 12MHz => Fout = 50MHz

	//clock source selection
	CLK->CLKSEL0 &= (~(0x07 << 0));
	CLK->CLKSEL0 |= (0x02 << 0); // Select PLL out
	//clock frequency division
	CLK->CLKDIV &= (~0x0F << 0);

	SYS_LockReg();  // Lock protected registers    
}







//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// --------------------------------------------------------------UART-------------------------------------------------------------------//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UART0_Config(void) {
	SYS_UnlockReg(); // Unlock protected registers
	//UART0 Clock selection and configuration
	CLK->CLKSEL1 |= (0b11 << 24); // UART0 clock source is 22.1184 MHz
	CLK->CLKDIV &= ~(0xF << 8); // clock divider is 1
	CLK->APBCLK |= (1 << 16); // enable UART0 clock
	
	// UART0 pin configuration. PB.1 pin is for UART0 TX
	PB->PMD &= ~(0b11 << 2);
	PB->PMD |= (0b01 << 2); // PB.1 is output pin
	SYS->GPB_MFP |= (1 << 1); // GPB_MFP[1] = 1 -> PB.1 is UART0 TX pin
	
	SYS->GPB_MFP |= (1 << 0); // GPB_MFP[0] = 1 -> PB.0 is UART0 RX pin	
	PB->PMD &= ~(0b11 << 0);	// Set Pin Mode for GPB.0(RX - Input)

	// UART0 operation configuration
	UART0->LCR |= (0b11 << 0); // 8 data bit
	UART0->LCR &= ~(1 << 2); // one stop bit	
	UART0->LCR &= ~(1 << 3); // no parity bit
	UART0->FCR |= (1 << 1); // clear RX FIFO
	UART0->FCR |= (1 << 2); // clear TX FIFO
	UART0->FCR &= ~(0xF << 16); // FIFO Trigger Level is 1 byte]
	
	//Baud rate config: BRD/A = 1, DIV_X_EN=0
	//--> Mode 0, Baud rate = 19200
	UART0->BAUD &= ~(0b11 << 28); // mode 0	
	UART0->BAUD &= ~(0xFFFF << 0);
	UART0->BAUD |= 70;
	
	// Interrupt
	// Low-level
	UART0->IER |= (0x1 << 0);
	
	// System level
	NVIC->ISER[0] |= 1 << 12; // Enable UART0 interrupt 
	NVIC->IP[3] &= (~(3 << 6)); // Set priority
	SYS_LockReg();  // Lock protected registers 
}


void UART02_IRQHandler(void){
	if(map_loading)
		{
			char ReceivedByte;
			ReceivedByte = (char)(UART0->DATA);	
			
			if(ReceivedByte != '\n'&& ReceivedByte !='\r'&&ReceivedByte !=' '){
			map[row][index] = ReceivedByte;
			index++;
			}
			if(index == 8){
			map[row][index] = '\0';
			index = 0;		
			row++;
			}
			if(row == 8){
			row = 0;
			finish_load_map = 1;
			}
		}

}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// --------------------------------------------------------------LCD--------------------------------------------------------------------//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void LCD_start(void)
{
	LCD_command(0xE2); // Set system reset
	LCD_command(0xA1); // Set Frame rate 100 fps  
	LCD_command(0xEB); // Set LCD bias ratio E8~EB for 6~9 (min~max)  
	LCD_command(0x81); // Set V BIAS potentiometer
	LCD_command(0xA0); // Set V BIAS potentiometer: A0 ()        	
	LCD_command(0xC0);
	LCD_command(0xAF); // Set Display Enable
}

void LCD_command(unsigned char temp)
{
	SPI3->SSR |= 1 << 0;
	SPI3->TX[0] = temp;
	SPI3->CNTRL |= 1 << 0;
	while (SPI3->CNTRL & (1 << 0));
	SPI3->SSR &= ~(1 << 0);
}

void LCD_data(unsigned char temp)
{
	SPI3->SSR |= 1 << 0;
	SPI3->TX[0] = 0x0100 + temp;
	SPI3->CNTRL |= 1 << 0;
	while (SPI3->CNTRL & (1 << 0));
	SPI3->SSR &= ~(1 << 0);
}

void LCD_clear(void)
{
	int16_t i;
	LCD_SetAddress(0x0, 0x0);
	for (i = 0; i < 132 * 8; i++)
	{
		LCD_data(0x00);
	}
}

void LCD_SetAddress(uint8_t PageAddr, uint8_t ColumnAddr)
{
	LCD_command(0xB0 | PageAddr);
	LCD_command(0x10 | (ColumnAddr >> 4) & 0xF);
	LCD_command(0x00 | (ColumnAddr & 0xF));
}
void SPI3_Config(void) {
	SYS_UnlockReg(); // Unlock protected registers
	// SPI3 clock enable
	CLK->APBCLK |= 1 << 15;
	
	SYS->GPD_MFP |= 1 << 11; //1: PD11 is configured for SPI3
	SYS->GPD_MFP |= 1 << 9; //1: PD9 is configured for SPI3
	SYS->GPD_MFP |= 1 << 8; //1: PD8 is configured for SPI3

	SPI3->CNTRL &= ~(1 << 23); //0: disable variable clock feature
	SPI3->CNTRL &= ~(1 << 22); //0: disable two bits transfer mode
	SPI3->CNTRL &= ~(1 << 18); //0: select Master mode
	SPI3->CNTRL &= ~(1 << 17); //0: disable SPI interrupt    
	SPI3->CNTRL |= 1 << 11; //1: SPI clock idle high 
	SPI3->CNTRL &= ~(1 << 10); //0: MSB is sent first   
	SPI3->CNTRL &= ~(3 << 8); //00: one transmit/receive word will be executed in one data transfer

	SPI3->CNTRL &= ~(31 << 3); //Transmit/Receive bit length
	SPI3->CNTRL |= 9 << 3;     //9: 9 bits transmitted/received per data transfer

	SPI3->CNTRL |= (1 << 2);  //1: Transmit at negative edge of SPI CLK       
	SPI3->DIVIDER = 0; // SPI clock divider. SPI clock = HCLK / ((DIVIDER+1)*2). HCLK = 50 MHz
	SYS_LockReg();  // Lock protected registers  
}




//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// --------------------------------------------------------------LEDs-------------------------------------------------------------------//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 void led_config(void){
	 	SYS_UnlockReg(); //Unlock protected regs
	 // PC 12 - LED5 
	 	PC->PMD &= ~(0x02 << 24);
    PC->PMD |= 0x01 << 24;
	 // PC 13 - LED6 
	  PC->PMD &= ~(0x02 << 26);
    PC->PMD |= 0x01 << 26;
	 // PC 14 - LED7
	 	PC->PMD &= ~(0x02 << 28);
    PC->PMD |= 0x01 << 28;
	 // PC 15 - LED8 
	  PC->PMD &= ~(0x02 << 30);
    PC->PMD |= 0x01 << 30;
		SYS_LockReg();  // Lock protected registers
	 PC->DOUT |= (0xF<<12); // Turn off all LED
 }
 void LED_flash(void){
	PC->DOUT |= (1 << 12);
	for(int i =0; i < 6; i++){
		PC->DOUT ^= (1 << 12);
		CLK_SysTickDelay(500000);
	}
	PC->DOUT |= (1 << 12);
}
 


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ------------------------------------------------------------------BUZZER-------------------------------------------------------------//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void buzzer_config(void){
	 	SYS_UnlockReg(); //Unlock protected regs
	 // PB 11 - Buzzer 
	 	PB->PMD &= ~(0x02 << 22);
    PB->PMD |= 0x01 << 22;

		SYS_LockReg();  // Lock protected registers
	 PB->DOUT |= (0x1<<11); // Turn off all LED
}

void buzzer_beep(int beep_time){
		PB->DOUT |= (1 << 11);
		for(int j =0; j < 2*beep_time; j++){
			PB->DOUT ^= (1 << 11);
			CLK_SysTickDelay(200000);
		}
	
		PB->DOUT |= (1 << 11);


}
 
 
 


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ----------------------------------------------------------7-SEGMENT LEDs-------------------------------------------------------------//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GPIO Configuration
 void seven_seg_config(void){
//This control the 7-segment LED to display the number of press
//Display number 5 on U11 7segment LED
//Set mode for PC4 to PC7 
  PC->PMD &= (~(0xFF<< 8));		 //clear PMD[15:8] 
  PC->PMD |= (0b01010101 << 8);//Set output push-pull for PC4 to PC7
	
//Set mode for PE0 to PE7
	PE->PMD &= (~(0xFFFF<< 0));		    //clear PMD[15:0] 
	PE->PMD |= 0b0101010101010101<<0; //Set output push-pull for PE0 to PE7

// Turn off 7-segment LED
	PE->DOUT |= (0xFF<<0);	 
 }
 
 
// Display 
void display_LED(int num, int digit)
{
	for (int j = 7; j > 3; j--) { // This loop turn off all digit
      PC->DOUT &= ~(1 << j);
    }
	PC->DOUT |= (1<<(7-digit));     //Logic 1 to turn on the digit


  switch (num)
  {
    case 0: // Number 0
			PE->DOUT |= (0xFF<<0);
			PE->DOUT &= (0x82<<0);
      break;
    case 1: // Number 1
			PE->DOUT |= (0xFF<<0);
			PE->DOUT &= (0xEE<<0);
      break;
    case 2: // Number 2
			PE->DOUT |= (0xFF<<0);
			PE->DOUT &= (0x07<<0);
      break;
    case 3: // Number 3
			PE->DOUT |= (0xFF<<0);
			PE->DOUT &= (0x46<<0);
      break;
    case 4: // Number 4
			PE->DOUT |= (0xFF<<0);
			PE->DOUT &= (0x6A<<0);
      break;
    case 5: // Number 5
			PE->DOUT |= (0xFF<<0);
			PE->DOUT &= (0x52<<0);
      break;
    case 6: // Number 6
			PE->DOUT |= (0xFF<<0);
			PE->DOUT &= (0x12<<0);
      break;
    case 7: // Number 7
			PE->DOUT |= (0xFF<<0);
			PE->DOUT &= (0xE6<<0);
      break;
    case 8: // Number 8
			PE->DOUT |= (0xFF<<0);
			PE->DOUT &= (0x02<<0);
      break;
    case 9: // Number 9
			PE->DOUT |= (0xFF<<0);
			PE->DOUT &= (0x42<<0);
      break;
  }
}
 

void timer3_config(int count_value){
	SYS_UnlockReg(); //Unlock protected regs
	//Timer initialization start--------------
	//TM3 Clock selection and configuration
	// Select TMR3_S = CLKSEL1[22:20] = 000 => 12MHz
	CLK->CLKSEL1 &= ~(0b111 << 20);
	// enable clock for Timer. TMR3_EN = APBCLK[5]
	CLK->APBCLK |= (1 << 5);
	
	//reset Timer 
	TIMER3->TCSR |= (1 << 26);

	//Pre-scale =0 => Timer  Controller Clock Frequency = 12Mhz/(0+1) = 12MHz
	TIMER3->TCSR &= ~(0xFF << 0);
	
	//define Timer  operation mode
	TIMER3->TCSR &= ~(0b11 << 27); 
	TIMER3->TCSR |= (0b01 << 27);  //  01 = Periodic mode
	TIMER3->TCSR &= ~(1 << 24);    // Disable counter mode
	
	//TDR to be updated continuously while timer counter is counting
	TIMER3->TCSR |= (1 << 16);
	
	//Fin = 12 MHz - Fout = 120Hz --> Counter's TCMPR = 50k
	TIMER3->TCMPR = count_value;
	
	// Interrupt 
	// System Level
	NVIC->ISER[0] |= 1 << 11; // Enable TIMER3 interrupt (TMR3)
	NVIC->IP[2] &= (~(3 << 30)); // Set priority
	
	// Low-level
	TIMER3->TCSR |= (1 << 29);     // Enable Interrupt in Timer3. TCSR[29] = 1 => enable
	
	//start counting
	TIMER3->TCSR |= (1 << 30);
	//Timer  initialization end----------------
	SYS_LockReg();  // Lock protected registers
}
 
// Timer 3
void TMR3_IRQHandler(void) {
	display_LED(digit[current_digit], current_digit);// display value for each digit
	current_digit++; // move to the next digit 
	if(current_digit == 4) current_digit = 0; // move back to digit 0
	TIMER3->TISR |= (1 << 0); // Clear Flag
	
}




//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ----------------------------------------------------------External Interrupt 1-------------------------------------------------------//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void EXINT1_config(void){
	SYS_UnlockReg(); //Unlock protected regs
	// External interupt 1 (EINT1) for GPIO-B15 interrupt source
	// Device level
	PB->PMD &= (~(0x03 << 30)); // Set input mode
	PB->IMD &= (~(1 << 15)); // Choose edge trigger = 0 or level trigger = 1
	PB->IEN |= (1 << 15); // choose falling edge [15:0] or rising edge [31:16]
	// System level
	NVIC->ISER[0] |= 1 << 3; // Enable external interrupt 1 (EINT1)
	NVIC->IP[0] &= (~(3 << 30)); // Set priority
	SYS_LockReg();  // Lock protected registers
}

void EXINT1_debounce_config(void){
	//Debounce initialization start --------------------
	SYS_UnlockReg(); //Unlock protected regs
	PB->DBEN |= (1<<15); // Enable debounce for pin PB15
	GPIO->DBNCECON &= ~(0xF << 0); // Clear Clock selection bits 
	GPIO->DBNCECON |= (0x6 << 0);  // Sample interrupt input once per 32 clocks
	GPIO->DBNCECON |= (0x1 << 4); // Choose 10kHz as clock source for debounce
	SYS_LockReg();  // Lock protected registers
	//Debounce initialization end ----------------------
}

void EINT1_IRQHandler(void) {
	EINT1_flag = 1; // raise flag indicate GP15 is triggered
	PB->ISRC |= (1 << 15); // Clear interrupt flag
}
 





//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ---------------------------------------------------------------KEY MATRIX------------------------------------------------------------//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void key_config(void){
	SYS_UnlockReg(); //Unlock protected regs
	//Configure GPIO for Key Matrix
	//Rows - outputs
	PA->PMD &= (~(0b11<< 6));
  PA->PMD |= (0b01 << 6);    
	PA->PMD &= (~(0b11<< 8));
  PA->PMD |= (0b01 << 8);  		
	PA->PMD &= (~(0b11<< 10));
  PA->PMD |= (0b01 << 10);  
	
	
	// Interrupt
	// Device level
	// Collum 1
	PA->IMD &= (~(1 << 0));
	PA->IEN |= (1 << 0);
	// Collum 2
	PA->IMD &= (~(1 << 1));
	PA->IEN |= (1 << 1);
	// Collum 3
	PA->IMD &= (~(1 << 2));
	PA->IEN |= (1 << 2);
	
	// System Level
	NVIC->ISER[0] |= 1 << 4; 
	NVIC->IP[1] &= (~(3 << 6)); // Set priority
	SYS_LockReg();  // Lock protected registers
}

void key_debounce_config(void){
	
	SYS_UnlockReg(); //Unlock protected regs
	// Debounce Configuration
	PA->DBEN |= (1<<0); // Enable debounce for pin PA0
	PA->DBEN |= (1<<1); // Enable debounce for pin PA1
	PA->DBEN |= (1<<2); // Enable debounce for pin PA2
	GPIO->DBNCECON &= ~(0xF << 0); // Clear Clock selection bits 
	GPIO->DBNCECON |= (0x6 << 0);  // Sample interrupt input once per 32 clocks
	GPIO->DBNCECON |= (0x1 << 4); // Choose 10kHz as clock source for debounce
	SYS_LockReg();  // Lock protected registers
}
// Interrupt Service Rountine of GPIO from PA
void GPAB_IRQHandler(void) {
		
		if(C1_pressed) 
		{
				// Drive ROW1 output pin as LOW. Other ROW pins as HIGH
				// Check key 1
				PA->DOUT &= ~(1<<3);
				PA->DOUT |= (1<<4);
				PA->DOUT |= (1<<5);
			
				if (C1_pressed)
				{   
					input_location = 1;
					key_pressed = 1;
					PA->ISRC |= (1 << 0); // Clear interrupt flag
				}
				else 
				{
					// check key 4
					PA->DOUT |= (1<<3);
					PA->DOUT &= ~(1<<4);
					PA->DOUT |= (1<<5);	
					if (C1_pressed)
					{
							input_location = 4;
							key_pressed = 1;
							PA->ISRC |= (1 << 0); // Clear interrupt flag
					}
					else
					{
						// check key 7
						PA->DOUT |= (1<<3);
						PA->DOUT |= (1<<4);		
						PA->DOUT &= ~(1<<5);	
						if (C1_pressed)
						{
							
								input_location = 7;
								key_pressed = 1;
								PA->ISRC |= (1 << 0); // Clear interrupt flag
						}	
					}
				}
		}	
		
		
		else if(C2_pressed) 
		{	
				// Check key 2
				PA->DOUT &= ~(1<<3);
				PA->DOUT |= (1<<4);
				PA->DOUT |= (1<<5);	
				if (C2_pressed)
				{
					input_location = 2;
					key_pressed = 1;
					PA->ISRC |= (1 << 1); // Clear interrupt flag
				}
				else 
				{
						// Check key 5
						PA->DOUT |= (1<<3);
						PA->DOUT &= ~(1<<4);
						PA->DOUT |= (1<<5);
						if (C2_pressed)
						{
							input_location = 5;
							key_pressed = 1;
							PA->ISRC |= (1 << 1); // Clear interrupt flag	
						}	
						else
						{
							// Check key 8
							PA->DOUT |= (1<<3);
							PA->DOUT |= (1<<4);		
							PA->DOUT &= ~(1<<5);
							if (C2_pressed)
							{
								input_location = 8;
								key_pressed = 1;
								PA->ISRC |= (1 << 1); // Clear interrupt flag						
							}					
						}				
				}
		}	

		else if(C3_pressed) 
		{	
				// Check key 3
				PA->DOUT &= ~(1<<3);
				PA->DOUT |= (1<<4);
				PA->DOUT |= (1<<5);		
				if (C3_pressed)
				{
				input_location = 3;
				key_pressed = 1;
				PA->ISRC |= (1 << 2); // Clear interrupt flag
				}
				else
				{
				  // Check key 6
					PA->DOUT |= (1<<3);
					PA->DOUT &= ~(1<<4);
					PA->DOUT |= (1<<5);
					if (C3_pressed)
					{
						input_location = 6;
						key_pressed = 1;
						PA->ISRC |= (1 << 2); // Clear interrupt flag
					}
					else
					{
						 // Check key 9
						PA->DOUT |= (1<<3);
						PA->DOUT |= (1<<4);		
						PA->DOUT &= ~(1<<5);		
						if (C3_pressed)
						{
							change_axis = 1;
							PA->ISRC |= (1 << 2); // Clear interrupt flag
						}
					}
				}
		}

			PA->ISRC |= (1 << 0); // Clear interrupt flag
			PA->ISRC |= (1 << 1); // Clear interrupt flag
			PA->ISRC |= (1 << 2); // Clear interrupt flag
			//Turn all Rows to LOW 
			PA->DOUT &= ~(1<<3);
			PA->DOUT &= ~(1<<4);
			PA->DOUT &= ~(1<<5);
		
}





//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ---------------------------------------------------------------Game Play-------------------------------------------------------------//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void update_map(char map[][9]){
			LCD_clear();
			for(int i =0; i < 8; i++){
				for(int j =0; j < 8; j++){
				// if the location stored 'X' => hit point => display 'X'
					if(map[i][j]== 'X'){
						printC_5x7(25 + j*10, 7 + i*7,'X');
					}
				// if '1' or '0' => '-'
					else if(map[i][j]== '0'||map[i][j]== '1')
					{
					printC_5x7(25 + j*10, 7 + i*7,'-');
					}
				}
			}

}

void reload(char map[][9]){
	for(int i =0; i < 8; i++){
			for(int j =0; j < 8; j++){
				// if the location stored 'X' => hit point => move back to '1'
					if(map[i][j] == 'X'){
						map[i][j] = '1';
					}

			}
	}

}


//------------------------------------------- main.c CODE ENDS ---------------------------------------------------------------------------
