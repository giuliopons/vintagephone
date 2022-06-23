
#define PIN_HANGUP_SWITCH D1      // switch hang up
#define PIN_BELL_1 D2
#define PIN_BELL_2 D3


void setup() {

  Serial.begin(115200);
  
  pinMode(PIN_HANGUP_SWITCH, INPUT_PULLUP);
  pinMode(PIN_BELL_1,OUTPUT);
  pinMode(PIN_BELL_2,OUTPUT);
 
}


//
// Ring the bells!
// Make a alternate square wave on bells coil with 2 pin and L293D driver
void bells() {
  int maxRings = 5;

  byte cornettaStatus; // 0 = OPEN, PICK UP - 1 = CLOSE, HUNG UP
  

  while(maxRings>=1) {
    int i = 0;
    
    cornettaStatus = digitalRead(PIN_HANGUP_SWITCH);  
    if(cornettaStatus==1) Serial.println("Ringing...");
    while(cornettaStatus==1 && i<30) {
      digitalWrite(PIN_BELL_2,LOW);
      digitalWrite(PIN_BELL_1,HIGH);
      delay(20);
      digitalWrite(PIN_BELL_2,HIGH);
      digitalWrite(PIN_BELL_1,LOW);
      delay(20);
      i++;
      cornettaStatus = digitalRead(PIN_HANGUP_SWITCH);  
    }
    if(cornettaStatus==0) Serial.println("Picked up!");
    digitalWrite(PIN_BELL_2,LOW);
    unsigned long ti = millis() + 2000;
    while(cornettaStatus == 1 && millis()<ti){
      delay(1);
      cornettaStatus = digitalRead(PIN_HANGUP_SWITCH);  
    }
    if(cornettaStatus==0) Serial.println("Picked up!");
    maxRings--;
  }
}



void loop()
{
  bells();

}