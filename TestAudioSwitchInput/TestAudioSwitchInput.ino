#include "Bounce2.h"

#define AUDIO_SWITCH 21
#define PTT_PIN 5

bool sw = false;

Bounce bounce = Bounce();

#include "AudioTools.h"
#include "es8388.h"

uint16_t sample_rate=40000;
uint16_t channels = 2;
uint16_t bits_per_sample = 16; // or try with 24 or 32
I2SStream i2s;
StreamCopy copier(i2s, i2s); // copies sound into i2s

void setup() 
{

  Serial.begin(115200);
    while(!Serial);
  AudioLogger::instance().begin(Serial, AudioLogger::Error);

  // Input/Output Modes
  es_dac_output_t output = (es_dac_output_t) ( DAC_OUTPUT_LOUT1 | DAC_OUTPUT_LOUT2 | DAC_OUTPUT_ROUT1 | DAC_OUTPUT_ROUT2 );
  es_adc_input_t input = ADC_INPUT_LINPUT2_RINPUT2;
  //  es_adc_input_t input = ADC_INPUT_LINPUT1_RINPUT1;

  TwoWire wire(0);
  wire.setPins( 33, 32 );
  
  es8388 codec;
  codec.begin( &wire );
  codec.config( bits_per_sample, output, input, 90 );

  // start I2S in
  Serial.println("starting I2S...");
  auto config = i2s.defaultConfig(RXTX_MODE);
  config.sample_rate = sample_rate; 
  config.bits_per_sample = bits_per_sample; 
  config.channels = 2;
  config.i2s_format = I2S_STD_FORMAT;
  config.pin_ws = 25;
  config.pin_bck = 27;
  config.pin_data = 26;
  config.pin_data_rx = 35;
  config.pin_mck = 0;

  i2s.begin(config);
  Serial.println("I2S started...");

  Serial.println("Attaching PTT_PIN" );
  bounce.attach( PTT_PIN,INPUT_PULLUP );
  bounce.interval(5);

  pinMode(AUDIO_SWITCH, OUTPUT);

  bounce.update();
  setTransmitReceive();
}

int transmitting = false;

void loop() 
{

  bounce.update();

  if ( bounce.changed() ) 
  {
    setTransmitReceive();
  }

  copier.copy();
  

}

void setTransmitReceive()
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
