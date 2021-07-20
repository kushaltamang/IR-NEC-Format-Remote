//MOHIT_TAMANG
//Universal Remote Controller
//All Rights Reserved
#define GPI (*((volatile uint32_t *)(0x42000000 + (0x400043FC-0x40000000)*32 + 2*4)))

#define IR_LED_MASK 32// PB5 mask
#define RED_LED_MASK 2
#define BLUE_LED_MASK 4
#define GPI_MASK 4 //pin A2
#define GPO_MASK 8 //pin A3

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "uart0.h"
#include "project.h"
#include "wait.h"
#include "eeprom.h"
#include "tm4c123gh6pm.h"

//global varialbes
uint8_t address_g;
uint8_t data_g;
uint8_t modulationidx;
uint8_t bit1_counter;
uint8_t index_g; // for address bits (index_g: 0-7 ) and data bits (index_g:8-15)
bool firsttime;
uint16_t decinum;

char* str;

//timer interrupt loads the values to set up the count-down timer from this table
double timetable[] = {4,5,6,11,12,13.781,14.343,14.905,15.467,16.029,16.591,17.153,17.715,18.277,18.838,19.401,19.963,20.525,21.087,21.649,22.211,
                       22.773,23.335,23.897,24.459,25.021,25.583,26.145,26.707,27.269,27.831,28.393,28.955,29.517,30.079,30.641,31.203,31.765,32.327,32.889,33.451,34.013,34.575,35.137,35.699,36.261,36.823,37.385,37.947,38.509,39.071,39.633,40.195,
                        40.557,41.119,41.681,42.243,42.805,43.367,43.929,44.491,45.053,45.615,46.177,46.739,47.301,47.863,48.425,48.987,49.549,50.111,50.673,51.235,51.797,52.359,52.921,53.483,54.045,54.607,55.169,55.731,56.293,56.805,57.417,57.979,58.541,59.103,59.665,60.227,60.789,61.251,61.913,62.475,63.037,63.499,64.061,64.623,65.185,65.747,66.409,66.771,67.333,67.895};

//to check the first 53 values received that should be exact match (   13.5 ms (5) + address and address inverse (48)  )
uint8_t SensorInput[53] = {0,0,0,1,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,1,1,0,1,1,1,0,1,1,1,0,1,1,1,0,1,1,1,0,1,1,1,0,1,1,1,0,1,1,1};

//this is where  data and data inverse bits goes
char databits[48];

//this is the actual data received after all processing is done
//char outputsignalbits[16];
uint8_t counter;


void initHw()
{
    // Configure HW to work with 16 MHz XTAL, PLL enabled, sysdivider of 5, creating system clock of 40 MHz
    SYSCTL_RCC_R = SYSCTL_RCC_XTAL_16MHZ | SYSCTL_RCC_OSCSRC_MAIN | SYSCTL_RCC_USESYSDIV | (4 << SYSCTL_RCC_SYSDIV_S);

    // Set GPIO ports to use APB (not needed since default configuration -- for clarity)
    SYSCTL_GPIOHBCTL_R = 0;

    // Enable clocks on GPIO ports that we will use
    // for the GPI pin, lets use PA2
    //for GPO pin, lets use PA3
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R0;//Turn on port A
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R5; //turn on port F
    _delay_cycles(3);

    // ******** Configure RED & BLUE LED ************//
    GPIO_PORTF_DIR_R |= RED_LED_MASK | BLUE_LED_MASK;
    GPIO_PORTF_DR2R_R |= RED_LED_MASK | BLUE_LED_MASK;
    GPIO_PORTF_DEN_R |= RED_LED_MASK | BLUE_LED_MASK;
    //********************************************//

    //configure PA2 for GPI and PA3 for GPO
    GPIO_PORTA_DIR_R &= ~GPI_MASK; //bit 2 of port A i.e, PA2 as input
    GPIO_PORTA_DIR_R |= GPO_MASK; //bit 3 of port A i.e, PA3 as output

    GPIO_PORTA_DR2R_R |= GPO_MASK; // set drive strength to 2mA (not needed since default configuration -- for clarity)

    //enable data on PA2 and PA3
    GPIO_PORTA_DEN_R |= GPI_MASK|GPO_MASK;
}

//returns the 48 data bits converted to a decimal number
//also does the processing to convert the 48 bits data and its inverse to corresponding signal bits i.e. '01' = 0 & '0111' = 1
uint16_t databitstoDecimal(char databits[])
{
    uint8_t i;
    uint8_t j;
    uint8_t onecounter;
    uint16_t decinum;
    j = 0;
    decinum = 0;
    for(i=0;i<48;i++)
    {
        if (databits[i]=='0')
        {
            onecounter = 0;
            continue;
        }

        if (databits[i]=='1')
        {
            onecounter++;
        }

        if(onecounter==1)
        {
            //outputsignalbits[j] = '0';
            j++;
        }
        else if(onecounter == 2)
        {
            j--;
        }
        else if(onecounter == 3)
        {
            //outputsignalbits[j] = '1';
            decinum += pow(2,15-j);
            j++;
        }
        else
        {
            continue;
        }
    }
    return decinum;
}


void edgetriggeredIsr()
{
    counter = 0;
    TIMER1_IMR_R = TIMER_IMR_TATOIM;//turn on timer1 interrupt
    GPIO_PORTA_DATA_R ^= GPO_MASK; //toggle the GPO pin on when ETI occurs
    GPIO_PORTA_IM_R &= ~GPI_MASK; //turn off ETI on GPI pin

    TIMER1_TAILR_R = timetable[0]*40000; //load timetables's first value to the timer
    TIMER1_CTL_R |= TIMER_CTL_TAEN; // turn on timer to start the count-down

    GPIO_PORTA_ICR_R |= GPI_MASK; //clear the interrupt flag on GPI pin
}

void timer1Isr()
{
    //check if the sample value does not match with SensorInput values (first 53 values before receiving databits which should be an exact match)
    //if it does not match, stop here and do not receive the data bits
    if(GPI != SensorInput[counter] && counter <53)
    {
        //GPIO_PORTF_DATA_R |= RED_LED_MASK; // toggle RED LED
        return;
    }
    //else, all the 53 values are exact match, so start receiving the data bits
    else
    {
        counter++;
        TIMER1_TAILR_R = (timetable[counter]-timetable[counter-1])*40000; //load delta(t)
        // when counter > 53, we start receiving the data bits
        if(counter>53 && counter < 102)
        {
            if(GPI==0)
            {
                databits[counter-54] = '0';
                //putcUart0(databits[counter-54]);

            }
            else
            {
                databits[counter-54] = '1';
                //putcUart0(databits[counter-54]);
            }
        }
        TIMER1_ICR_R = TIMER_ICR_TATOCINT; // clear Timer's interrupt flag
        GPIO_PORTA_DATA_R ^= GPO_MASK; //toggle the GPO bit
    }
}

//processes the address and data bits
//returns true if bit is 1 and false if bit is 0
bool getval()
{
    bool val;
    if(index_g < 8) val = (address_g & (128 >> index_g));
    else if(index_g >= 8 && index_g <16) val = !(address_g & (128 >> (index_g-8)));
    else if(index_g >=16 && index_g <24) val = (data_g & (128 >> (index_g-16)));
    else if(index_g >=24 && index_g <32) val = !(data_g & (128 >> (index_g-24)));
    return val;
}

//modulates the PWM0 signal using DEN bit to match the IR signal sent by pressing a remote button
void timer2Isr()
{
    TIMER2_ICR_R = TIMER_ICR_TATOCINT; // clear interrupt flag

    //when modulation is done
    if(index_g >= 32)
    {
        TIMER2_IMR_R &= ~TIMER_IMR_TATOIM;// turn off timer2 interrupt
        GPIO_PORTB_DEN_R &= ~IR_LED_MASK; // disable data
        TIMER2_CTL_R &= ~TIMER_CTL_TAEN; // turn off the countdown

        // Reset variables
        bit1_counter = 0; index_g = 0; firsttime = true;
        return;
    }

    if (modulationidx == 0)
    {
        GPIO_PORTB_DEN_R |= IR_LED_MASK; //enable PB5 data register
        TIMER2_TAILR_R = 360000; //load time 9ms
        TIMER2_CTL_R |= TIMER_CTL_TAEN; // turn on timer2 to start the count-down
    }

    else if(modulationidx == 1)
    {
        GPIO_PORTB_DEN_R &= ~IR_LED_MASK;//disable PB5 data
        TIMER2_TAILR_R = 180000; //load time 4.5ms
    }

    else
    {
       if(!getval()) //if bit is 0
       {
           if(firsttime)
           {
               GPIO_PORTB_DEN_R |= IR_LED_MASK; // enable data
               TIMER2_TAILR_R = 22480; // load time 0.562ms
               firsttime = false;
           }
           else
           {
               GPIO_PORTB_DEN_R &= ~IR_LED_MASK; // disable data
               TIMER2_TAILR_R = 22480; // load time 0.562ms
               firsttime = true;
               index_g++;//go to next bit
           }
       }
       else if(getval()) // if bit is 1
       {
           if(firsttime)
           {
               GPIO_PORTB_DEN_R |= IR_LED_MASK; // enable data
               TIMER2_TAILR_R = 22480; //load .562ms
               firsttime = false;
           }
           else
           {
               GPIO_PORTB_DEN_R &= ~IR_LED_MASK; // disable data
               TIMER2_TAILR_R = 67440;//load 3*.562ms
               firsttime = true;
               index_g++;//go to next bit
           }
       }
    }
    modulationidx++;
}


void playCommand(uint8_t address, uint8_t data)
{
    modulationidx = 0; bit1_counter = 0; index_g = 0; firsttime = true; // set the variables
    address_g = address;
    data_g = data;

    TIMER2_TAILR_R = 0;
    TIMER2_IMR_R = TIMER_IMR_TATOIM;//turn on timer2 interrupt
    TIMER2_CTL_R |= TIMER_CTL_TAEN; // turn on timer2 to start the count-down
 }

int main(void)
{
    bool info = false; bool decode = false; bool learn = false; bool list = false; bool erase = false;bool play= false;bool alert = false;bool commandtyped = true; bool goodtone = false; bool badtone = false;bool playtone = false;
    uint32_t commandname; uint32_t commanddata;uint32_t datatobestored; uint8_t eepromaddress = 0; uint8_t fieldnum = 0;
    USER_DATA data; // the user data structure with buffer, field count, field position, and field type
    initHw(); //configures REDBOARD
    initHw1();//initialize TIMER2 interrupt
    initUart0(); // configures UART0
    initEeprom();
    setUart0BaudRate(115200, 40e6); // sets data-transmission rate of UART0
    putsUart0("*******REMOTE CONTROLLER*********\n");
    while(true)
    {
        if(commandtyped)
        {
            if(play && (fieldnum >= 1))
            {
                putsUart0(" ");

            }
            else
            {
                putcUart0('\n');
                getsUart0(&data);//receives chars from the user interface, processing special characters such as backspace, and writes resultant string into the buffer
                parseFields(&data);
                if(isCommand(&data,"decode",0)) decode = true;
                if(isCommand(&data,"learn",1)) learn = true;
                if(isCommand(&data,"info",1)) info = true;
                if(isCommand(&data,"list",1)) list = true;
                if(isCommand(&data,"erase",1)) erase = true;
                if(isCommand(&data,"play",1)) play = true;
                if(isCommand(&data,"alert",1)) alert = true;
            }

            if(decode | learn | play)//if decode or learn dont have to initpwm0
            {
                initInterrupts(); // starts the interrupts
                initPWM0(); //configure PWM0 aux function module for 50% duty cycle
                commandtyped = false;
             }
            if(info|erase|list|play)
             {
                 int i; char* x;
                 fieldnum = data.fieldCount - 1;
                 if(play && (fieldnum >= 1))
                 {
                     x = getFieldString(&data,fieldnum);
                 }
                 else x = getFieldString(&data,1);
                 for(i=eepromaddress; i>0; i=i-3)
                 {
                     commandname = readEeprom(i-3);//gets the name of the command
                     commanddata = readEeprom(i-1);//gets the valid/invalid , address and data bit

                     if(strcmp(x,(char*)&(commandname))==0)//compares command name to the user input in UART
                     {
                         if((commanddata >> 16) == 1)// valid/invalid check
                         {
                             if(info)
                             {
                                 putsUart0("address: ");
                                 putsUart0("00 ");
                                 putsUart0("data: ");
                                 printhexdata(commanddata & 0x000000FF);
                                 putsUart0("\n");
                             }
                             if(erase)
                             {
                                 writeEeprom(i-1,commanddata & 0xFF00FFFF);
                             }
                             if(play)
                             {
                                 uint32_t datatobeplayed = commanddata & 0x000000FF;
                                 playCommand(0x00,datatobeplayed);
                                 playtone = true;
                             }
                         }
                     }
                     if(list && strcmp(x,"commands")==0)
                     {
                         if((commanddata >> 16) == 1)// valid/invalid check
                         {
                             putsUart0((char*)&(commandname));
                             putsUart0(" address: ");
                             putsUart0("00 ");
                             putsUart0("data: ");
                             printhexdata(commanddata & 0x000000FF);
                             putcUart0('\n');
                         }
                     }
                 }
                 info = false; erase = false;list = false;
             }
             if(alert)
             {
                 if(strcmp(getFieldString(&data,1),"good")==0)
                 {
                     if(strcmp(getFieldString(&data,2),"on")==0)
                     {
                         goodtone = true;
                         //putsUart0("ALERT GOOD ON");
                     }
                     if(strcmp(getFieldString(&data,2),"off")==0)
                     {
                         goodtone = false;
                     }

                 }
                 else if(strcmp(getFieldString(&data,1),"bad")==0)
                 {
                     if(strcmp(getFieldString(&data,2),"on")==0)
                     {
                         badtone = true;
                     }
                     if(strcmp(getFieldString(&data,2),"off")==0)
                     {
                         //putsUart0("ALERT BAD OFF");
                         badtone = false;
                     }
                 }
                 alert = false;
             }

        }
        decinum = 0;
        if(counter == 103) //when counter == 103, all the data bits are received
        {
            TIMER1_IMR_R &= ~TIMER_IMR_TATOIM;//turn off the timer interrupt
            counter = 0; //reset the counter
            decinum = databitstoDecimal(databits); //convert the databits to a decimal number
            if(learn)
            {
                uint32_t setvalid = 0x00010000;
                datatobestored = decinum >> 8;
                str = getFieldString(&data,1);
                writeEeprom(eepromaddress,*((uint32_t*)&str[0]));//writes the "name" to address 0
                eepromaddress++;
                writeEeprom(eepromaddress,*((uint32_t*)&str[4]));
                eepromaddress++;
                writeEeprom(eepromaddress,datatobestored | setvalid);//writes the valid/invalid, address, data (eg:0x000100A2) to address 2
                commandname = readEeprom(0); //reads the name of the command stored at address 0
                commanddata = readEeprom(2); //reads the command data at address 2
                eepromaddress++;
            }
            if ((playtone && fieldnum >= 1) | decode) putvalueintoUart0(decinum,goodtone,badtone); //print the remote button number into UART0
            //GPIO_PORTA_ICR_R |= GPI_MASK; //clear the interrupt flag on GPI pin
            //GPIO_PORTA_IM_R |= GPI_MASK; //turn on GPI ET interrupt for another remote button signal
            commandtyped = true; learn = false; decode = false; playtone = false;
            if(play && (fieldnum >= 1))
            {
                play = true;
                data.fieldCount = data.fieldCount - 1;
            }
            else play = false;
        }
    }
}
