// #define rcIN 2
// #define hasLED false
// #define redLED 0
// #define greenLED 0
// SOFTPWM_DEFINE_CHANNEL_INVERT(0, DDRD, PORTD, PORTD4);  //Arduino pin 2 ApFET
// SOFTPWM_DEFINE_CHANNEL_INVERT(1, DDRC, PORTC, PORTC3);  //Arduino pin 4 CpFET
// SOFTPWM_DEFINE_CHANNEL(2, DDRB, PORTB, PORTB0);  //Arduino pin 5   CnFET
// SOFTPWM_DEFINE_CHANNEL(3, DDRD, PORTD, PORTD5);  //Arduino pin 13  AnFET

#define rcIN 2
#define revH true
#define revL false
#define pinAH 17
#define pinAL 13
#define pinBH 19
#define pinBL 18
#define pinCH 4
#define pinCL 5

void pin_setup(){
  //high-side pins
  pinMode(pinAH, OUTPUT);
  digitalWrite(pinAH, revH);
  pinMode(pinBH, OUTPUT);
  digitalWrite(pinBH, revH);
  pinMode(pinCH, OUTPUT);
  digitalWrite(pinCH, revH);
  //low-side pins
  pinMode(pinAL, OUTPUT);
  digitalWrite(pinAL, revL);
  pinMode(pinBL, OUTPUT);
  digitalWrite(pinBL, revL);
  pinMode(pinCL, OUTPUT);
  digitalWrite(pinCL, revL);
  //ppm input
  pinMode(rcIN, INPUT_PULLUP);
}

volatile bool ppm_flag = false;
volatile uint16_t ppm_reading;

volatile bool fwd = true;
volatile uint8_t new_pwm = 0;

 //Iterrupt0 / Pin2 interrupt function. 
ISR(INT0_vect){
  //processing depends on rising/falling edge
  if(digitalRead(rcIN)){
    //rising edge - start timer1
    TCNT1 = 0;
    bitSet(TIFR, TOV1); //also reset overflow flag
  }
  else{
    //falling edge
    uint16_t temp = TCNT1;
    bool overflow = bitRead(TIFR, TOV1);
    if(overflow) Serial.println("Overflow error!");
    else{
      ppm_reading = temp;
      ppm_flag = true;
    }
  }
}

void ppm_setup(){
  cli();
  //rcIN interrupt setup. Pin 2 = INT0
  //generate interrupt on any edge
  bitClear(MCUCR, ISC01);
  bitSet(MCUCR, ISC00);
  //turn on INT0
  bitSet(GICR, INT0);

  //Timer1 setup: normal mode, no prescaler
  TCCR1A = 0;
  TCCR1B = 1;

  // Serial.printf("TCCR1A: %x\n", TCCR1A); Serial.printf("TCCR1B: %x\n", TCCR1B); Serial.printf("TCNT1H: %x\n", TCNT1H);Serial.printf("TCNT1L: %x\n", TCNT1L);
  // Serial.printf("OCR1AH: %x\n", OCR1AH); Serial.printf("OCR1AL: %x\n", OCR1AH); Serial.printf("OCR1AH: %x\n", OCR1BH); Serial.printf("OCR1AL: %x\n", OCR1BH);
  // Serial.printf("ICR1H: %x\n", ICR1H); Serial.printf("ICR1L: %x\n", ICR1L); Serial.printf("TIMSK: %x\n", TIMSK); Serial.printf("TIFR: %x\n", TIFR);
  sei();
}

inline void pwm_L_on(){
  //pins 13, 18, 5: PB5, PC4, PD5
  bitSet(PORTB, PINB5);
  bitSet(PORTC, PINC4);
  bitSet(PORTD, PIND5);
}

inline void pwm_L_off(){
  //pins 13, 18, 5: PB5, PC4, PD5
  bitClear(PORTB, PINB5);
  bitClear(PORTC, PINC4);
  bitClear(PORTD, PIND5);
}

inline void pwm_H_on(){
  //pins 17, 19, 4: PC3, PC5, PD4
  bitSet(PORTC, PINC3);
  bitSet(PORTC, PINC5);
  bitSet(PORTD, PIND4);
}

inline void pwm_H_off(){
  //pins 17, 19, 4: PC3, PC5, PD4
  bitClear(PORTC, PINC3);
  bitClear(PORTC, PINC5);
  bitClear(PORTD, PIND4);
}

 //Tnterrupt timer 2 compare match -> turn on mosfets
ISR(TIMER2_COMP_vect){
  pwm_L_on();
}

 //Iterrupt timer 2 overflow -> turn off mosfets
ISR(TIMER2_OVF_vect){
  static uint16_t ppm = 0;

  if(ppm_flag){
    //load new PPM reading
    cli();
    ppm = ppm_reading;
    ppm_flag = false;
    
    
    //14500-34000
    if(ppm < 14500){
      pwm_L_on();
      bitClear(TIMSK, OCIE2); //DON'T generate interrupt on compare match
    }
    else if(ppm > 34000){
      
      bitClear(TIMSK, OCIE2); //DON'T generate interrupt on compare match
    }
    else{
      OCR2 = map(ppm, 14500, 34000, 0, 255);
      bitSet(TIMSK, OCIE2); //generate interrupt on compare match
    }
    sei();
    Serial.printf("PPM: %u %u\n", ppm, OCR2);
  }
  if(ppm >= 14500)
    pwm_L_off();
  
}

void pwm_setup(){
  cli();
  //Timer2 setup: normal mode, no prescaler
  //PWM frequency: 1kHz -> *100, 2kHz -> *011, 8kHz -> *010
  TCCR2 = 0b00000010;
  OCR2 = 240;
  bitSet(TIMSK, OCIE2); //generate interrupt on compare match
  bitSet(TIMSK, TOIE2); //generate interrupt on counter overflow
  // Serial.printf("TCCR1A: %x\n", TCCR1A); Serial.printf("TCCR1B: %x\n", TCCR1B); Serial.printf("TCNT1H: %x\n", TCNT1H);Serial.printf("TCNT1L: %x\n", TCNT1L);
  // Serial.printf("OCR1AH: %x\n", OCR1AH); Serial.printf("OCR1AL: %x\n", OCR1AH); Serial.printf("OCR1AH: %x\n", OCR1BH); Serial.printf("OCR1AL: %x\n", OCR1BH);
  // Serial.printf("ICR1H: %x\n", ICR1H); Serial.printf("ICR1L: %x\n", ICR1L); Serial.printf("TIMSK: %x\n", TIMSK); Serial.printf("TIFR: %x\n", TIFR);
  sei();
}

void setup(){
  pin_setup();
  Serial.begin(115200);
  delay(3000);
  ppm_setup();
  pwm_setup();
}

void loop(){
}
  