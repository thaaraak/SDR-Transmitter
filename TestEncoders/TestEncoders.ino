#include "Bounce2.h"
#include "Encoder.h"

Encoder *encFrequency;
Encoder *encMenu;

void setup() 
{

  Serial.begin(115200);
  while(!Serial);

  encFrequency = new Encoder(13, 14);
  encMenu = new Encoder(15, 2);

}

long oldFrequency = -999;
long oldMenu = -999;

void loop() 
{
  char buf[30];
  bool freqchanged;
  bool menuchanged;
  
  freqchanged = readEncoder( encFrequency, oldFrequency );
  menuchanged = readEncoder( encMenu, oldMenu );

  if ( freqchanged )
  {
    sprintf( buf, "Freq: %d", oldFrequency );
    Serial.println( buf );
  }

  if ( menuchanged )
  {
    sprintf( buf, "Menu: %d", oldMenu );
    Serial.println( buf );
  }
}

bool readEncoder( Encoder* enc, long& oldpos ) 
{
  bool valchanged = false;
  long newpos = enc->read() / 4;
  
  if (newpos != oldpos) {

    if ( oldpos != -999 )
    {
       valchanged = true;
    }
    else
    {
       valchanged = false;
    }
    
    oldpos = newpos;
  }

  return valchanged;
}
