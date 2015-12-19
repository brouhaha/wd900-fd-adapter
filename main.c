// WD900-FD-ADAPTER firmware
// Copyright 2015 Eric Smith <spacewar@gmail.com>

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 3
// as published by the Free Software Foundation.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <stdbool.h>
#include <stdint.h>
#include <xc.h>

// PIC16F1827 Configuration Bit Settings
// CONFIG1
#pragma config FOSC = INTOSC    // Oscillator Selection (INTOSC oscillator: I/O function on CLKIN pin)
#pragma config WDTE = OFF       // Watchdog Timer Enable (WDT disabled)
#pragma config PWRTE = OFF      // Power-up Timer Enable (PWRT disabled)
#pragma config MCLRE = ON       // MCLR Pin Function Select (MCLR/VPP pin function is MCLR/Vpp)
#pragma config CP = OFF         // Flash Program Memory Code Protection (Program memory code protection is disabled)
#pragma config CPD = OFF        // Data Memory Code Protection (Data memory code protection is disabled)
#pragma config BOREN = ON       // Brown-out Reset Enable (Brown-out Reset enabled)
#pragma config CLKOUTEN = OFF   // Clock Out Enable (CLKOUT function is disabled. I/O or oscillator function on the CLKOUT pin)
#pragma config IESO = OFF       // Internal/External Switchover (Internal/External Switchover mode is disabled)
#pragma config FCMEN = OFF      // Fail-Safe Clock Monitor Enable (Fail-Safe Clock Monitor is disabled)
// CONFIG2
#pragma config WRT = OFF        // Flash Memory Self-Write Protection (Write protection off)
#pragma config PLLEN = OFF      // PLL Enable (4x PLL disabled)
#pragma config STVREN = ON      // Stack Overflow/Underflow Reset Enable (Stack Overflow or Underflow will cause a Reset)
#pragma config BORV = LO        // Brown-out Reset Voltage Selection (Brown-out Reset Voltage (Vbor), low trip point selected.)
#pragma config LVP = OFF        // Low-Voltage Programming Enable (High-voltage on MCLR/VPP must be used for programming)

// head load timing in ms, indexed by DIP switch setting
// DIP switch 1 (LSB) is on the left when looking at PCB from component
// side with DC37P connector on left.
static const uint16_t hld_delay[16] =
{
    /* 0000 */   20,
    /* 0001 */   30,
    /* 0010 */   40,
    /* 0011 */   60,
    /* 0100 */   80,
    /* 0101 */  100,
    /* 0110 */  150,
    /* 0111 */  250,
    /* 1000 */  350,
    /* 1001 */  450,
    /* 1010 */  550,
    /* 1011 */  650,
    /* 1100 */  750,
    /* 1101 */  850,
    /* 1110 */  950,
    /* 1111 */ 1050
};


// pins        ports   dir     funct   use
// 17,18,1, 2  RA0..3  input           /!DS1..4
// 3           RA4     output  (CCP4)  HLT
// 4           RA5     input   MCLR
// 15          RA6     output  (OSC2)
// 16          RA7     output  (OSC1)
//             RB0     input           HLD
// 7..10       RB1..4  input           DIP switch
// 11          RB5     coutput         to ICD header
// 12          RB6     output  ICDCLK
// 13          RB7     output  ICDDAT

uint8_t prev_hld;
uint8_t prev_ds;

uint16_t hld_counter;

int main(int argc, char** argv)
{
    uint8_t inputs;

    OSCCON = 0x72; // internal HF osc, 8 MHz
                   // inst rate 2 MHz, inst cycle 500ns
    
    OPTION_REG = 0x7f; // clear /WPUEN to allow weak pullups to be enabled
    
    ANSELA = 0x00;
    // unfortunately on port A, only RA5 has a weak pullup
    LATA = 0x00;
    TRISA = 0x2f;
    
    ANSELB = 0x00;
    WPUB   = 0x1f;
    LATB   = 0x00;
    TRISB  = 0x1f;

    // T2 timebase is Fosc/4, which is 2 MHz (500ns))
    // prescaler = 16 (8us), postscaler=1
    TMR2 = 0;
    PR2 = 125; // period 1000us
    T2CON = 0x06;  // timer on, prescaler = 16 (8us), postscaler=1
    PIE1bits.TMR2IE = 1;  // enable TMR2 interrupt

    prev_hld = 0;
    prev_ds  = 0;
    
    hld_counter = 0;

    // enable interrupts
    INTCONbits.PEIE = 1;
    INTCONbits.GIE = 1;
    
    while (true)
    {
        ;
    }
}

void interrupt interrupt_handler(void)
{
    uint8_t new_hld;
    uint8_t new_ds;
    uint8_t dip_switch;
    
    if (! PIR1bits.TMR2IF)
        return;

    PIR1bits.TMR2IF = 0;

    new_hld = PORTBbits.RB0 ^ 1;
    new_ds  = (PORTA & 0x0f) ^ 0x0f;
    
    // read DIP switch, complement so that ON position is 1
    dip_switch = ((PORTB >> 1) & 0x0f) ^ 0x0f;
    
    if ((new_ds != prev_ds) || (! new_hld))
    {
        LATAbits.LATA4 = 0;
        hld_counter = 0;
        prev_hld = 0;
    }
    
    if (new_hld && ! prev_hld)
        hld_counter = hld_delay[dip_switch] + 1;
    
    if (hld_counter)
    {
        if (--hld_counter == 0)
            LATAbits.LATA4 = 1;
    }
    
    prev_hld = new_hld;
    prev_ds = new_ds;
}
