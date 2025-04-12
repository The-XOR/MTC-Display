// device: Arduino Leonardo

#include <MIDIUSB.h>
#include <MIDIUSB_Defs.h>
#include <MIDI.h>
#include "USB-MIDI.h"
#include "DigitLedDisplayEx.h"

USBMIDI_CREATE_DEFAULT_INSTANCE();


/* Arduino Pin to Display Pin
   7 to DIN,
   6 to CS,
   5 to CLK */
#define DIN 7
#define CS 6
#define CLK 5   
DigitLedDisplay ld = DigitLedDisplay(8, DIN, CS, CLK);
char toDisp[30];

enum { F24 = 0, F25 = 2, F30DF = 4, F30 = 6 }; // Frames type
void handleTimeCodeQuarterFrame(byte data)
{
    static byte tc[8];   
    int indice = (data & 0xf0)>>4;
    if (indice>7)
      return;

    tc[indice]= data & 0x0f;
    if (indice==7)
    {      
      byte frameType = tc[7] & 0x06;
      byte h = (tc[7] & 0x01)<<4 + tc[6];
      byte m = (tc[5]<<4)+tc[4];
      byte s = (tc[3]<<4)+tc[2];
      byte f = (tc[1]<<4)+tc[0];

      if (h>23)  h = 23;
      if (m>59)  m = 59;
      if (s>59)  s = 59;

      byte fmax;
      switch(frameType)
      {
        case F24: fmax=24; break;
        case F25: fmax=25; break;
        case F30DF: 
        case F30: fmax=30; break;
        default:
          h = m = s = f = 8;  //ERROR!
          break;
      }
      if (f>fmax)  f = fmax;

      sprintf(toDisp, "%02d.%02d.%02d.%02d", h, m, s, f);
    }  
}

void setup()
{
  ld.setBright(1);
  ld.clear();
  strcpy(toDisp, "!thExOR!");
  MIDI.setHandleTimeCodeQuarterFrame(handleTimeCodeQuarterFrame);
  MIDI.begin(1);    //listen channel 1, ma tanto non e' usato    
}

void loop()
{
  MIDI.read();
  if(toDisp[0])
  {
    ld.printString(toDisp);
    toDisp[0]=0;
  }
}
