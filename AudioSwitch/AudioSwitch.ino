#include "Bounce2.h"

#define AUDIO_SWITCH 21
#define PTT_PIN 5

bool sw = false;

Bounce bounce = Bounce();

void setup() 
{

  Serial.begin(115200);

  Serial.println("Attaching PTT_PIN" );
  bounce.attach( PTT_PIN,INPUT_PULLUP );
  bounce.interval(5);

  pinMode(AUDIO_SWITCH, OUTPUT);
}

int transmitting = false;

void loop() 
{

  bounce.update();

  if ( bounce.changed() ) 
  {
    int deboucedInput = bounce.read();
    
    if ( deboucedInput == LOW ) {
      Serial.println( "Transmit pressed" );
      digitalWrite(AUDIO_SWITCH, LOW);
      transmitting = true;
    } else {
      Serial.println( "Transmit released" );
      digitalWrite(AUDIO_SWITCH, HIGH);
      transmitting = false;
    }

  }
}
