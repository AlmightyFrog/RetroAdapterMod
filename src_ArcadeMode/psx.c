#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include "report.h"
#include "psx.h"
#include "hid_modes.h"

//#define PS2PRESSURE

#define PSXCLK	22	// 22 us from rising edge to falling edge this low clock values ensures that the adapter works with PSX, Dualshock and NegCon without any bitflips and jitter.
#define PSXBYTEDELAY 3 // 3 us between bytes

#ifdef PS2PRESSURE
// ps2 pressure button support, preliminary and wip (not working well yet)
#define PS2CLK 12 // faster clock for PS2 pressure sensitive mode
#define PS2BYTEDELAY 3 // faster clock for PS2 pressure sensitive mode

static uchar enter_config[]={0x01,0x43,0x00,0x01,0x00};
static uchar set_bytes_large[]={0x01,0x4F,0x00,0xFF,0xFF,0x03,0x00,0x00,0x00};
static uchar exit_config[]={0x01,0x43,0x00,0x00,0x5A,0x5A,0x5A,0x5A,0x5A};
#endif

/*	LDRU
	0000	-
	0001	0
	0010	2
	0011	1
	0100	4
	0101	-
	0110	3
	0111	-
	1000	6
	1001	7
	1010	-
	1011	-
	1100	5
*/

const uchar psx_hat_lut[] PROGMEM  = { -1, 0, 2, 1, 4, -1, 3, -1, 6, 7, -1, -1, 5 };
extern	uchar	hidMode;

/*
PB5		DAT		In
PB4		CMD		Out
PB3		ATT		Out
PB2		CLK		Out
PB1		ACK		In
*/

void ReadPSX(report_t *reportBuffer, reportWheel_t *reportBufferWheel, reportAnalogButtons_t *reportBufferAnalogButtons)
{
	uchar	data, id;

	DDRB	&= 0b11011101;			// See table above for I/O directions
	DDRB	|= 0b00011100;
	PORTB	|= 0b00111110;			// Pull-ups prevent floating inputs, all outputs start high

	PORTB &= ~ATT;					// Attention line low for duration of comms
	_delay_us(PSXBYTEDELAY);

	data = PSXCommand(0x01);		// Issue start command, data is discarded
	
	id = PSXCommand(0x42);			// Request controller ID
	
#ifdef PS2PRESSURE
	// pressure sensitive PS2 Button support test, disabled because those buttons are horrible in gameplay
	if (id == PSX_ID_A_RED) //if reported as analog controller, then try to switch to PS2 pressure button mode (id 0x79)
	{	
		PORTB |= ATT;				// ATT high again
		_delay_us(PSXBYTEDELAY);	// wait a few us
		
		PS2SendCommandString(enter_config, sizeof(enter_config));
  		PS2SendCommandString(set_bytes_large, sizeof(set_bytes_large));
  		PS2SendCommandString(exit_config, sizeof(exit_config));
		
		_delay_us(100);
		
  		PORTB &= ~ATT;					// Attention line low for duration of comms
		_delay_us(PSXBYTEDELAY);

		data = PSXCommand(0x01);		// Issue start command, data is discarded
	
		id = PSXCommand(0x42);			// Request controller ID, it could now be supporting pressure buttons if it is indeed a PS2 controller!			
    }
#endif
	if ((id == PSX_ID_DIGITAL) | (id == PSX_ID_A_RED) | (id == PSX_ID_A_GREEN))
	{
		data = PSXCommand(0xff);	// expect 0x5a from controller
		if (data == 0x5a)
		{
			data = PSXCommand(0xff);
			if (!(data & (1<<0))) reportBuffer->b2 |= (1<<1);	// Select
			if (!(data & (1<<3))) reportBuffer->b2 |= (1<<0);	// Start

			if (id == PSX_ID_DIGITAL)
			{
				if (!(data & (1<<4))) reportBuffer->y = -128;		// Up
				if (!(data & (1<<6))) reportBuffer->y = 127;		// Down
				if (!(data & (1<<7))) reportBuffer->x = -128;		// Left
				if (!(data & (1<<5))) reportBuffer->x = 127;		// Right
			}
			else if(id == PSX_ID_A_RED)
			{
				if (!(data & (1<<1))) reportBuffer->b2 |= (1<<2);	// L3 Left joystick
				if (!(data & (1<<2))) reportBuffer->b2 |= (1<<3);	// R3 Right joystick
				reportBuffer->hat = pgm_read_byte(&psx_hat_lut[(~(data>>4)&0x0f)]);
			}

			data = PSXCommand(0xff);
			
			if (!(data & (1<<0))) reportBuffer->b1 |= (1<<4);	// L2
			if (!(data & (1<<1))) reportBuffer->b1 |= (1<<5);	// R2
			if (!(data & (1<<2))) reportBuffer->b1 |= (1<<6);	// L1
			if (!(data & (1<<3))) reportBuffer->b1 |= (1<<7);	// R1
			if (!(data & (1<<4))) reportBuffer->b1 |= (1<<0);	// /\ Triangle
			if (!(data & (1<<5))) reportBuffer->b1 |= (1<<1);	// O  Circle
			if (!(data & (1<<6))) reportBuffer->b1 |= (1<<2);	// X  Cross
			if (!(data & (1<<7))) reportBuffer->b1 |= (1<<3);	// [] Square

			if ((id == PSX_ID_A_RED) | (id == PSX_ID_A_GREEN))
			{
				data = PSXCommand(0xff);
				reportBuffer->rx = -128+(char)data;
				
				data = PSXCommand(0xff);
				reportBuffer->ry = -128+(char)data;
				
				data = PSXCommand(0xff);
				reportBuffer->x = -128+(char)data;
				
				data = PSXCommand(0xff);
				reportBuffer->y = -128+(char)data;				
			}
		}
	}
	if (id==PSX_ID_NEGCON) 
	{
		hidMode = HIDM_WHEEL;
		
		data = PSXCommand(0xff);	// expect 0x5a from controller
		if (data == 0x5a)
		{
			data = PSXCommand(0xff);
			if (!(data & (1<<3))) reportBufferWheel->b2 |= (1<<0);	// Start
			reportBufferWheel->hat = pgm_read_byte(&psx_hat_lut[(~(data>>4)&0x0f)]);

			data = PSXCommand(0xff);
			if (!(data & (1<<3))) reportBufferWheel->b1 |= (1<<7);	// R1
			if (!(data & (1<<4))) reportBufferWheel->b1 |= (1<<0);	// /\ Triangle (A on Negcon)
			if (!(data & (1<<5))) reportBufferWheel->b1 |= (1<<1);	// O  Circle (B on Negcon)

			data = PSXCommand(0xff); //Steering axis 0x00 = left
			//TESTED ON REAL PSX: moving the right half away from you turns the Wipeout vehicle to the RIGHT!
			reportBufferWheel->x = -128+(char)data;
				
			data = PSXCommand(0xff); //I button (bottom button analog)
			reportBufferWheel->slider1 = (char)data;
			
			data = PSXCommand(0xff); //II button (left button analog)
			reportBufferWheel->slider2 = (char)data;

			data = PSXCommand(0xff); //L1 Button analo
			reportBufferWheel->slider3 = (char)data;
		}
	}
#ifdef PS2PRESSURE
	// pressure sensitive PS2 button support, preliminary
	if (id == PSX_ID_PS2_PRESSURE) // use faster clock for ps2
	{
		hidMode = HIDM_ANALOGBUTTONS;
		
		data = PS2Command(0xff);	// expect 0x5a from controller
		if (data == 0x5a)
		{
			data = PS2Command(0xff);
			if (!(data & (1<<0))) reportBufferAnalogButtons->b2 |= (1<<1);	// Select
			if (!(data & (1<<3))) reportBufferAnalogButtons->b2 |= (1<<0);	// Start

			if (!(data & (1<<1))) reportBufferAnalogButtons->b2 |= (1<<2);	// L3 Left joystick
			if (!(data & (1<<2))) reportBufferAnalogButtons->b2 |= (1<<3);	// R3 Right joystick
			reportBufferAnalogButtons->hat = pgm_read_byte(&psx_hat_lut[(~(data>>4)&0x0f)]);

			data = PS2Command(0xff);
			
			if (!(data & (1<<0))) reportBufferAnalogButtons->b1 |= (1<<4);	// L2
			if (!(data & (1<<1))) reportBufferAnalogButtons->b1 |= (1<<5);	// R2
			if (!(data & (1<<2))) reportBufferAnalogButtons->b1 |= (1<<6);	// L1
			if (!(data & (1<<3))) reportBufferAnalogButtons->b1 |= (1<<7);	// R1
			if (!(data & (1<<4))) reportBufferAnalogButtons->b1 |= (1<<0);	// /\ Triangle
			if (!(data & (1<<5))) reportBufferAnalogButtons->b1 |= (1<<1);	// O  Circle
			if (!(data & (1<<6))) reportBufferAnalogButtons->b1 |= (1<<2);	// X  Cross
			if (!(data & (1<<7))) reportBufferAnalogButtons->b1 |= (1<<3);	// [] Square

			data = PS2Command(0xff);
			reportBufferAnalogButtons->rx = -128+(char)data;
				
			data = PS2Command(0xff);
			reportBufferAnalogButtons->ry = -128+(char)data;
				
			data = PS2Command(0xff);
			reportBufferAnalogButtons->x = -128+(char)data;
				
			data = PS2Command(0xff);
			reportBufferAnalogButtons->y = -128+(char)data;				
					
			// here come the pressure sensitive button values
			data = PS2Command(0xff); //byte 9: right
			reportBufferAnalogButtons->slider1 = data;
			
			data = PS2Command(0xff); //byte 10: left
			reportBufferAnalogButtons->slider10 = data;

			data = PS2Command(0xff); //byte 11: up
			reportBufferAnalogButtons->slider11 = data;

			data = PS2Command(0xff); //byte 12: down
			reportBufferAnalogButtons->slider12 = data;
				
			data = PS2Command(0xff); //byte 13: Triangle
			reportBufferAnalogButtons->slider1 = data;
				
			data = PS2Command(0xff); //byte 14: Circle
			reportBufferAnalogButtons->slider2 = data;
				
			data = PS2Command(0xff); //byte 15: Cross
			reportBufferAnalogButtons->slider3 = data;
				
			data = PS2Command(0xff); //byte 16: Square
			reportBufferAnalogButtons->slider4 = data;
				
			data = PS2Command(0xff); //byte 17: L1
			reportBufferAnalogButtons->slider5 = data;
				
			data = PS2Command(0xff); //byte 18: R1
			reportBufferAnalogButtons->slider6 = data;
				
			data = PS2Command(0xff); //byte 19: L2
			reportBufferAnalogButtons->slider7 = data;
				
			data = PS2Command(0xff); //byte 20: R2
			reportBufferAnalogButtons->slider8 = data; 
		}
	}	
#endif
	PORTB |= ATT;				// ATT high again
	_delay_us(PSXBYTEDELAY);
}

uchar PSXCommand(uchar command)
{
	uchar i = 0;
	uchar data = 0;

	_delay_us(PSXBYTEDELAY);

	for (i = 0; i < 8; i++)
	{
		// set command line
		if (command & 1) PORTB |= CMD;
		else PORTB &= ~CMD;
		command >>= 1;
		
		PORTB &= ~CLK;				// clock falling edge this is when data is changed by both host and controller
		
		_delay_us(PSXCLK);
		
		data >>= 1;		

		if (PINB & DAT) data |= (1<<7); //(1<<i); // if this command is done after the next command, there are some random bit flips most notably on NegCon

		PORTB |= CLK;				// clock rising edge this is when data is read by both host and controller
		
		_delay_us(PSXCLK);
	}
	_delay_us(PSXBYTEDELAY);
	
	return data;
}
#ifdef PS2PRESSURE
//faster clock works on ps2 but not on dualshock, so these functions for ps2 use the faster clock
uchar PS2Command(uchar command)
{
	uchar i = 0;
	uchar data = 0;

	_delay_us(PS2BYTEDELAY);

	for (i = 0; i < 8; i++)
	{
		// set command line
		if (command & 1) PORTB |= CMD;
		else PORTB &= ~CMD;
		command >>= 1;
		
		PORTB &= ~CLK;				// clock falling edge this is when data is changed by both host and controller
		
		_delay_us(PS2CLK);
		
		data >>= 1;		

		if (PINB & DAT) data |= (1<<7); //(1<<i);

		PORTB |= CLK;				// clock rising edge this is when data is read by both host and controller
		
		_delay_us(PS2CLK);
	}
	_delay_us(PS2BYTEDELAY);
	
	return data;
}

void PS2SendCommandString(uchar string[], uchar len) 
{
	// low enable joysticks
	PORTB &= ~ATT;
	for (int y=0; y<len; y++)
	{
		PS2Command(string[y]);
	};
	PORTB |= ATT;
	_delay_us(PS2BYTEDELAY);
}
#endif