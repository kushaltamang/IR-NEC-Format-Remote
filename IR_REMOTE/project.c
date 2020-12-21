//Final Project Library
#include <project.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "tm4c123gh6pm.h"
#include "uart0.h"
#include "wait.h"

#define IR_LED_MASK 32// PB5 mask
#define GPI_MASK 4 //pin A2

//each button binary bits converted to decimal
uint16_t remotevalues[] = {0xA2,0x62,0xE2,0x22,0x02,0xC2,0xE0,0xA8,0x90,0x68,0x98,0xB0,0x30,0x18,0x7A,0x10,0x38,0x5A,0x42,0x4A,0x52};


//calculates a number to the power of exponent, used to convert binary data bits to decimal
uint16_t pow(uint8_t base, uint8_t exp)
{
    uint16_t result;
    result = 1;
    while(exp != 0)
    {
        result *= base;
        exp--;
    }
    return result;
}

void initPWM0() // configure the PWM0 (PB5) module for 50% duty cycle
{
    // Enable clocks
    SYSCTL_RCGCPWM_R |= SYSCTL_RCGCPWM_R0;//PWM0
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R1;//Port B
    _delay_cycles(3);

    // Configure PB5 for PWM module
    GPIO_PORTB_DIR_R |= IR_LED_MASK; // make PB5 an output
    GPIO_PORTB_DEN_R &= IR_LED_MASK; //disable data on PB5
    GPIO_PORTB_DR2R_R |= IR_LED_MASK; // set drive strength to 2
    GPIO_PORTB_AFSEL_R |= IR_LED_MASK;// select auxilary function
    GPIO_PORTB_PCTL_R &= GPIO_PCTL_PB5_M;// enable PWM
    GPIO_PORTB_PCTL_R |= GPIO_PCTL_PB5_M0PWM3;

    // Configure PWM module 0 to drive IR_LED on M0PWM3 (PB5), M0PWM1b
    SYSCTL_SRPWM_R = SYSCTL_SRPWM_R0; // reset PWM0 module
    SYSCTL_SRPWM_R = 0;// leave reset state
    PWM0_1_CTL_R = 0;  // turn-off PWM0 generator 1
    PWM0_1_GENB_R = PWM_0_GENB_ACTCMPBD_ZERO | PWM_0_GENB_ACTLOAD_ONE;// output 3 on PWM0, gen 1b, cmpb
    PWM0_1_LOAD_R = 1050;// set period to 40 MHz sys clock / 2 / 526 = 38.02 kHz
    PWM0_INVERT_R = PWM_INVERT_PWM3INV;// invert outputs so duty cycle increases with increasing compare values
    PWM0_1_CMPB_R = 525;// set for 50 % duty cycle
    PWM0_1_CTL_R = PWM_0_CTL_ENABLE;// turn-on PWM0 generator 1
    PWM0_ENABLE_R = PWM_ENABLE_PWM3EN;// enable outputs

}

//initialize TIMER2 interrupt
void initHw1()
{
    // Enable clock for Timer 2
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R0;// Port A clock
    SYSCTL_RCGCTIMER_R |= SYSCTL_RCGCTIMER_R2; //Enable clocks for the Timer2
    _delay_cycles(3);

    //Timer2 Interrupt
    TIMER2_CTL_R &= ~TIMER_CTL_TAEN;                 // turn-off timer before reconfiguring
    TIMER2_CFG_R = TIMER_CFG_32_BIT_TIMER;           // configure as 32-bit timer
    TIMER2_TAMR_R = TIMER_TAMR_TAMR_PERIOD;          // configure for periodic mode (count down)
    NVIC_EN0_R |= 1 << (INT_TIMER2A-16);             // call interrupt 39 on NVIC when (TIMER2A) interrupt occurs
}

//configure edge-triggered interrupt on the GPI(PA2) pin and sets up timer1A interrupt
void initInterrupts()
{
    //Enable clocks for the timer1
    SYSCTL_RCGCTIMER_R |= SYSCTL_RCGCTIMER_R1;
    _delay_cycles(3);

    //Edge Triggered Interrupt
    GPIO_PORTA_IS_R &= ~GPI_MASK;  //edge mode
    GPIO_PORTA_IBE_R &= ~GPI_MASK; //single edge
    GPIO_PORTA_IEV_R &= ~GPI_MASK; //falling edge
    GPIO_PORTA_ICR_R |= GPI_MASK;
    NVIC_EN0_R |= 1 << (INT_GPIOA-16); //call interrupt 16 on NVIC vector table when interrupt occurs

    //Timer Interrupt
    TIMER1_CTL_R &= ~TIMER_CTL_TAEN;                 // turn-off timer before reconfiguring
    TIMER1_CFG_R = TIMER_CFG_32_BIT_TIMER;           // configure as 32-bit timer
    TIMER1_TAMR_R = TIMER_TAMR_TAMR_PERIOD;          // configure for periodic mode (count down)
    NVIC_EN0_R |= 1 << (INT_TIMER1A-16);             // call interrupt 37 on NVIC when (TIMER1A) interrupt occurs

    GPIO_PORTA_IM_R |= GPI_MASK; //turn on GPI ET interrupt
}

//prints the remote button number pressed into UART
void putvalueintoUart0(uint16_t decinum,bool goodtone,bool badtone)
{
    char c;
    uint8_t i;
    uint8_t val;
    val = 0;
    for(i=0;i<21;i++) // check decinum with all the decoded remote buttons
    {
        if((decinum >>8) == remotevalues[i])//change remotevalues to hex 0xA2 and compare decinum >> 8
        {
            //GPIO_PORTF_DATA_R |= BLUE_LED_MASK; // toggle BLUE LED for debug
            val = i+1;
        }
    }
    if (val == 0)
    {
        putcUart0('0');
        //play a tone for bad IR command
        if(badtone)
        {
            GPIO_PORTB_DEN_R |= IR_LED_MASK;
            PWM0_1_LOAD_R = 45454;
            PWM0_1_CMPB_R = 22727;
            waitMicrosecond(50000);
            PWM0_1_LOAD_R = 45454;
            PWM0_1_CMPB_R = 22727;
            waitMicrosecond(50000);
        }
     }
    else
    {
        putsUart0("address: ");
        putsUart0("00 ");
        putsUart0("data: ");
        printhexdata(decinum >> 8); // to debug, prints databits

        // play a tone for good IR command
        if(goodtone)
        {
            GPIO_PORTB_DEN_R |= IR_LED_MASK;
            PWM0_1_LOAD_R = 38251;
            PWM0_1_CMPB_R = 19120;
            waitMicrosecond(50000);
        }
        /*
        if(val == 10) putsUart0("10");
        else if(val == 11) putsUart0("11");
        else if(val == 12) putsUart0("12");
        else if(val == 13) putsUart0("13");
        else if(val == 14) putsUart0("14");
        else if(val == 15) putsUart0("15");
        else if(val == 16) putsUart0("16");
        else if(val == 17) putsUart0("17");
        else if(val == 18) putsUart0("18");
        else if(val == 19) putsUart0("19");
        else if(val == 20) putsUart0("20");
        else if(val == 21) putsUart0("21");
        else
         {
            c = val + '0';
            putcUart0(c);
         }
         */
    }
    putcUart0('\n');
    GPIO_PORTB_DEN_R &= ~IR_LED_MASK;
}

void getsUart0(USER_DATA* data)
{
    int count; // count the no of characters received from the terminal
    char c; // char received from UART
    bool loop = true;
    count = 0;
    while(loop)
    {
        c = getcUart0(); // get a char from UART0
        if(c == 8 | c ==127 ) // check if the char received is a backspace
        {
            if(count > 0)
            {
                count--;
            }
        }
        else // char received is not a backspace
        {
            if(c==13) // char is a carriage return <ENTER KEY>
            {
                data->buffer[count] = NULL; // indicate the end of the string
                return; // return from the function
            }
            else // char is not an enter key
            {
                if(c >= 32) // char c is a printable character
                {
                    data->buffer[count] = c; // store character in the buffer
                    count++; // increment the count
                    if(count == MAX_CHARS) // no more room in the buffer
                    {
                        // terminate the string and return from the function
                        data->buffer[count] = NULL;
                        return;
                    }
                }
            }
         }
    }
return; // never gets out of the while loop
}

void parseFields(USER_DATA* data)
{
    data->fieldCount = 0;
    bool transition = true;
    //find index and type of the important part excluding delimiters in the buffer
    uint8_t i,j;
    j=0;
    for(i=0;i<MAX_CHARS+1;i++)
    {
        //return if <ENTER KEY>/NULL is found
        if(data->buffer[i] == 13 || data->buffer[i] == NULL)
        {
            return;
        }

        //char is an alphabet
        if((data->buffer[i] >= 'a' && data->buffer[i] <= 'z') || (data->buffer[i] >= 'A' && data->buffer[i] <= 'Z'))
        {
            if(transition) //if last char was a delimiter
            {
                data->fieldPosition[j] = i;
                data->fieldType[j] = 'a';
                (data->fieldCount)++;
                transition = false;
                j++;
            }
        }
        //char is a number
        else if((data->buffer[i] >= '0' && data->buffer[i] <= '9'))
        {
            if(transition) //if last char was a delimiter
            {
                data->fieldPosition[j] = i;
                data->fieldType[j] = 'n';
                (data->fieldCount)++;
                transition = false;
                j++;
            }
        }
        //char is '+' or '-' sign
        else if(data->buffer[i] == '-' | data->buffer[i] == '+')
        {
            if(transition) //if last char was a delimiter
            {
                 data->fieldPosition[j] = i;
                 data->fieldType[j] = 'n';
                 (data->fieldCount)++;
                  transition = false;
                  j++;
            }
        }
        //char is a delimiter
        else
        {
            data->buffer[i] = NULL; //convert delimiter to NULL
            transition = true;
        }
        //return if fieldCount = MAX_FIELDS
        if(data->fieldCount == MAX_FIELDS)
        {
            return;
        }
    }
}
//return the value of a field requested if the field number is in range
char* getFieldString(USER_DATA* data, uint8_t fieldNumber)
{
    if(fieldNumber <= data->fieldCount)
    {
        return &(data->buffer[data->fieldPosition[fieldNumber]]);
    }
    else
    {
        return NULL;
    }
}

//returns true if the command matches the first field and the number of arguments (excluding the command field) is greater than or equal to the requested number of minimum arguments.
bool isCommand(USER_DATA* data, const char strCommand[],uint8_t minArguments)
{
    if((data->fieldCount)-1 >= minArguments)
    {
        int i;
        i = 0;
        uint8_t indexofbufferchar = data->fieldPosition[0];
        while(data->buffer[indexofbufferchar] != '\0' || strCommand[i] !='\0')
        {
            //putcUart0(strCommand[i]);
            //putcUart0(data->buffer[indexofbufferchar]);
            if(strCommand[i] == data->buffer[indexofbufferchar])
            {
                i++;
                indexofbufferchar++;
            }
            else
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }
    return true;
}

void printhexdata(uint32_t data)
{
    uint8_t i = 0;
    uint32_t p;
    while(i<2)
    {
        if(i==0) p = data >> 4;
        else p = data & 0x0000000F;
        if(p<10) putcUart0(p+48);
        else
        {
            putcUart0(p+55);
        }
        i++;
    }
}


