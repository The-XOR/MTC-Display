// device: Arduino Leonardo

#include <MIDIUSB.h>
#include <MIDIUSB_Defs.h>
#include <MIDI.h>
#include "USB-MIDI.h"
#include "DigitLedDisplayEx.h"
//#define SERIAL_DEBUG

USBMIDI_CREATE_DEFAULT_INSTANCE();

/* Arduino Pin to Display Pin
   7 to DIN,
   6 to CS,
   5 to CLK */
#define DIN 7
#define CS 6
#define CLK 5
#define BANNER "tHE XOR "

DigitLedDisplay ld = DigitLedDisplay(8, DIN, CS, CLK);
char toDisp[20];
volatile unsigned long lastSMPTETime;
volatile unsigned long lastMsg;
volatile bool midiclock_enabled;
volatile uint16_t songPosition;
volatile bool transportRunning;
volatile uint16_t signature_numerator;
volatile uint16_t signature_denominator;

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
	lastMsg = lastSMPTETime = millis();
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

void handleReset()
{
	initialize();
}

void handleSongPosition(uint16_t beats)
{
	lastMsg = millis();
	songPosition = beats * 6;
	updateMidiClockDisplay();
}

void handleControlChange(byte channel, byte control, byte value)
{
	if(midiclock_enabled)
	{
		lastMsg = millis();
		switch(control)
		{
			case 0x59: // Time Signature: denominatore
				if(value > 0 && value <= 16)
				{
					signature_denominator = value; 
					songPosition = 0;
					sprintf(toDisp, "S: %d/%d", signature_numerator, signature_denominator);	
				}
				break;

			case 0x58: // Time Signature: numeratore
				if(value > 0 && value <= 16)
				{
					signature_numerator = value; // Numerator
					songPosition = 0;
					sprintf(toDisp, "S: %d/%d", signature_numerator, signature_denominator);	
				}
				break;
		}
	}
}

void handleProgramChange(byte channel, byte program) 
{
	if(program == 127)  // PC 127 ---> reset generale (cosi', a gradire, se dovesse servire...)
		initialize();
}

void updateMidiClockDisplay()
{
	if(midiclock_enabled)
	{
		// Compute clocks per beat and per measure using the current time signature.
		// MIDI has 24 clocks per quarter note, so clocks per beat = 96 / denominator.
	  	
		uint16_t total_beats = (songPosition / 24) * signature_denominator);
		uint16_t m = 1+(total_beats / signature_numerator);

		uint16_t clocks_per_beat = 96 / signature_denominator; // 96 = 24 * 4
		if(clocks_per_beat == 0) 
			return;

		uint16_t clocks_per_measure = clocks_per_beat * signature_numerator;
		if(clocks_per_measure == 0) 
			return;

		uint16_t rem = songPosition % clocks_per_measure
		uint16_t b = rem / clocks_per_beat;
		uint16_t s = rem % clocks_per_beat;
		if(m < 100)
			sprintf(toDisp, "%02lu %02lu %02lu", m, b+1, s);
		else
			sprintf(toDisp, "%03lu.%02lu %02lu", m, b+1, s);
	}
}

void clr()
{
	memset(toDisp, 0, sizeof(toDisp));
	strcpy(toDisp, BANNER);
}

void initialize()
{
	clr();	
	lastSMPTETime = 0;
	lastMsg = millis();
	midiclock_enabled = false;
	transportRunning = false;
	songPosition = 0;
	signature_numerator = 4;
	signature_denominator = 4;
}

void setup()
{
	ld.setBright(1);
	ld.clear();
	MIDI.setHandleTimeCodeQuarterFrame(handleTimeCodeQuarterFrame);
	MIDI.setHandleClock(handleClock);
	MIDI.setHandleStart(handleStart);
	MIDI.setHandleContinue(handleContinue);
	MIDI.setHandleStop(handleStop);
	MIDI.setHandleReset(handleReset);
  	MIDI.setHandleProgramChange(handleProgramChange);
  	MIDI.setHandleControlChange(handleControlChange);
	MIDI.setHandleSongPosition(handleSongPosition);

	initialize();
	MIDI.begin(1); // listen channel 1, ma tanto non e' usato
	#ifdef SERIAL_DEBUG
	Serial.begin(115200);
  	Serial.println("Debug abilitato");
	#endif
}

void loop()
{
	MIDI.read();
	if(toDisp[0])
	{
		ld.printString(toDisp);
		memset(toDisp, 0, sizeof(toDisp));
	}

	bool test_mclock = (millis() - lastSMPTETime > 10000); // se non si riceve un casso entro 10 secondi, abilita la POSSIBILE riceziun del midiclock. Altrimenti, SMPTE got the prece/dance.	
	if(test_mclock != midiclock_enabled)
	{
		clr();	
		midiclock_enabled = test_mclock;
	} 
	
	if((millis() - lastMsg) > 1200000) // se non si riceve lo stesso casso di cui sopra per 20 minuti, o ci siamo addormentati, o siamo morti o molto piu' probabilmente ubriachi come la merda
	{
		clr();	
	}		
}
