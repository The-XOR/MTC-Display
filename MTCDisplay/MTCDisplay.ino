// device: Arduino Leonardo
// ATTANSIUN:
// Non serve smontare tuttecose, il programma viene trasmesso via USB senza 
// abbisogna di accedere all'hardware.

#include <MIDIUSB.h>
#include <MIDIUSB_Defs.h>
#include <MIDI.h>
#include "USB-MIDI.h"
#include "DigitLedDisplayEx.h"
//#define SERIAL_DEBUG

#define VERSION " VER 2.0 "

USBMIDI_CREATE_DEFAULT_INSTANCE();

/* Arduino Pin to Display Pin
   7 to DIN,
   6 to CS,
   5 to CLK */
#define DIN 7
#define CS 6
#define CLK 5
#define BANNER "tHE XOR "
#define PPQN	24
#define QUARTER_NOTE  4

#define LED_DIM		10
#define LED_G     16
#define LED_B     15
#define LED_R     14
#define LED_ON	  LOW
#define LED_OFF		HIGH

DigitLedDisplay ld = DigitLedDisplay(8, DIN, CS, CLK);
char toDisp[20];
volatile unsigned long idleTime;
volatile uint8_t idleMode;
volatile unsigned long lastMsg;
volatile uint16_t songPosition;
volatile bool transportRunning;
volatile uint16_t signature_numerator;
volatile uint16_t signature_denominator;
volatile uint16_t ticks_per_measure;
volatile uint16_t ticks_per_beats;

enum
{
	F24 = 0,
	F25 = 2,
	F30DF = 4,
	F30 = 6
}; // Frames type

void handleTimeCodeQuarterFrame(byte data)
{
	static byte tc[8];
	lastMsg = millis();
	int indice = (data & 0xf0) >> 4;
	if (indice > 7)
		return;

	tc[indice] = data & 0x0f;
	if (indice == 7)
	{
		byte frameType = tc[7] & 0x06;
		byte h = (tc[7] & 0x01) << 4 + tc[6];
		byte m = (tc[5] << 4) + tc[4];
		byte s = (tc[3] << 4) + tc[2];
		byte f = (tc[1] << 4) + tc[0];

		if (h > 23)
			h = 23;
		if (m > 59)
			m = 59;
		if (s > 59)
			s = 59;

		byte fmax;
		switch (frameType)
		{
			case F24:
				fmax = 24;
				break;
			case F25:
				fmax = 25;
				break;
			case F30DF:
			case F30:
				fmax = 30;
				break;
			default:
				h = m = s = f = 8; // ERROR!
				break;
		}
		if (f > fmax)
			f = fmax;

		sprintf(toDisp, "%02d.%02d.%02d.%02d", h, m, s, f);
	}
}

void handleClock()
{
	lastMsg = millis();
	if(transportRunning)
	{
		songPosition++;
		updateMidiClockDisplay();
	}
}

void handleStart()
{
	lastMsg = millis();
	songPosition = 0;
	transportRunning = true;
	updateMidiClockDisplay();
}

void handleContinue()
{
	lastMsg = millis();
	transportRunning = true;
}
 
void handleStop()
{
	lastMsg = millis(); 
	transportRunning = false;
}

void handleSongPosition(uint16_t beats)
{
	// il messaggio song position pointer misura i BEAT (sedicesimi di note); quindi beats indica quanti 1/16 di nota sono passati
	/*
	la conversione richiede due passaggi:
	1. Dai sedicesimi ai Quarti
	   L'unità base dell'SPP è il sedicesimo. Poiché una nota da un quarto equivale a 4 note da 1/16, 
		 dobbiamo quadruplicare il valore SPP per portarlo all'unità di misura base del Tick (il quarto). Valore in Quarti = Valore SPP / 4
	2. Dai Quarti ai Ticks
	   Il MIDI Clock standard fornisce 24 impulsi (tick) per ogni nota da un quarto. Moltiplichiamo il risultato precedente per 24.
	   Ticks totali = Valore in Quarti * 24
	   Combinando le due operazioni otteniamo la formula finale:
	   Ticks totali = (Valore SPP/4) * 24
	   Semplificando:
	   Ticks totali = Valore SPP * 6
	*/
	lastMsg = millis();
	songPosition = beats * 6;
	updateMidiClockDisplay();
}

void handleControlChange(byte channel, byte control, byte value)
{
	lastMsg = millis();
	switch(control)
	{
		case 104:	// dim: 0 = off, > 0 = on
			digitalWrite(LED_DIM, value > 0 ? LED_ON : LED_OFF);
			break;

		case 105: // controllo LED RGB:
			// bit 0: led rosso
			// bit 1: led verde
			// bit 2: led blu
			printf("Valore = %d", value);
			digitalWrite(LED_R, (value & 0x01) ? LED_ON : LED_OFF);
			digitalWrite(LED_G, (value & 0x02) ? LED_ON : LED_OFF);
			digitalWrite(LED_B, (value & 0x04) ? LED_ON : LED_OFF);
			break;			

		case 102: // Time Signature: ultimi 3 bit denominatore, 4 bit piu' significativi numeratore
			// 000 = 0 = denom 1
			// 001 = 1 = denom 2
			// 010 = 2 = denom 4
			// 011 = 3 = denom 8
			// 100 = 4 = denom 16
			// 101 = 5 = denom 32
			// 110 = 6 = denom 64
			signature_denominator = 1 << (value & 0x7);	
			signature_numerator = (value >> 3) & 0x0f;
			updateSignature();
			showSignature();
			break;

		case 103:	// CONTROL
		{
			switch(value)
			{
				case 0x00:  // 103 0 = RESET
					initialize();
					break;

				case 0x01:	// 103 1  = SHOW SIGNATURE
					showSignature();
					break;
			}
		}
		break;
	}
}

void showSignature()
{
	ld.clear();
	sprintf(toDisp, "%d / %d", signature_numerator, signature_denominator);
}

void updateSignature()
{
	songPosition = 0;
	ticks_per_beats = PPQN / (signature_denominator / QUARTER_NOTE);
	ticks_per_measure = ticks_per_beats * signature_numerator;
}

void updateMidiClockDisplay()
{
	uint16_t m = 1+(songPosition/ticks_per_measure);
	uint16_t b = 1+(songPosition % ticks_per_measure)/ticks_per_beats;
	uint16_t s = songPosition % ticks_per_beats;
	if(m < 100)
		sprintf(toDisp, "%02u %02u %02u", m, b, s);
	else
		sprintf(toDisp, "%03u.%02u %02u", m, b, s);
}

void clr()
{
	ld.clear();
	strcpy(toDisp, BANNER);
}

void initialize()
{
	clr();	
	signature_numerator = 4;
	signature_denominator = 4;
	updateSignature();
	lastMsg = 0;
	idleTime = millis();
	idleMode = 0;
	transportRunning = false;
}

void setup()
{
	pinMode(LED_DIM, OUTPUT);
	digitalWrite(LED_DIM, LED_OFF); 
	pinMode(LED_R, OUTPUT);
	digitalWrite(LED_R, LED_OFF); 
	pinMode(LED_G, OUTPUT);
	digitalWrite(LED_G, LED_OFF); 
	pinMode(LED_B, OUTPUT);
	digitalWrite(LED_B, LED_OFF); 

	ld.setBright(1);
	ld.clear();
	MIDI.setHandleTimeCodeQuarterFrame(handleTimeCodeQuarterFrame);
	MIDI.setHandleClock(handleClock);
	MIDI.setHandleStart(handleStart);
	MIDI.setHandleContinue(handleContinue);
	MIDI.setHandleStop(handleStop);
 	MIDI.setHandleControlChange(handleControlChange);
	MIDI.setHandleSongPosition(handleSongPosition);

	initialize();
	MIDI.begin(1); // listen channel 1, ma tanto non e' usato
	#ifdef SERIAL_DEBUG
		Serial.begin(115200);
  	Serial.println("Debug abilitato");
	#endif
}

void enterIdle()
{
	idleTime = millis();
	idleMode = 0;
	showIdle();
}

void showIdle()
{
	switch(idleMode)
	{
		case 0:
			clr();
			break;

		case 1:
			showSignature();
			break;

		case 2:
				strcpy(toDisp, VERSION);
				break;
	}
}

void loop()
{
	MIDI.read();
	if(toDisp[0] != 0)
	{
		ld.printString(toDisp);
		toDisp[0] = 0;
	}

	if((millis() - lastMsg) > 60000)		// dopo 60 secondi si va in idle
	{
		if(lastMsg != 0)
			enterIdle();
		lastMsg = 0;
	}

	if(lastMsg == 0) // siamo in idle mode
	{
		if(millis() - idleTime > 10000)  // ogni 10 secondi cambia scritta
		{
			idleTime = millis();
			idleMode = (idleMode + 1) % 3;
			showIdle();
		}
	}
}
