//Final Project Library
#ifndef PROJECT_H_
#define PROJECT_H_

#include <stdint.h>
#include <stdbool.h>

#define MAX_CHARS 80
#define MAX_FIELDS 5 //max args
#define DELIMITERS_SIZE 4

//structure that holds user data
typedef struct _USER_DATA
{
    char buffer[MAX_CHARS+1];
    uint8_t fieldCount;//argument count
    uint8_t fieldPosition[MAX_FIELDS]; //index
    char fieldType[MAX_FIELDS];//type
}USER_DATA;

uint16_t pow(uint8_t base, uint8_t exp);//calculates power of a number
void initPWM0();// configure the PWM0 (PB5) module for 50% duty cycle
void initHw1();//initialize TIMER2 interrupt
void initInterrupts();//configure edge-triggered interrupt on the GPI(PA2) pin and sets up timer1A interrupt
void putvalueintoUart0(uint16_t decinum,bool goodtone, bool badtone);//prints the remote button number pressed into UART and plays good or bad tone
void getsUart0(USER_DATA* data);
void parseFields(USER_DATA* data);
char* getFieldString(USER_DATA* data, uint8_t fieldNumber);
bool isCommand(USER_DATA* data, const char strCommand[],uint8_t minArguments);
void printhexdata(uint32_t data);

#endif /* PROJECT_H_ */
