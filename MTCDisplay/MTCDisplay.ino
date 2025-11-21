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
#define PPQN	24
#define QUARTER_NOTE  4

DigitLedDisplay ld = DigitLedDisplay(8, DIN, CS, CLK);
char toDisp[20];
volatile unsigned long lastSMPTETime;
volatile unsigned long lastMsg;
volatile bool midiclock_enabled;
volatile uint16_t songPosition;
volatile bool transportRunning;
volatile uint16_t signature_numerator;
volatile uint16_t signature_denominator;
volatile uint16_t ticks_per_measure;
volatile uint16_t ticks_per_beats;
volatile uint16_t ticks_per_subdivision;

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
	// il messaggio song position pointer misura i BEAT (ottavi di note); quindi beats indica quanti 1/8 di nota sono passati
	/*
	la conversione richiede due passaggi:
	1. Dagli Ottavi ai Quarti
	   L'unità base dell'SPP è l'ottavo. Poiché una nota da un quarto equivale a due note da un ottavo, dobbiamo raddoppiare il valore SPP per portarlo all'unità 
	   di misura base del Tick (il quarto). Valore in Quarti = Valore SPP} / 2
	2. Dai Quarti ai Ticks
	   Il MIDI Clock standard fornisce 24 impulsi (tick) per ogni nota da un quarto. Moltiplichiamo il risultato precedente per 24.
	   Ticks totali = Valore in Quarti} * 24
	   Combinando le due operazioni otteniamo la formula finale:
	   Ticks totali = (Valore SPP/2) * 24
	   Semplificando:
	   Ticks totali = Valore SPP * 12
	*/
	lastMsg = millis();
	songPosition = beats * 12;
	updateMidiClockDisplay();
}

void handleControlChange(byte channel, byte control, byte value)
{
	lastMsg = millis();
	switch(control)
	{
		case 0x58: // Time Signature: ultimi 3 bit denominatore, 4 bit piu' significativi numeratore
			// 000 = denom 1
			// 001 = denom 2
			// 010 = denom 4
			// 100 = denom 8
			// 101 = denom 16
			// 110 = denom 32
			// 111 = denom 64
			signature_denominator << (value & 0x7);	
			signature_numerator = (value >> 3) & 0x0f;
			updateSignature();
			showSignature();
			break;
	}
}

void showSignature()
{
	sprintf(toDisp, ": %d/%d :", signature_numerator, signature_denominator);	
}

void updateSignature()
{
	songPosition = 0;
	ticks_per_beats = PPQN / (signature_denominator / QUARTER_NOTE);
	ticks_per_measure = ticks_per_beats * signature_numerator;
	ticks_per_subdivision = ticks_per_beats / 2;
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
	  	uint16_t pos = songPosition - 1;
		if(pos >= 0)
		{
			uint16_t m = 1+(pos/ticks_per_measure);
			uint8_t b = 1+(pos % ticks_per_measure)/ticks_per_beats;
			uint8_t s = 1+(pos % ticks_per_beats)/ticks_per_subdivision;
			if(m < 100)
				sprintf(toDisp, "%02lu %02lu %02lu", m, b, s);
			else
				sprintf(toDisp, "%03lu.%02lu %02lu", m, b, s);
		}
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
	signature_numerator = 4;
	signature_denominator = 4;
	updateSignature();
	lastSMPTETime = 0;
	lastMsg = millis();
	midiclock_enabled = false;
	transportRunning = false;
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

	bool test_mclock = (millis() - lastSMPTETime > 10000); // se non si riceve un MTC entro 10 secondi, abilita la POSSIBILE riceziun del midiclock. Altrimenti, SMPTE got the prece/dance.	
	if(test_mclock != midiclock_enabled)
	{
		clr();	
		midiclock_enabled = test_mclock;
		if(midiclock_enabled)
			showSignature();
	} 
	
	if((millis() - lastMsg) > 120000) // se non si riceve lo stesso casso di cui sopra per 2 minuti, o ci siamo addormentati, o siamo morti o molto piu' probabilmente ubriachi come la merda
	{
		clr();	
	}		
}
