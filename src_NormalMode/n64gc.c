#include <avr/io.h>
#include <util/delay.h>
#include "report.h"
#include "n64gc.h"

//	PB0		-	data

extern	uchar readnintendo(report_t *reportBuffer);

void ReadN64GC(report_t *reportBuffer)
{
	uchar	dpad;

	PORTB	&= ~(1<<0);		// data bus no pull-up
	DDRB	&= ~(1<<0);		// data bus input

	PORTD	&= ~(1<<7);		// debug
	DDRD	|= (1<<7);

	dpad = readnintendo(reportBuffer);

	if (dpad & 0x8) {
		reportBuffer->ry = -127;	// up
	} else if (dpad & 0x4) {
		reportBuffer->ry = 127;		// down
	} else {
		reportBuffer->ry = 0; 		// center
	}

	if (dpad & 0x1) {
		reportBuffer->rx = 127;		// right
	} else if (dpad & 0x2) {
		reportBuffer->rx = -127;	// left
	} else {
		reportBuffer->rx = 0; 		// center
	}
}
