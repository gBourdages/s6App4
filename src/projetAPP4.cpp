/******************************************************/
//       THIS IS A GENERATED FILE - DO NOT EDIT       //
/******************************************************/

#line 1 "c:/Users/Gabriel/Desktop/s6App4/src/projetAPP4.ino"
#include "Particle.h"

void setup();
void loop();
void fallingInterrupt();
void risingInterrupt();
void registerHeaderData(bool data);
void registerBitData(bool data);
void message();
void entete(uint8_t flags, uint8_t length);
void preambule();
void startEndByte();
void sendManchesterLOW();
void sendManchesterHIGH();
#line 3 "c:/Users/Gabriel/Desktop/s6App4/src/projetAPP4.ino"
SYSTEM_MODE(SEMI_AUTOMATIC);

SYSTEM_THREAD(ENABLED);

#define OUTPUT_PIN D2
#define INPUT_PIN D4
#define MANCHESTER_TIME 1000

#define WAITING 0
#define PREAMBULE 1
#define START 2
#define HEADER 3
#define MESSAGE 4
#define CRC 5


void sendingThreadFunction(void *param);

void interrupt();

Thread thread("sendingThread", sendingThreadFunction);

uint32_t manchesterTicks = System.ticksPerMicrosecond() * MANCHESTER_TIME;

volatile uint32_t state = WAITING;
volatile uint32_t preambuleStateTimes = 0;
volatile uint32_t startStateTimes = 0;

volatile uint32_t periodStart = 0;
volatile uint32_t period = 0;
volatile uint32_t interruptTick = 0;
volatile uint32_t manchesterTicksReceiver;
volatile uint32_t lastStateChange;
volatile uint16_t header = 0b0000000000000000;
volatile uint16_t headerMask = 0b0000000000000001;
volatile uint8_t msgLength;
volatile uint8_t byteCount = 0;
volatile uint8_t bitMask = 0b00000001;
volatile uint8_t bitBuffer = 0b00000000;
volatile uint8_t byteBuffer[255] = {};

void setup() {
	Serial.begin(9600);
  pinMode(OUTPUT_PIN, OUTPUT_OPEN_DRAIN);
  pinMode(INPUT_PIN, INPUT_PULLUP);
  attachInterrupt(INPUT_PIN, interrupt, CHANGE);
}

void loop() {

}

void interrupt() {
  interruptTick = System.ticks();
  if (pinReadFast(INPUT_PIN))
    risingInterrupt();
  else
    fallingInterrupt();
}

void fallingInterrupt() {
  switch (state) {
  case WAITING:
    state = PREAMBULE;
    break;

  case HEADER:
    if ((interruptTick - lastStateChange) < (manchesterTicksReceiver * 1.5))
      break;
    lastStateChange = interruptTick;
    registerHeaderData(1);
    if (!headerMask) {
      state = MESSAGE;
      msgLength = header & 0b0000000011111111;
      //Serial.println(msgLength);
    }
    break;

  case MESSAGE:
    //Serial.println(interruptTick - lastStateChange);
    if ((interruptTick - lastStateChange) < (manchesterTicksReceiver * 1.5))
      break;
    lastStateChange = interruptTick;
    registerBitData(1);
    if (!bitMask) {
      byteBuffer[byteCount++] = bitBuffer;
      bitBuffer = 0b00000000;
      bitMask = 0b00000001;
      if (byteCount == msgLength) {
        state = CRC;
      }
    }
    break;

    case CRC:
      Serial.println((char*)byteBuffer);
      break;
  }
}

void risingInterrupt() {
  switch (state) {
  case PREAMBULE:
    period = interruptTick - periodStart;
    periodStart = interruptTick;
    if (preambuleStateTimes++ >= 4) {
      state = START;
      manchesterTicksReceiver = period / 4;
      //Serial.println(manchesterTicksReceiver);
      //Serial.println(manchesterTicks);
    }
    break;

  case START:
    if (startStateTimes++ >= 5) {
      lastStateChange = interruptTick;
      state = HEADER;
    }
    break;
  
  case HEADER:
    if ((interruptTick - lastStateChange) < (manchesterTicksReceiver * 1.5))
      break;
    lastStateChange = interruptTick;
    registerHeaderData(0);
    if (!headerMask) {
      state = MESSAGE;
      msgLength = header & 0b0000000011111111;
      //Serial.println(msgLength);
    }
    break;
  
  case MESSAGE:
    //Serial.println(interruptTick - lastStateChange);
    if ((interruptTick - lastStateChange) < (manchesterTicksReceiver * 1.5))
      break;
    lastStateChange = interruptTick;
    registerBitData(0);
    if (!bitMask) {
      byteBuffer[byteCount++] = bitBuffer;
      bitBuffer = 0b00000000;
      bitMask = 0b00000001;
      if (byteCount == msgLength) {
        state = CRC;
      }
    }
    break;

    case CRC:
      Serial.println((char*)byteBuffer);
      break;
  }
}

void sendingThreadFunction(void *param) {
	while(true) {
		preambule();
    startEndByte();
    entete(1,1);
    message();
    startEndByte();

    pinSetFast(OUTPUT_PIN);
    delay(10000);
	}
}

void registerHeaderData(bool data) {
  if (data)
    header |= headerMask;
  headerMask <<= 1;
}

void registerBitData(bool data) {
  if (data)
    bitBuffer |= bitMask;
  bitMask <<= 1;
}

void message() {
  sendManchesterLOW();
  sendManchesterLOW();
  sendManchesterHIGH();
  sendManchesterLOW();
  sendManchesterHIGH();
  sendManchesterHIGH();
  sendManchesterLOW();
  sendManchesterLOW();

  sendManchesterLOW();
  sendManchesterHIGH();
  sendManchesterLOW();
  sendManchesterLOW();
  sendManchesterHIGH();
  sendManchesterHIGH();
  sendManchesterLOW();
  sendManchesterLOW();

  sendManchesterLOW();
  sendManchesterLOW();
  sendManchesterLOW();
  sendManchesterLOW();
  sendManchesterLOW();
  sendManchesterLOW();
  sendManchesterLOW();
  sendManchesterLOW();
}

void entete(uint8_t flags, uint8_t length) {
  sendManchesterHIGH();
  sendManchesterHIGH();
  sendManchesterLOW();
  sendManchesterLOW();
  sendManchesterLOW();
  sendManchesterLOW();
  sendManchesterLOW();
  sendManchesterLOW();

  sendManchesterLOW();
  sendManchesterLOW();
  sendManchesterLOW();
  sendManchesterLOW();
  sendManchesterLOW();
  sendManchesterLOW();
  sendManchesterHIGH();
  sendManchesterLOW();
}

void preambule() {
  sendManchesterLOW();
  sendManchesterHIGH();
  sendManchesterLOW();
  sendManchesterHIGH();
  sendManchesterLOW();
  sendManchesterHIGH();
  sendManchesterLOW();
  sendManchesterHIGH();
}

void startEndByte() {
  sendManchesterLOW();
  sendManchesterHIGH();
  sendManchesterHIGH();
  sendManchesterHIGH();
  sendManchesterHIGH();
  sendManchesterHIGH();
  sendManchesterHIGH();
  sendManchesterLOW();
}

void sendManchesterLOW() {
  pinResetFast(OUTPUT_PIN);
  System.ticksDelay(manchesterTicks);
  pinSetFast(OUTPUT_PIN);
  System.ticksDelay(manchesterTicks);
}

void sendManchesterHIGH() {
  pinSetFast(OUTPUT_PIN);
  System.ticksDelay(manchesterTicks);
  pinResetFast(OUTPUT_PIN);
  System.ticksDelay(manchesterTicks);
}
