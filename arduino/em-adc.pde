#define PSI_PIN A0
#define BAT_PIN A1
#define HORN_PIN 11

#define ANALOG_SAMPLES 15
#define SAMPLE_DELAY 700
#define MAX_DUR 5000
#define MAX_FRQ 6000

int i, drval, fqval, ch, bufcnt, state = 0;
char drbuf[20];
char fqbuf[20];
int analogbuf_psi[ANALOG_SAMPLES];
int analogbuf_bat[ANALOG_SAMPLES];
int analogcount_psi = 0, analogcount_bat = 0;
int analogsum_psi, analogsum_bat;
int analogsumfinal_psi, analogsumfinal_bat;
int sample_delay_cnt = 0;

void setup() {
  Serial.begin(9600);
  pinMode(PSI_PIN, INPUT);
  pinMode(BAT_PIN, INPUT);
  pinMode(HORN_PIN, OUTPUT);
  digitalWrite(PSI_PIN, LOW);
  digitalWrite(BAT_PIN, LOW);
  digitalWrite(HORN_PIN, LOW);
  delay(1000);
}

void loop() {
  sample_delay_cnt++;
  
  if(sample_delay_cnt > (int)SAMPLE_DELAY) {
    sample_delay_cnt = 0;
    analogbuf_psi[analogcount_psi] = analogRead(PSI_PIN);
    analogbuf_bat[analogcount_psi] = analogRead(BAT_PIN);
    analogcount_psi++;
  }
  
  if(analogcount_psi >= (int)ANALOG_SAMPLES) {
    analogcount_psi = 0;
    analogcount_bat = 0;
    analogsum_psi = 0;
    analogsum_bat = 0;
    analogsumfinal_psi = 0;
    analogsumfinal_bat = 0;

    // add samples
    for(i = 0; i < (int)ANALOG_SAMPLES; i++) {
      analogsum_psi += analogbuf_psi[i]; 
      analogsum_bat += analogbuf_bat[i];
    }
  
    // get average of samples
    analogsum_psi = (int)((float)analogsum_psi / (float)ANALOG_SAMPLES);
    analogsum_bat = (int)((float)analogsum_bat / (float)ANALOG_SAMPLES); 
    
    // filter samples 
    for(i = 0; i < (int)ANALOG_SAMPLES; i++) {
      if( (analogbuf_psi[i] < (analogsum_psi*1.3)) && (analogbuf_psi[i] > (analogsum_psi*0.7)) ) {
        analogsumfinal_psi += analogbuf_psi[i];  
        analogcount_psi++;
      }

      if( (analogbuf_bat[i] < (analogsum_bat*1.3)) && (analogbuf_bat[i] > (analogsum_bat*0.7)) ) {
        analogsumfinal_bat += analogbuf_bat[i];  
        analogcount_bat++;
      }
    }

    // get average of samples
    analogsumfinal_psi = (int)((float)analogsumfinal_psi / (float)analogcount_psi);
    analogsumfinal_bat = (int)((float)analogsumfinal_bat / (float)analogcount_bat);

    Serial.print(":P"); Serial.print(analogsumfinal_psi); Serial.print(":B"); Serial.print(analogsumfinal_bat);

    analogcount_psi = 0;
    analogcount_bat = 0;
  }

  if(Serial.available() > 0) {
    ch = Serial.read();
      
    if(ch == 'F' || ch == 'f') {
      state=1;
      bufcnt=0;
    } else if(ch == 'D' || ch == 'd') {
      state=2;
      fqbuf[bufcnt] = '\0';
      bufcnt=0;
    } else if(ch == 'E' || ch == 'e') {
      state=0;
      drbuf[bufcnt] = '\0';
      bufcnt=0;
     
      // now we have dr and fq buf's ready... convert
      drval = atoi(drbuf);
      fqval = atoi(fqbuf);
      
      // cull results
      if(drval < 0) drval = 0;
      if(drval > (int)MAX_DUR) drval = (int)MAX_DUR;
      if(fqval < 0) fqval = 0;
      if(fqval > (int)MAX_FRQ) fqval = (int)MAX_FRQ;
      Serial.print("Playing sound ");
      Serial.print(fqval);
      Serial.print(" ");
      Serial.println(drval);

      // play some music
      tone(HORN_PIN, fqval, drval); // what the signal should look like: FFF1000DDD200EEE
    } else { // main data
      if(state == 1) {
        fqbuf[bufcnt] = ch; 
        bufcnt++; 
      } else if(state == 2) {
        drbuf[bufcnt] = ch;
        bufcnt++;
      }
    }
  }
}
