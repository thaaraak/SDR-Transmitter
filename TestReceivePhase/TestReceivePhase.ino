#include "Bounce2.h"
#include "si5351.h"
#include "Encoder.h"

#include "AudioTools.h"
#include "es8388.h"
#include "FIRConverter.h"

#include "fir_coeffs_501Taps_44100_150_4000.h"
#include "fir_coeffs_251Taps_22000_350_6000.h"

Si5351 *si5351;
TwoWire wire(0);
TwoWire wireExt(1);

#define AUDIO_SWITCH 21
#define PTT_PIN 5
#define USBLSB_PIN 4

bool sw = false;

Bounce bounce = Bounce();
Bounce usblsb = Bounce();


uint16_t sample_rate = 22000;
uint16_t channels = 2;
uint16_t bits_per_sample = 16;

I2SStream i2s;
StreamCopy copier(i2s, i2s); // copies sound into i2s
FIRAddConverter<int16_t> *fir;

int currentFrequency = -1;
int lastMult = -1;
float currentDir = 1.0;
int directionState = 1;

Encoder *encFrequency;
Encoder *encMenu;

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
      mult = 16;
    else if ( freq < 70000000 )
      mult = 10;    
    else if ( freq < 90000000 )
      mult = 8;
    else if ( freq < 124000000 )
      mult = 8;
    else if ( freq < 138000000 )
      mult = 8;
    else if ( freq < 148000000 )
      mult = 8;
    else
      mult = 8;
      
    uint64_t f = freq * 100ULL;
    uint64_t pllFreq = freq * mult * 100ULL;

    si5351->set_freq_manual(f, pllFreq, SI5351_CLK0);
    si5351->set_freq_manual(f, pllFreq, SI5351_CLK2);

    if ( mult != lastMult )
    {
      si5351->set_phase(SI5351_CLK0, 0);
      si5351->set_phase(SI5351_CLK2, mult);
      si5351->pll_reset(SI5351_PLLA);
      //si5351->pll_reset(SI5351_PLLB);
      si5351->update_status();

      lastMult = mult;
    }


}

void setupSynth()
{
  si5351 = new Si5351( &wireExt );
  si5351->init( SI5351_CRYSTAL_LOAD_8PF, 0, 0);
}

void setupFIR()
{
  //fir = new FIRAddConverter<int16_t>( (float*)&coeffs_hilbert_501Taps_44100_150_4000, (float*)&coeffs_delay_501, 501 );
  fir = new FIRAddConverter<int16_t>( (float*)&coeffs_hilbert_251Taps_22000_350_6000, (float*)&coeffs_delay_251, 251 );
  //fir = new FIRAddConverter<int16_t>( (float*)&coeffs_hilbert_501Taps_44100_350_10000, (float*)&coeffs_delay_501, 501 );
  fir->setCorrection(currentDir);
  
  //filtered.setFilter(0, new FIR<float>(coeffs_hilbert_251Taps_22000_350_10000));
  //filtered.setFilter(1, new FIR<float>(coeffs_delay_251));
}

void setupEncoders()
{
    encFrequency = new Encoder(13, 14);
    encMenu = new Encoder(15, 2);
}

void setup() 
{

  Serial.begin(115200);
    while(!Serial);
  AudioLogger::instance().begin(Serial, AudioLogger::Error);

  wireExt.setPins( 23, 19 );
  setupSynth();
  changeFrequency(14200000);  

  setupEncoders();
  setupFIR();

  // Input/Output Modes
  es_dac_output_t output = (es_dac_output_t) ( DAC_OUTPUT_LOUT1 | DAC_OUTPUT_LOUT2 | DAC_OUTPUT_ROUT1 | DAC_OUTPUT_ROUT2 );
  es_adc_input_t input = ADC_INPUT_LINPUT2_RINPUT2;
  //  es_adc_input_t input = ADC_INPUT_LINPUT1_RINPUT1;

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

  fir->setGain(8);
  i2s.begin(config);
  Serial.println("I2S started...");

  Serial.println("Attaching PTT_PIN" );
  bounce.attach( PTT_PIN,INPUT_PULLUP );
  bounce.interval(5);
  bounce.update();

  Serial.println("Attaching USBLSB_PIN" );
  usblsb.attach( USBLSB_PIN,INPUT_PULLUP );
  usblsb.interval(5);
  usblsb.update();

  pinMode(AUDIO_SWITCH, OUTPUT);

  setTransmitReceive();

}

int transmitting = false;
long oldFrequency = -999;
long oldMenu = -999;
long oldDir = -999;

void loop() 
{

  copier.copy( *fir );

  bounce.update();

  if ( bounce.changed() ) 
  {
    setTransmitReceive();
  }
  
  usblsb.update();

  if ( usblsb.changed() ) 
  {
    setUSBLSB();
  }

  readEncoders();


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


void setUSBLSB()
{  
    int deboucedInput = usblsb.read();
    if ( deboucedInput == LOW ) 
    {

      directionState = directionState * -1; 
      fir->setDirection(directionState );

      Serial.println( "Changing upper/lower" );

    }

    
}

bool readEncoder( Encoder* enc, long& oldpos ) 
{
  bool valchanged = false;
  long newpos = enc->read() / 4;
  
  if (newpos != oldpos) {
    valchanged = true;
    oldpos = newpos;
  }

  return valchanged;
}

void readEncoders() 
{
  char buf[30];
  bool freqchanged;
  bool menuchanged;

  long newFrequency;
  long newDir = oldDir;
  
  freqchanged = readEncoder( encFrequency, oldFrequency );
  menuchanged = readEncoder( encMenu, newDir );

  if ( freqchanged )
  {
    sprintf( buf, "Freq: %d", oldFrequency );
    Serial.println( buf );
  }

  if ( menuchanged )
  {
    if ( oldDir > newDir )
      currentDir -= 0.0005;
    else
      currentDir += 0.0005;

    fir->setCorrection(currentDir);
    sprintf( buf, "Dir: %7.5f", currentDir );

    Serial.println(buf);
    oldDir = newDir;

  }

}
