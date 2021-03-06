// ADCTestMain.c
// Runs on TM4C123
// This program periodically samples ADC channel 0 and stores the
// result to a global variable that can be accessed with the JTAG
// debugger and viewed with the variable watch feature.
// Daniel Valvano
// September 5, 2015

/* This example accompanies the book
   "Embedded Systems: Real Time Interfacing to Arm Cortex M Microcontrollers",
   ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2015

 Copyright 2015 by Jonathan W. Valvano, valvano@mail.utexas.edu
    You may use, edit, run or distribute this file
    as long as the above copyright notice remains
 THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
 OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
 VALVANO SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL,
 OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 For more information about my classes, my research, and my books, see
 http://users.ece.utexas.edu/~valvano/
 */

// center of X-ohm potentiometer connected to PE3/AIN0
// bottom of X-ohm potentiometer connected to ground
// top of X-ohm potentiometer connected to +3.3V 
#include <stdint.h>
#include "ADCSWTrigger.h"
#include "../inc/tm4c123gh6pm.h"
#include "PLL.h"
#include "Timer1.h"
#include "ST7735.h"
#include <stdio.h>

#define PF2             (*((volatile uint32_t *)0x40025010))
#define PF1             (*((volatile uint32_t *)0x40025008))
void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value
void WaitForInterrupt(void);  // low power mode

volatile uint32_t ADCvalue;
// This debug function initializes Timer0A to request interrupts
// at a 100 Hz frequency.  It is similar to FreqMeasure.c.

uint32_t currentIndex = 0;
uint32_t timestamps[1000] = { 0 }, adcValues[1000] = { 0 };
uint32_t smallestTime = 0xFFFFFFFF;
uint32_t largestTime = 0;
uint32_t smallestADC = 0xFFFFFFFF;
uint32_t largestADC = 0;
uint32_t histogram[4096] = { 0 };
uint32_t deltaTime;
uint32_t timeJitter;
//Globals that contain the index for the Timer0A ISR, and array for timestamp and ADC values

void Timer0A_Init100HzInt(void){
  volatile uint32_t delay;
  DisableInterrupts();
  // **** general initialization ****
  SYSCTL_RCGCTIMER_R |= 0x01;      // activate timer0
  delay = SYSCTL_RCGCTIMER_R;      // allow time to finish activating
  TIMER0_CTL_R &= ~TIMER_CTL_TAEN; // disable timer0A during setup
  TIMER0_CFG_R = 0;                // configure for 32-bit timer mode
  // **** timer0A initialization ****
                                   // configure for periodic mode
  TIMER0_TAMR_R = TIMER_TAMR_TAMR_PERIOD;
  TIMER0_TAILR_R = 799999;         // start value for 100 Hz interrupts
  TIMER0_IMR_R |= TIMER_IMR_TATOIM;// enable timeout (rollover) interrupt
  TIMER0_ICR_R = TIMER_ICR_TATOCINT;// clear timer0A timeout flag
  TIMER0_CTL_R |= TIMER_CTL_TAEN;  // enable timer0A 32-b, periodic, interrupts
  // **** interrupt initialization ****
                                   // Timer0A=priority 2
  NVIC_PRI4_R = (NVIC_PRI4_R&0x00FFFFFF)|0x40000000; // top 3 bits
  NVIC_EN0_R = 1<<19;              // enable interrupt 19 in NVIC
}

void Timer0A_Handler(void){
  TIMER0_ICR_R = TIMER_ICR_TATOCINT;    // acknowledge timer0A timeout
  PF2 ^= 0x04;                   // profile
  PF2 ^= 0x04;                   // profile
  ADCvalue = ADC0_InSeq3();
  PF2 ^= 0x04;                   // profile

  if(currentIndex < 1000) {
    timestamps[currentIndex] = TIMER1_TAR_R;
    adcValues[currentIndex] = ADCvalue;
    currentIndex += 1;
  }
}

void Draw_Line(int16_t startX, int16_t startY, int16_t endX, int16_t endY) {
	
	for(int16_t x = startX; x < endX; x += 1) {
		ST7735_DrawPixel(x, startY, ST7735_BLUE);
	}
	
	for(int16_t y = startY; y < endY; y += 1) {
		ST7735_DrawPixel(startX, y, ST7735_BLUE);
	}
	
}

int main(void){
	DisableInterrupts();									// disable interrupts while configuring
  PLL_Init(Bus80MHz);                   // 80 MHz
  SYSCTL_RCGCGPIO_R |= 0x20;            // activate port F
  ADC0_InitSWTriggerSeq3_Ch9();         // allow time to finish activating
  Timer0A_Init100HzInt();               // set up Timer0A for 100 Hz interrupts
  GPIO_PORTF_DIR_R |= 0x06;             // make PF2, PF1 out (built-in LED)
  GPIO_PORTF_AFSEL_R &= ~0x06;          // disable alt funct on PF2, PF1
  GPIO_PORTF_DEN_R |= 0x06;             // enable digital I/O on PF2, PF1
                                        // configure PF2 as GPIO
  GPIO_PORTF_PCTL_R = (GPIO_PORTF_PCTL_R&0xFFFFF00F)+0x00000000;
  GPIO_PORTF_AMSEL_R = 0;               // disable analog functionality on PF
  PF2 = 0;                              // turn off LED
  Timer1_Init();												// set up Timer1 for 12.5ns interval counting
	ST7735_InitR(INITR_REDTAB);
	EnableInterrupts();
	
	while(currentIndex < 1000){
    //PF1 ^= 0x02;
		PF1 = (PF1*12345678)/1234567+0x02;  // this line causes jitter
	} // Toggles heartbeat while ADC is collecting data
	
	DisableInterrupts(); // disable data collection
	
	/* PROCESS DATA */
		/* Calculate delta Time for each timestamp pair, then calculate the Time Jitter */
	
	for(int i = 0; i < 1000 - 1; i += 1) {
		deltaTime = timestamps[i] - timestamps[i + 1];
		if(deltaTime < smallestTime) {
			smallestTime = deltaTime;
		}
		if(deltaTime > largestTime) {
			largestTime = deltaTime;
		}	
	}

	timeJitter = largestTime - smallestTime;  // if time jitter is larger than 10ms then we have a problem
	
		/* Sum up discrete ADC values for histogram plot */
	
	for(int i = 1; i < 1000; i += 1) {
		histogram[adcValues[i]] += 1;
		if(adcValues[i] < smallestADC) {
			smallestADC = adcValues[i];
		}
		else if(adcValues[i] > largestADC) {
			largestADC = adcValues[i];
		}
	} // histgram array now contains the count for every possible adc value for display on the LCD

	Draw_Line(10,10,50,50);

}
// End of Main
