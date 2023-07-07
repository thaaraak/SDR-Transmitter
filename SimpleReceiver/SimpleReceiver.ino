#include "Bounce2.h"
#include "AudioTools.h"
#include "es8388.h"
#include "Wire.h"
#include "WiFi.h"
#include "si5351.h"
#include "Encoder.h"
#include "FIRConverter.h"
#include "LiquidCrystal_I2C.h"

//#include "fir_coeffs_501Taps_44100_150_4000.h"
#include "fir_coeffs_251Taps_22000_350_6000.h"
#include "fir_coeffs_501Taps_22000_350_10000.h"
//#include "fir_coeffs_161Taps_44100_200_19000.h"
//#include "fir_coeffs_251Taps_44100_500_21000.h"
#include "fir_coeffs_801Taps_22000_200_10500.h"
#include "fir_coeffs_161Taps_22000_400_10000.h"

// Lowpass coefficients for 44.1 kHz
#include "120-tap-4khz-lowpass.h"
#include "120-tap-minus-45.h"
#include "120-tap-plus-45.h"

Si5351 *si5351;
TwoWire wire(0);
//TwoWire wireExt(1);

#define TRANSCEIVER

#ifdef TRANSCEIVER

#define AUDIO_SWITCH 42
#define PTT_PIN 5
#define USBLSB_PIN 12
#define EXT_SDA 36
#define EXT_SCL 35
#define FREQ_ENC_A 13
#define FREQ_ENC_B 14
#define MENU_ENC_A 15
#define MENU_ENC_B 7

#else

#define PTT_PIN 5
#define USBLSB_PIN 4
#define EXT_SDA 23
#define EXT_SCL 19
#define FREQ_ENC_A 12
#define FREQ_ENC_B 15
#define MENU_ENC_A 13
#define MENU_ENC_B 14

#endif

bool sw = false;

Bounce bounce = Bounce();
Bounce usblsb = Bounce();

LiquidCrystal_I2C *lcd;

uint16_t sample_rate = 22000;
uint16_t channels = 2;
uint16_t bits_per_sample = 16;

I2SStream in;
StreamCopy copier(in, in); 

FIRAddConverter<int16_t> *fir;
FIRConverter<int16_t> *lowpass_fir;
FIRConverter<int16_t> *bandpass_fir;
MultiConverter<int16_t> *multi;

int currentFrequency = -1;
int lastMult = -1;
float currentDir = 1.0;
int directionState = 1;

Encoder *encFrequency;
Encoder *encMenu;

void printFrequency()
{
  char buf[20];

  int freq = currentFrequency;
  int millions = freq / 1000000;
  int thousands = ( freq - millions * 1000000 ) / 1000;
  int remain = freq % 1000;

  sprintf( buf, "%3d.%03d.%03d %3s", millions, thousands, remain, directionState == 1 ? "USB" : "LSB" );
  lcd->setCursor(0,0);
  lcd->print( buf );
}

void changeFrequency( int freq )
{
    int mult = 0;
    currentFrequency = freq;

    printFrequency();

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
      si5351->update_status();

      lastMult = mult;
    }


}

void setupLCD()
{
  
  lcd = new LiquidCrystal_I2C(0x27,20,2,&wire);

  lcd->init();
  lcd->init();
  lcd->backlight();
  lcd->setCursor(0,0);
}

void setupSynth()
{
  si5351 = new Si5351( &wire );
  si5351->init( SI5351_CRYSTAL_LOAD_8PF, 0, 0);
}


void setupFIR()
{

  multi = new MultiConverter<int16_t>();

  //fir = new FIRAddConverter<int16_t>( (float*)&coeffs_hilbert_161Taps_22000_400_10000, (float*)&coeffs_delay_161, 161 );
  //fir = new FIRAddConverter<int16_t>( (float*)&coeffs_hilbert_251Taps_44100_500_21000, (float*)&coeffs_delay_251, 251 );
  //fir = new FIRAddConverter<int16_t>( (float*)&coeffs_hilbert_501Taps_44100_150_4000, (float*)&coeffs_delay_501, 501 );
  //fir = new FIRAddConverter<int16_t>( (float*)&plus_45_120, (float*)&minus_45_120, 120 );
  //fir = new FIRAddConverter<int16_t>( (float*)&coeffs_hilbert_251Taps_22000_350_6000, (float*)&coeffs_delay_251, 251 );
  //fir = new FIRAddConverter<int16_t>( (float*)&coeffs_hilbert_161Taps_44100_200_19000, (float*)&coeffs_delay_161, 161 );
  fir = new FIRAddConverter<int16_t>( (float*)&coeffs_hilbert_501Taps_22000_350_10000, (float*)&coeffs_delay_501, 501 );
  fir->setCorrection(currentDir);

//  lowpass_fir = new FIRConverter<int16_t>( (float*)&lowpass_4KHz, (float*)&lowpass_4KHz, 120 );
  bandpass_fir = new FIRConverter<int16_t>( (float*)&coeffs_hilbert_161Taps_22000_400_10000, (float*)&coeffs_hilbert_161Taps_22000_400_10000, 161 );
  
  multi->add( *bandpass_fir );
  multi->add( *fir );
  
}

/*
void setupFIR()
{
  //fir = new FIRAddConverter<int16_t>( (float*)&coeffs_hilbert_501Taps_44100_150_4000, (float*)&coeffs_delay_501, 501 );
  //fir = new FIRAddConverter<int16_t>( (float*)&coeffs_hilbert_501Taps_22000_350_10000, (float*)&coeffs_delay_501, 501 );
  fir = new FIRAddConverter<int16_t>( (float*)&coeffs_hilbert_251Taps_44100_500_21000, (float*)&coeffs_delay_251, 251 );
  fir->setCorrection(currentDir);
  
  //filtered.setFilter(0, new FIR<float>(coeffs_hilbert_251Taps_22000_350_10000));
  //filtered.setFilter(1, new FIR<float>(coeffs_delay_251));
}
*/

void setupEncoders()
{
    encFrequency = new Encoder(FREQ_ENC_A, FREQ_ENC_B);
    encMenu = new Encoder(MENU_ENC_A, MENU_ENC_B);
}

void setupButtons()
{
  /*
  Serial.println("Attaching PTT_PIN" );
  bounce.attach( PTT_PIN,INPUT_PULLUP );
  bounce.interval(5);
  bounce.update();
*/

  Serial.println("Attaching USBLSB_PIN" );
  usblsb.attach( USBLSB_PIN,INPUT_PULLUP );
  usblsb.interval(5);
  usblsb.update();


}

void setupI2S()
{
  // Input/Output Modes
  es_dac_output_t output = (es_dac_output_t) ( DAC_OUTPUT_LOUT1 | DAC_OUTPUT_LOUT2 | DAC_OUTPUT_ROUT1 | DAC_OUTPUT_ROUT2 );
  //es_adc_input_t input = ADC_INPUT_LINPUT2_RINPUT2;
  es_adc_input_t input = ADC_INPUT_LINPUT1_RINPUT1;

  es8388 codec;

  codec.begin( &wire );
  codec.config( bits_per_sample, output, input, 90 );

  // start I2S in
  auto config = in.defaultConfig(RXTX_MODE);
  config.sample_rate = sample_rate;
  config.bits_per_sample = bits_per_sample;
  config.i2s_format = I2S_STD_FORMAT;
  config.is_master = true;
  config.port_no = 0;
  config.pin_ws = 20;
  config.pin_bck = 47;
  config.pin_data = 21;
  config.pin_data_rx = 19;
  config.pin_mck = 45;
  config.use_apll = true;  
  in.begin(config);

  fir->setGain(4);
}


void setup() 
{

  Serial.begin(115200);
  AudioLogger::instance().begin(Serial, AudioLogger::Error);

  wire.setPins( 36, 35 );

  setupFIR();
  setupI2S();
  setupSynth();
  setupLCD();
  
  changeFrequency(14200000);  

  setupEncoders();
  setupButtons();
  
  //pinMode(AUDIO_SWITCH, OUTPUT);
  //setTransmitReceive();

}

int transmitting = false;
long oldFrequency = -999;
long oldMenu = -999;
long oldDir = -999;



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
      printFrequency();
      
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

  long newDir = oldDir;
  long newFrequency = oldFrequency;
  
  freqchanged = readEncoder( encFrequency, oldFrequency );
  menuchanged = readEncoder( encMenu, newDir );

  if ( freqchanged )
  {
    sprintf( buf, "Freq: %d", oldFrequency );
    Serial.println( buf );

    if ( oldFrequency > newFrequency )
        currentFrequency -= 500;
    else
        currentFrequency += 500;

    changeFrequency( currentFrequency );
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


void loop() 
{

/*
  bounce.update();

  if ( bounce.changed() ) 
  {
    setTransmitReceive();
  }
 */
  
  usblsb.update();

  if ( usblsb.changed() ) 
  {
    setUSBLSB();
  }

  readEncoders();

  copier.copy(*multi);
//  copier.copy( *fir );


}
