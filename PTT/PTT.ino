#include "Bounce2.h"

#include "AudioTools.h"
#include "es8388.h"
#include "Wire.h"

#include "FIRConverter.h"

#include "fir_coeffs_Bandpass_201Taps_44100_500_4000.h"
#include "fir_coeffs_501Taps_44100_150_21000.h"
#include "fir_coeffs_301Taps_44100_150_21000.h"
#include "120-tap-plus-45.h"
#include "120-tap-minus-45.h"
#include "120-tap-4khz-lowpass.h"
#include "120-tap-1khz-lowpass.h"

uint16_t sample_rate = 44100;
uint16_t channels = 2;
uint16_t bits_per_sample = 16;
I2SStream in;

StreamCopy copier(in, in);               // copies sound into i2s

FIRConverter<int16_t> *fir;
FIRConverter<int16_t> *lowpass_fir;
FIRConverter<int16_t> *bandpass_fir;
MultiConverter<int16_t> *multi;

#define PTT_PIN 32
#define XMIT_LED 14

Bounce bounce = Bounce();

void setup() {

  Serial.begin(115200);

  pinMode(XMIT_LED, OUTPUT);
  digitalWrite(XMIT_LED, HIGH);

  bounce.attach( PTT_PIN,INPUT_PULLUP );
  bounce.interval(5);

  AudioLogger::instance().begin(Serial, AudioLogger::Error); 

  multi = new MultiConverter<int16_t>();

  //fir = new FIRSplitterConverter<int16_t>( (float*)&plus_45_120, (float*)&minus_45_120, 120, true );
  fir = new FIRSplitterConverter<int16_t>( (float*)&coeffs_hilbert_301Taps_44100_150_21000, (float*)&coeffs_delay_301, 301, true );

  lowpass_fir = new FIRConverter<int16_t>( (float*)&lowpass_4KHz, (float*)&lowpass_4KHz, 120 );
  //lowpass_fir = new FIRConverter<int16_t>( (float*)&lowpass_1khz, (float*)&lowpass_1khz, 120 );
  //bandpass_fir = new FIRConverter<int16_t>( (float*)&coeffs_Bandpass_201Taps_44100_500_4000, (float*)&coeffs_Bandpass_201Taps_44100_500_4000, 201 );
  
  multi->add( *lowpass_fir );
  multi->add( *fir );
  
  // Input/Output Modes
  es_dac_output_t output = (es_dac_output_t) ( DAC_OUTPUT_LOUT1 | DAC_OUTPUT_LOUT2 | DAC_OUTPUT_ROUT1 | DAC_OUTPUT_ROUT2 );
  es_adc_input_t input = ADC_INPUT_LINPUT2_RINPUT2;

  TwoWire wire(0);
  // 4, 15 for on breadboard
  //wire.setPins( 4, 15 );
  wire.setPins( 23, 22 );
  
  es8388 codec;
  codec.begin( &wire );
  codec.config( bits_per_sample, output, input, 90 );

  // start I2S in
  Serial.println("starting I2S...");
  auto config = in.defaultConfig(RXTX_MODE);
  config.sample_rate = sample_rate; 
  config.bits_per_sample = bits_per_sample; 
  config.channels = 2;
  config.i2s_format = I2S_STD_FORMAT;
  // For breadboard
  //config.pin_ws = 18;
  //config.pin_bck = 5;
  //config.pin_data = 17;
  //config.pin_data_rx = 16;
  config.pin_ws = 17;
  config.pin_bck = 18;
  config.pin_data = 5;
  config.pin_data_rx = 16;
  config.pin_mck = 3;
  in.begin(config);
}

bool transmitting = false;

void loop() {

  if ( transmitting )
    copier.copy(*multi);

  bounce.update();

  if ( bounce.changed() ) 
  {
    int deboucedInput = bounce.read();
    
    if ( deboucedInput == LOW ) {
      Serial.println( "Transmit pressed" );
      digitalWrite(XMIT_LED, LOW);
      transmitting = true;
    } else {
      Serial.println( "Transmit released" );
      digitalWrite(XMIT_LED, HIGH);
      transmitting = false;
    }

  }
}
