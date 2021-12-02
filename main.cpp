/*
 * Simple remote radio relay control
 * James Colderwood Nov 2021
 * 
 */

#include <EEPROM.h>

// Memory locations
#define passcode 0x0
#define enable 0x01
#define hangtime 0x02
#define id_enable 0x03
#define pip_enable 0x04
#define timeout 0x05
// END

// Hard coded
#define ctcss 0x0
#define dtmf_mask 0x0F
#define dtmf_detect_mask 0x10
#define H _BV(WGM21) // High Tone
#define L _BV(WGM20) // Low Tone
#define rx_mask 0x80
#define MAX_VALUE 0xFF
#define dit 70
#define dah dit*3
#define space dah
// END

bool rx = false;
bool tx = true;
long st = 0;
long li = 0;

struct rpt{
  bool repeat_enable;
  int hang;
  bool id;
  bool pip;
  unsigned long timeo;
};
rpt myrpt;

void checks() {
  EEPROM.update(timeout, 240);
  EEPROM.update(enable, 1);
  if (EEPROM.read(passcode) == MAX_VALUE){
    EEPROM.update(passcode, 149);
  }
  myrpt.repeat_enable=EEPROM.read(enable);
  myrpt.hang=EEPROM.read(hangtime);
  myrpt.id=EEPROM.read(id_enable);
  myrpt.pip=EEPROM.read(pip_enable);
  myrpt.timeo=EEPROM.read(timeout);
}

void id(){
  char* idc[6] = {"--.","-...","...--","...-",".--","-.."};
  for (int i=0; i < sizeof(idc) / sizeof(idc[0]); i++){
    if (i > 4){
      _delay_ms(space*2);
    }
    ids(idc[i]);
  }
  li = millis();
}

void ids(char* s){
  int j = 0;
  while (s[j] != NULL){
    idm(s[j], H);
    j++;
  }
  _delay_ms(space);
}

void idm(char c, int HL){
  TCCR2A = _BV(COM2A1) | _BV(COM2B1) | HL | _BV(WGM20);
  if (c == '.'){
    _delay_ms(dit);
  } else {
    _delay_ms(dah);
  }
  TCCR2A = 0;
  _delay_ms(dit);
}

void setup() {
  DDRB = DDRB | 0x3c; DDRD = 0x0;
  PORTB = 0x0; PORTD = rx_mask; 
  checks();
  OCR2A = 32;
  PORTB = 0x02;
  if (myrpt.id){
    _delay_ms(1000);
    id(); 
  }
}

void loop() {

  while (!myrpt.repeat_enable){
    PORTB = (1 << 5);
  }

  while ((PIND & dtmf_detect_mask) == dtmf_detect_mask){ // DTMF DECODE
    int CODE;
    PORTB = PORTB | (1 << 5);
    for (int i = 0; i < 200; i++){
    _delay_ms(25);
    if ((PIND & dtmf_detect_mask) == dtmf_detect_mask){
      idm("-", H);
      CODE = (PIND & dtmf_mask);
      i = 0;
    }
    switch (CODE){
      case 0x01:
      EEPROM.update(id_enable, 0);
      break;
      case 0x02:
      EEPROM.update(id_enable, 1);
      break;
      case 0x03:
      EEPROM.update(hangtime, 25);
      break;
      case 0x04:
      EEPROM.update(hangtime, MAX_VALUE);
      break;
      case 0x05:
      EEPROM.update(hangtime, 150);
      break;
      case 0x06:
      EEPROM.update(pip_enable, 1);
      break;
      case 0x07:
      EEPROM.update(pip_enable, 0);
      break;
      case 0x08:
      id();
      break;
    }
    CODE = 0x0;
    }
    checks();
    PORTB = PORTB & (0 << 5);
  }

  if (!rx && (PIND & rx_mask) == ctcss){
    PORTB = PORTB | 0x07;
    rx = true;
    tx = true;
    st = millis();
  } else if (rx && (PIND & rx_mask) == rx_mask){
    PORTB = PORTB & 0x02;
    rx = false;
    if ( myrpt.pip && millis() - st >= 2000 ){
      _delay_ms(250);
       idm('.', L);  
    }
    st = millis();
  }
  
  if (myrpt.timeo > 10 && rx && millis() - st == myrpt.timeo*1000){
    PORTB = (1 << 5);
    idm('-', L);
    while ((PIND & rx_mask) == ctcss){
      PORTB = PORTB | (1 << 0);
    }
    _delay_ms(10);
  }

  if (tx && !rx && millis() - st >= myrpt.hang*20){ 
    PORTB = PORTB & 0x0;
    tx=false;
  }

  if(myrpt.id && tx && !rx && millis() - li >= 600000){
    _delay_ms(500);
    id();
  }
}
