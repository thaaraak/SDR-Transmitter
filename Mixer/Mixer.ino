#include "Bounce2.h"

#include "AudioTools.h"
#include "es8388.h"
#include "Wire.h"
#include <si5351.h>

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
FIRConverter<int16_t> *firusb;
FIRConverter<int16_t> *firlsb;


FIRConverter<int16_t> *lowpass_fir;
FIRConverter<int16_t> *bandpass_fir;
MultiConverter<int16_t> *multi;

#define PTT_PIN 32
#define SSB_PIN 33
#define XMIT_LED 14
#define IQBAL 34

Bounce bounce = Bounce();
Bounce bounceSsb = Bounce();

Si5351 *si5351;

TwoWire wire(0);
TwoWire siWire(1);

int currentFrequency = -1;
int lastMult = -1;

void changeFrequency( int freq )
{
    int mult = 0;
    currentFrequency = freq;

    if ( freq < 5000000 )
      mult = 150;
    else if ( freq < 6000000 )
      mult = 120;
    else if ( freq < 8000000 )
      mult = 100;
    else if ( freq < 11000000 )
      mult = 80;
    else if ( freq < 15000000 )
      mult = 50;
    else if ( freq < 22000000 )
      mult = 40;
    else if ( freq < 30000000 )
      mult = 30;
    else if ( freq < 40000000 )
      mult = 20;
    else if ( freq < 50000000 )
      mult = 15;

    uint64_t f = freq * 100ULL;
    uint64_t pllFreq = freq * mult * 100ULL;

    si5351->set_freq_manual(f, pllFreq, SI5351_CLK0);
    si5351->set_freq_manual(f, pllFreq, SI5351_CLK2);

    if ( mult != lastMult )
    {
      si5351->set_phase(SI5351_CLK0, 0);
      si5351->set_phase(SI5351_CLK2, mult);
      si5351->pll_reset(SI5351_PLLA);
      si5351->update_status();

      lastMult = mult;
    }
}

void setupSynth()
{
  si5351 = new Si5351( &siWire );
  si5351->init( SI5351_CRYSTAL_LOAD_8PF, 0, 0);
}


void setup() {

  Serial.begin(115200);

  pinMode(XMIT_LED, OUTPUT);
  digitalWrite(XMIT_LED, HIGH);

  bounce.attach( PTT_PIN,INPUT_PULLUP );
  bounce.interval(5);

  bounceSsb.attach( SSB_PIN,INPUT_PULLUP );
  bounceSsb.interval(5);

  siWire.setPins( 27, 26 );
  setupSynth();
  changeFrequency(10100000);
  
  AudioLogger::instance().begin(Serial, AudioLogger::Error); 

  multi = new MultiConverter<int16_t>();

  //fir = new FIRSplitterConverter<int16_t>( (float*)&plus_45_120, (float*)&minus_45_120, 120, true );
  //fir = new FIRSplitterConverter<int16_t>( (float*)&coeffs_hilbert_301Taps_44100_150_21000, (float*)&coeffs_delay_301, 301, true );
  firlsb = new FIRSplitterConverter<int16_t>( (float*)&coeffs_hilbert_501Taps_44100_150_21000, (float*)&coeffs_delay_501, 501, true );
  firusb = new FIRSplitterConverter<int16_t>( (float*)&coeffs_delay_501, (float*)&coeffs_hilbert_501Taps_44100_150_21000, 501, true );
  fir = firusb;
  
  lowpass_fir = new FIRConverter<int16_t>( (float*)&lowpass_4KHz, (float*)&lowpass_4KHz, 120 );
  //lowpass_fir = new FIRConverter<int16_t>( (float*)&lowpass_1khz, (float*)&lowpass_1khz, 120 );
  //bandpass_fir = new FIRConverter<int16_t>( (float*)&coeffs_Bandpass_201Taps_44100_500_4000, (float*)&coeffs_Bandpass_201Taps_44100_500_4000, 201 );
  
  //multi->add( *lowpass_fir );
  multi->add( *fir );
  
  // Input/Output Modes
  es_dac_output_t output = (es_dac_output_t) ( DAC_OUTPUT_LOUT1 | DAC_OUTPUT_LOUT2 | DAC_OUTPUT_ROUT1 | DAC_OUTPUT_ROUT2 );
  es_adc_input_t input = ADC_INPUT_LINPUT2_RINPUT2;

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
int lastIQBalVal = -1;
long lastCheck = 0;

void loop() {

  char buf[20];

  bounceSsb.update();

  if ( bounceSsb.changed() ) 
  {
    int debouncedInput = bounceSsb.read();
    if ( debouncedInput == LOW ) {

      if ( fir == firusb )
        fir = firlsb;
      else
        fir = firusb;

      Serial.println( "Changed Sideband" );
      multi = new MultiConverter<int16_t>();
      multi->add( *fir );
    }
  }
  
  //if ( transmitting )
    copier.copy(*multi);

  int iqBalVal = analogRead(IQBAL) / 50;
  if ( iqBalVal != lastIQBalVal )
  {
    lastIQBalVal = iqBalVal;

    float correction = iqBalVal / 800.0 + .95;
    sprintf( buf, "%4.3f", correction );
    Serial.println( buf );

    fir->setCorrection(correction);
  }
  
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
