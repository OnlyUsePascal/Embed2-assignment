//------------------------------------------- main.c CODE STARTS -------------------------------------------------------------------------------------
#include <stdio.h>
#include "NUC100Series.h"
#include "MCU_init.h"
#include "SYS_init.h"
#include "LCD.h"
#include "Draw2D.h"


// Macro define
#define HXT_STATUS 1<<0
#define PLL_STATUS 1<<2
#define PLLCON_FB_DV_VAL 10
#define CPUCLOCKDIVIDE 1
#define BUZZER_BEEP_TIME 10						// Buzzer will go off 5 times
#define BOUNCING_DELAY 200000					// Fix the boucing problem

// Declare funtion
void System_Config(void);
void KeyPad_Config(void);
uint8_t KeyPadScanning(void);
void main_game(void);			// Specify the shoot and coordinate
void sevenSegDislay(void);				// content of game displayed
void Buzzer_Beep(int beepTime);

// Functions for UART0
void UART0_Config(void);
void UART0_sendChar(int ch);
void UART02_IRQHandler(void);

// Functions for SPI3 and LCD
void SPI3_Config(void);
void TIMER0_Config(void);
void GPIO_Config(void);
void LCD_start(void);
void LCD_command(unsigned char temp);
void LCD_data(unsigned char temp);
void LCD_clear(void);
void LCD_SetAddress(uint8_t PageAddr, uint8_t ColumnAddr);
// Function for LED GPIO
void sevenSeg_Config (void);


// Temp map coordinator row = y and col = x
// Only use for paste value from uart to board
volatile int col = 0;
volatile int row = 0;

// Map coordinates and key matrix
volatile int coor[2]={1,1};			// X and Y Coordinator display on the LED (not the real coordinate of the map)
volatile int shotNum = 0;			// the number of shot
volatile int currentCoor = 0;		// Cuurent coordinator X and Y
uint8_t keyPressed = 0;				// identify the pressed Key Matrix
volatile XY_LED = 0;				// X and Y on LED, value x = 0 and y = 1

// Define score and its key
volatile char highScore_str[] = "00";
volatile int highScore = 0;
volatile int score = 0;
volatile char score_str[] = "00";
volatile int shipNum = 0;
volatile char ship_str[] = "00";

// Interrupt variables
volatile int loadedMap = 1;		// variable key to test the map was loaded
volatile int buzzerFlag = 0;		// to activate Buzzer_Beep() 
volatile int tmr0Flag = 0;	// for tmr interrupt
// Each recievied from uart temp memory
volatile char ReceivedByte;

//FMS: welcome_screen -> map_loading -> game_processing -> end_game -> reset_value
typedef enum {welcome_screen, map_loading, game_processing, end_game, reset_value} game_state;
game_state state;

//// Default map
//int map[8][8]={
//{0,0,0,0,0,0,0,0},
//{0,0,0,0,0,0,0,0},
//{0,0,0,0,0,0,0,0},
//{0,0,0,0,0,0,0,0},
//{0,0,0,0,0,0,0,0},
//{0,0,0,0,0,0,0,0},
//{0,0,0,0,0,0,0,0},
//{0,0,0,0,0,0,0,0}
//};

// Default map
int map[8][8]={
{1,1,0,1,1,0,1,1},
{0,0,0,0,0,0,0,0},
{1,1,0,1,1,0,0,0},
{0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0}
};

//7 Segment partern
int pattern[] = {
	0b10000010,  //Number 0          
	0b11101110,  //Number 1          
	0b00000111,  //Number 2         
	0b01000110,  //Number 3          
	0b01101010,  //Number 4          
	0b01010010,  //Number 5          
	0b00010010,  //Number 6          
	0b11100110,  //Number 7          
	0b00000010,  //Number 8         
	0b01000010,  //Number 9
	0b11111111   //Blank LED 
};  

int main(void){
	System_Config();
	TIMER0_Config();
	UART0_Config();
	SPI3_Config();
	GPIO_Config();

	KeyPad_Config();
	sevenSeg_Config();

	LCD_start();
	LCD_clear();

	while (1){
		PC->DOUT &= ~(1<<14);				// LED8 - program execution
		main_game();						// initiate the game algorithm
	}
}






















// ---------------------------------------------------------------------------------
// INTERRUPT
// ---------------------------------------------------------------------------------
// Timer0 interrupt for 7 segs LED
void TMR0_IRQHandler(void){
	tmr0Flag = 1;						// Time taken/time delay to scan LEDs transition 
	TIMER0->TISR |= (1<<0);		// generate interrupt
}

// UART0 interrupt - RX
void UART02_IRQHandler(void){
	// Receive the input data from TX 
	ReceivedByte = UART0->DATA;		
	UART0_sendChar(ReceivedByte);
		
	// Reconstruct the map from the TX read data
	if (ReceivedByte == '0' || ReceivedByte == '1'){
		if (ReceivedByte == '0') map[row][col] = 0;
		if (ReceivedByte == '1') map[row][col] = 1;	
		// Create the map from read file
		if (col == 7)
		{
			col = 0;
			if (row == 7)
			{
				// Stop RX
				loadedMap = 1;		// set flag to 1
				row = 0;
			} else row++;
		} else col++;
	}
}

// PB15
void EINT1_IRQHandler(void) 
{
	PD12 = 0;
	switch (state)	
	{
		case welcome_screen: //Welcome Screen
			LCD_clear();	
			CLK_SysTickDelay(200000);
			if (loadedMap != 0) {
				state = map_loading;	
			}
			else {
				state = welcome_screen;	// state transition
			}
			break;

		case game_processing:
			// Check the shooted coordinoor = ship coordinator
			//coor[x] - 1 because of display visual (1=0)
			if(map[coor[1]-1][coor[0]-1]==1) 		// the coorinator of the ship
				{
					map[coor[1]-1][coor[0]-1] = 2;		// Assign value to 2 when hit 
					score++;
					for(int i = 0; i < 6; i++)
						{
							PC->DOUT ^= 1 << 12;				// LED5 flash 3 time
							CLK_SysTickDelay(500000);			//0.5s
						}
				}

			// Check sunk ship
			//Check if two dots are adjacent Horizontal		
			for(int i = 0; i < 8; i++) {
				for(int j = 0; j < 7; j++) 
				{
					if (map[j][i] == 2 && map[j+1][i] == 2 ) {
						shipNum++;
						map[j][i] = 3;
						map[j+1][i] = 3;
					}	
				}
			}
			//Check if two dots are adjacent Vertical	
			for(int i = 0; i < 8; i++) {
				for(int j = 0; j < 8; j++) 
				{
					if (map[i][j] == 2 && map[i][j+1] == 2 ) {
						shipNum++;
						map[i][j] = 3;
						map[i][j+1] = 3;
					}	
				}
			}	
			if (score > highScore) {
				highScore = score;
			}	

			if (shotNum<16) shotNum++;
			state = map_loading;			// Upload the map
			break;
			
		case end_game://restart
			LCD_clear();	
			state = reset_value;				// Reset value to default
			break;	
	}  
	PB->ISRC |= (1 << 15);			// Generate interrupt
	PD12 = 1;
}
















// ---------------------------------------------------------------------------------
// CONFIG
// ---------------------------------------------------------------------------------
void System_Config(void){
	SYS_UnlockReg();				// Unlock protected bits
	
	// ---- PLL Config
	CLK->PWRCON |= (0x01 << 0);
	while(!(CLK->CLKSTATUS & (1 << 0)));
	CLK->PLLCON &= ~(1u << 19); //0: PLL input is HXT
	CLK->PLLCON &= ~(1u << 16); //PLL in normal mode
	CLK->PLLCON &= (~(0x01FFu << 0));
	CLK->PLLCON |= 48;
	CLK->PLLCON &= ~(1u <<18); //0: enable PLLOUT
	while(!(CLK->CLKSTATUS & (0x01 << 2)));
	//normal mode opertation
	CLK->PWRCON &= ~(1u << 7);
	// CPU clock source selection
	CLK->CLKSEL0 &= (~(0x07u << 0));
	CLK->CLKSEL0 |= (0x02 << 0);
	//clock frequency division
	CLK->CLKDIV &= ~(0x0Fu << 0);
	
	SYS_LockReg();
}

void TIMER0_Config(void){
	SYS_UnlockReg(); // Unlock protected registers

	CLK->APBCLK |= (1 << 2); //enable timer0
	//reset timer
	CLK->CLKSEL1 &= ~(7u <<8);
	CLK->CLKSEL1 |= (0b010 << 8);		// select clock source = 50MHz
	TIMER0->TCSR &= ~(0xFF <<0); //prescaler
	
	//periodic mode
	TIMER0->TCSR |= (1 <<26); //reset 
	TIMER0->TCSR &= ~(3u <<27); 
	TIMER0->TCSR |= (1 <<27); //periodic mode
	
	//update count to timer
	TIMER0->TCSR &= ~(1u << 24); //disable counter mode
	TIMER0->TCMPR = 5e4; //compare value
	// TIMER0->TCSR |= (1 << 16);
	
	// interrupt
	TIMER0->TCSR |= (1 << 29); //enable interrupt
	NVIC->ISER[0] |= 1 << 8; //Set Timer0 in NVIC Set-Enable Control Register (NVIC_ISER)	
	NVIC->IP[2] &= (~(3u << 6)); //Priority for Timer 0

	TIMER0->TCSR |= (1 << 30); //start counting

	SYS_LockReg();  // Lock protected registers
}

// UART0 congiguration
void UART0_Config(void){
	SYS_UnlockReg(); // Unlock protected registers

	// CLOCK
	CLK->CLKSEL1 |= (0x3 << 24); // UART0 clock source is 22.1184 MHz
	CLK->CLKDIV &= ~(0xF << 8); // clock divider is 1
	CLK->APBCLK |= (0x1 << 16); // enable UART0 clock

	// UART0 pin configuration. PB.1 pin is for UART0 TX
	PB->PMD &= ~(0b11 << 2);
	PB->PMD |= (0b01 << 2); 		// PB.1 is output pin
	PB->PMD &= ~(3 << 0);	// Set Pin Mode for GPB.0(RX - Input)
	SYS->GPB_MFP |= (1 << 0); // GPB_MFP[0] = 1 -> PB.0 is UART0 RX pin
	SYS->GPB_MFP |= (1 << 1); // GPB_MFP[1] = 1 -> PB.1 is UART0 TX pin
	
	// UART0 operation configuration
	UART0->LCR |= (3 << 0); // 8 data bit
	UART0->LCR &= ~(1 << 2); // one stop bit	
	UART0->LCR &= ~(1 << 3); // no parity bit
	UART0->FCR |= (1 << 1); // clear RX FIFO
	UART0->FCR |= (1 << 2); // clear TX FIFO
	UART0->FCR &= ~(0xF << 16); // FIFO Trigger Level is 1 byte

	//Baud rate config: BRD/A = 1, DIV_X_EN=0
	//--> Mode 0, Baud rate = UART_CLK/[16*(A+2)] = 22.1184 MHz/[16*(10+2)]= 115200 bps
	UART0->BAUD &= ~(0x03u << 28); // mode 0
	UART0->BAUD &= ~(0x0FFFFu << 0);
	UART0->BAUD |= 10;
	
	//UART0 Interrupt
	UART0->IER |= (1 << 0); //enable interrupt
	UART0->FCR |= (0000 << 4); 	// RX interrupt trigger level
	NVIC->ISER[0] = 1 << 12; 	//priority
	NVIC->IP[1] &= (~(3<<22));		// IP[1] - PRI2

	SYS_LockReg();  // Lock protected registers
}

void KeyPad_Config(void) {
	GPIO_SetMode(PA, BIT0, GPIO_MODE_QUASI);
	GPIO_SetMode(PA, BIT1, GPIO_MODE_QUASI);
	GPIO_SetMode(PA, BIT2, GPIO_MODE_QUASI);
	GPIO_SetMode(PA, BIT3, GPIO_MODE_QUASI);
	GPIO_SetMode(PA, BIT4, GPIO_MODE_QUASI);
	GPIO_SetMode(PA, BIT5, GPIO_MODE_QUASI);
}

void SPI3_Config(void){
	SYS_UnlockReg(); // Unlock protected registers

	// CLOCK
	CLK->APBCLK |= 1 << 15; //enable SPI3

	SYS->GPD_MFP |= 1 << 11; //1: PD11 is configured for alternative function
	SYS->GPD_MFP |= 1 << 9; //1: PD9 is configured for alternative function
	SYS->GPD_MFP |= 1 << 8; //1: PD8 is configured for alternative function
	
	SPI3->CNTRL &= ~(1u << 23); //0: disable variable clock feature
	SPI3->CNTRL &= ~(1u << 22); //0: disable two bits transfer mode
	SPI3->CNTRL &= ~(1u << 18); //0: select Master mode
	SPI3->CNTRL &= ~(1u << 17); //0: disable SPI interrupt    

	SPI3->CNTRL |= 1 << 11; //1: SPI clock idle high 
	SPI3->CNTRL &= ~(1 << 10); //0: MSB is sent first   
	SPI3->CNTRL &= ~(3 << 8); //00: one transmit/receive word will be executed in one data transfer
	SPI3->CNTRL &= ~(31 << 3); //Transmit/Receive bit length
	SPI3->CNTRL |= 9 << 3;     //9: 9 bits transmitted/received per data transfer
	
	SPI3->CNTRL |= (1 << 2);  //1: Transmit at negative edge of SPI CLK       
	SPI3->DIVIDER = 0; // SPI clock divider. SPI clock = HCLK / ((DIVIDER+1)*2). HCLK = 50 MHz

	SYS_LockReg();  // Lock protected registers
}

//Configure GPIO for 7segment
void sevenSeg_Config (void) {
	//PC4 to PC7 - output push-pull
	GPIO_SetMode(PC, BIT4, GPIO_MODE_OUTPUT);
	GPIO_SetMode(PC, BIT5, GPIO_MODE_OUTPUT);
	GPIO_SetMode(PC, BIT6, GPIO_MODE_OUTPUT);
	GPIO_SetMode(PC, BIT7, GPIO_MODE_OUTPUT);		
	//PE0 to PE7 - output push-pull
	GPIO_SetMode(PE, BIT0, GPIO_MODE_OUTPUT);		
	GPIO_SetMode(PE, BIT1, GPIO_MODE_OUTPUT);		
	GPIO_SetMode(PE, BIT2, GPIO_MODE_OUTPUT);		
	GPIO_SetMode(PE, BIT3, GPIO_MODE_OUTPUT);		
	GPIO_SetMode(PE, BIT4, GPIO_MODE_OUTPUT);		
	GPIO_SetMode(PE, BIT5, GPIO_MODE_OUTPUT);		
	GPIO_SetMode(PE, BIT6, GPIO_MODE_OUTPUT);		
	GPIO_SetMode(PE, BIT7, GPIO_MODE_OUTPUT);	
}

void GPIO_Config(void){
	// Debounce
	// CLK->PWRCON |= (1 << 3); //turn on 10 kHz crystal for debounce function
	// while (!(CLK->CLKSTATUS & (1 << 3)));
  PB->DBEN |= (1 << 15);
  GPIO->DBNCECON |= (1 << 4); // Debounce counter clock source is the internal 10kHz low speed oscillator 
  GPIO->DBNCECON &= ~(0xF << 0);
  GPIO->DBNCECON |= 0x7 << 0; //sample interrupt every 128 clocks, which is around 78 times per second
	
	// BUZZER - interrupt handling routine
	PB->PMD &= ~(0x03<<22);	
	PB->PMD |= (0x01<<22);

	// LED5 toggle
	PC->PMD &= ~(0x03<<24);			// clear bits
	PC->PMD |= (0x01<<24);			// output pc12

	//GPIO Interrupt configuration. GPIO-B15 is the interrupt source
  PB->PMD &= (~(0x03ul << 30));
  PB->IMD &= (~(1ul << 15));
  PB->IEN |= (1ul << 15);
	//NVIC interrupt configuration for GPIO-B15 interrupt source
  NVIC->ISER[0] |= 1ul<<3;
  NVIC->IP[0] &= (~(3ul<<30));

	// 7 segment
	//7 segment configuration
	for (int i = 0; i <= 7; ++i){
		PE->PMD &= ~(0x03u << (i << 1));
		PE->PMD |= (1 << (i << 1));
	}
	//7 segment setupsetup
	for (int i = 4; i <= 7; ++i){
		PC->PMD &= ~(0x03u << (i << 1));
		PC->PMD |= (1 << (i << 1));
		PC->DOUT &= ~(1 << i);
	}	
}























// ---------------------------------------------------------------------------------
// MAIN GAME
// ---------------------------------------------------------------------------------
// Set up the game algorith from each state to shooting
void main_game(void){
	switch(state){
		//---------------------- reset_value state ------------------------------------------------
		case reset_value:			// Reset global var to defautl 0
			for (int i = 0; i < 8; i++){
				for (int j=0; j<8; j++){
					if (map[i][j] == 2 || map[i][j] == 3 ){				// Reset the value of shooted point to 1
						map[i][j] = 1;
					} 
				}
			} 
			// Reset all values when game restarts
			row = 0;
			col = 0;
			shotNum = 0;
			score = 0;
			shipNum = 0;
			buzzerFlag = 0;
			coor[0] = 1; // x
			coor[1] = 1; // y
			
			// Display context
			printS_5x7(1, 40, "Loading the game. Please wait ...");
			CLK_SysTickDelay(4000000);
			LCD_clear();
			
			state = welcome_screen;		// state transition
			break;
			
		//------------------------------------------------ Welcome state ------------------------------------------------
		case welcome_screen:					// Display welcomescreen
			printS_5x7(0, 0,  " ------------------------ ");
			printS_5x7(0, 10, "      BATTLESHIP GAME    ");
			printS_5x7(0, 20, " ------------------------ ");
			printS_5x7(0, 45, " Press PB15 to continue  ");
			printS(0, 55, "                                ");

			if (loadedMap == 0) {
				printS_5x7(0, 30, "Pls load the map to play.");
			}

			if (loadedMap > 0) 
				printS_5x7(0, 30, " Map Loaded Successfully.");
			break;
			
		//------------------------------------------------ Initialize map state ------------------------------------------------
		case map_loading:		// Loading the map
			LCD_clear();
			int indexRow = 2;
			int indexCol = 0;
		
			// Display the 8x8 map from the file
			for (int i=0; i<8;i++){
				for (int j=0; j<8;j++)
				{
					// Generate the map battleship
					if (map[i][j] != 2 && map[i][j] != 3 ) {
						printS_5x7(indexRow, indexCol, "-"); 						// Display the coordinate of water "0" as "-"
					}	
					else printS_5x7(indexRow, indexCol, "X");						// Display the coordinate of the ship "2" and "3" as "X"
					indexRow = indexRow+8;
				}
				indexCol = indexCol+8;
				indexRow = 2;
			}
			// Print score
			sprintf(score_str, "  %d", score); //trans integer into str
			printS_5x7(90, 0, " Score:");
			printS_5x7(96, 8, score_str);

			//Print sunk shup number
			sprintf(ship_str, "  %d", shipNum);
			printS_5x7(90, 16, " Ship :");
			printS_5x7(96, 24,  ship_str);
			state = game_processing;				// state transition
			break;
		
		//------------------------------------------------ Gameplay state --------------------------------------------------
		case game_processing:			// game play
			sevenSegDislay();
			keyPressed = KeyPadScanning();		// check key
			
			//Current coor of X or Y on U11
			printS_5x7(70, 32, "     Coor :     ");
			if (XY_LED == 1) {
				printS_5x7(85, 40, "    Y      ");
				printS_5x7(85, 48, "           ");
				printS_5x7(85, 48 + 8, "           ");
				printS_5x7(85, 48 + 8 + 4, "           ");
			}
			else {
				printS_5x7(85, 40, "    X      ");
				printS_5x7(85, 48, "           ");
				printS_5x7(85, 48 + 8, "           ");
				printS_5x7(85, 48 + 8 + 4, "           ");
			}
				
			if(keyPressed == 9)
			{
				//Change coordination whenever K9 is pressed
				if (XY_LED == 1) {
					XY_LED = 0;
				} else {
					XY_LED = 1;
				}
					
				if(currentCoor == 0) {
					currentCoor = 1; 
				}
				else {
					currentCoor = 0;
				} 
				CLK_SysTickDelay(BOUNCING_DELAY);
				
			} else { 
				if (keyPressed != 0) 
				{
					// Update the coordinate when Key matrix pressed
					coor[currentCoor] = keyPressed;
					CLK_SysTickDelay(BOUNCING_DELAY);
				}
			}
			keyPressed = 0;

			//If shoot all 5 ships
			if (score == 10) 
			{
				LCD_clear();
				state = end_game; // state transition
			}	

			//If shoot more than 15 times
			if (shotNum > 15) 
			{
				LCD_clear();
				state = end_game;		// state transition 
			}
			
			break;
			
		//-------------------------- Game Over state -----------------------------------------------
		case end_game:		
			// Turn off all 7Seg LED and LED 5
			PC->DOUT &= ~(1<<7);    // U11
			PC->DOUT &= ~(1<<6);	// U12
			PC->DOUT &= ~(1<<5);	// U13
			PC->DOUT &= ~(1<<4);	// U14

			// Print high score and gameover state
			char finalScore[] = "00";
			if (score == 10) {	// --------------------------
				sprintf(highScore_str, " %d   ", highScore);
				printS_5x7(1, 5,  "HIGH SCORE:  ");
				printS_5x7(60, 5, highScore_str);
				printS_5x7(0, 16, "      YOU WON!      ");
			} else {
				sprintf(highScore_str, " %d   ", highScore);
				printS_5x7(1, 5,  "HIGH SCORE:  ");
				printS_5x7(60, 5, highScore_str);
				printS_5x7(0, 16, "        YOU LOSE!       ");
			}
			
			// Final score
			sprintf(finalScore, "%d    ", score);	
			printS_5x7(0, 29, "Final Score:");
			printS_5x7(60, 29, finalScore);
			
			// Number of sunk ship
			sprintf(ship_str,   " %d    ", shipNum);
			printS_5x7(0, 37, "Sunk Ship: ");
			printS_5x7(60, 37,  ship_str);
		
			printS_5x7(0, 47, "--------------------------");
			printS_5x7(0, 55, " Press PB15 to restart!   ");
			printS_5x7(0, 55 + 1, "--------------------------");
			//printS_5x7(0, 55 + 8 * 2, "--------------------------");

			//Buzzer will go off 5 times (only once)
			if(buzzerFlag == 0)
			{
				Buzzer_Beep(BUZZER_BEEP_TIME);
				buzzerFlag = 1;				//Turn off buzzerFlag key
			}
			CLK_SysTickDelay(20000);
			break;
		
		default: break;
	}
}























// ---------------------------------------------------------------------------------
// ACTION
// ---------------------------------------------------------------------------------
// Sending map file from module - TX
void UART0_sendChar(int ch){
	while (UART0->FSR & (0x01<<23));	// wait til TX FIFO is not full
	UART0->DATA = ch;
	if (ch == '\n'){				// map jump to the new line
		while (UART0->FSR & (0x01<<23));
		UART0->DATA = '\r';		// new line
	}
}

// Setup the U11 and LED5 for game algorithm
void sevenSegDislay(void){
	//A loop to trigger tmr0
	while (tmr0Flag != 1);
	tmr0Flag = 0;
	// Display the X or Y 7 Segment coordinate
	if(XY_LED == 0) {
		PE->DOUT = pattern[coor[0]]; // Display coordinator of X
	}		
	else PE->DOUT = pattern[coor[1]];// Display coordinator of Y
	
	// Turn on LED of coordinator X or Y
	PC->DOUT |= (1<<7);     // on U11
	PC->DOUT &= ~(1<<6);	// off U12
	PC->DOUT &= ~(1<<5);	// off U13
	PC->DOUT &= ~(1<<4);	// off U14	
	
	if(shotNum != 0)
	{
		if(shotNum/10 == 1)
		{
			while (tmr0Flag != 1);		
			tmr0Flag = 0;
			//Select the 7segment U13
			PE->DOUT = pattern[1];
			PC->DOUT &= ~ (1<<7);   //off U11
			PC->DOUT &= ~(1<<6);	//off U12
			PC->DOUT |=(1<<5);		//on U13
			PC->DOUT &= ~(1<<4);	//off U14
		}
					
		while (tmr0Flag != 1);		
		tmr0Flag = 0;;
		//Select the 7segment U14
		PE->DOUT = pattern[shotNum % 10];
		PC->DOUT &= ~ (1<<7);    	//off U11
		PC->DOUT &= ~(1<<6);		//off U12	
		PC->DOUT &= ~(1<<5);		//off U13	
		PC->DOUT |=(1<<4);			//on U14	
		
		while (tmr0Flag != 1);
		tmr0Flag = 0;
	}
}

// Function scan key pressed
uint8_t KeyPadScanning(void) {
	// Col 1
	PA0 = 1; PA1 = 1; PA2 = 0; PA3 = 1; PA4 = 1; PA5 = 1;
	if (PA3 == 0) return 1;
	if (PA4 == 0) return 4;
	if (PA5 == 0) return 7;
	// Col 2
	PA0 = 1; PA1 = 0; PA2 = 1; PA3 = 1; PA4 = 1; PA5 = 1;
	if (PA3 == 0) return 2;
	if (PA4 == 0) return 5;
	if (PA5 == 0) return 8;
	// Col 3
	PA0 = 0; PA1 = 1; PA2 = 1; PA3 = 1; PA4 = 1; PA5 = 1;
	if (PA3 == 0) return 3;
	if (PA4 == 0) return 6;
	if (PA5 == 0) return 9;
	return 0;
}

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

void Buzzer_Beep(int beepTime){
	for (int i=0; i<beepTime; i++){
		PB->DOUT ^= (1<<11);
		CLK_SysTickDelay(2000000);

	} 
}
