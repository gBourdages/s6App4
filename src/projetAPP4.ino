#include "Particle.h"

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
#define END 6

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
volatile uint8_t msgMask = 0b00000001;
volatile uint8_t msgBuffer = 0b00000000;
volatile uint16_t crcMask = 0b0000000000000001;
volatile uint16_t crcBuffer = 0b0000000000000000;
volatile uint8_t endByteMask = 0b00000001;
volatile uint8_t endByteBuffer = 0b00000000;
volatile uint8_t byteBuffer[255] = {};

volatile bool inputPinValue;

void setup() {
	Serial.begin(9600);
  pinMode(OUTPUT_PIN, OUTPUT_OPEN_DRAIN);
  pinMode(INPUT_PIN, INPUT_PULLUP);
  attachInterrupt(INPUT_PIN, interrupt, CHANGE);
  
}

void loop() {
  if (
}

void resetMEF() {
  byteCount = 0;
  preambuleStateTimes = 0;
  startStateTimes = 0;

  headerMask = 0b0000000000000001;
  header = 0b0000000000000000;

  msgMask = 0b00000001;
  msgBuffer = 0b00000000;

  crcMask = 0b0000000000000001;
  crcBuffer = 0b0000000000000000;

  endByteMask = 0b00000001;
  endByteBuffer = 0b00000000;
}

void interrupt() {
  interruptTick = System.ticks();
  inputPinValue = pinReadFast(INPUT_PIN);
  switch (state) {
  case WAITING:
    if(inputPinValue)
      break;
    state = PREAMBULE;
    break;

  case PREAMBULE:
    if(!inputPinValue)
      break;
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
    if(!inputPinValue)
      break;
    if (startStateTimes++ >= 5) {
      lastStateChange = interruptTick;
      state = HEADER;
    }
    break;
  
  case HEADER:
    if ((interruptTick - lastStateChange) < (manchesterTicksReceiver * 1.5))
      break;
    lastStateChange = interruptTick;
    registerHeaderData(!inputPinValue);
    if (!headerMask) {
      state = MESSAGE;
      msgLength = (header & 0b1111111100000000) >> 8;
      //Serial.print("MSG LENGTH");
      //Serial.println(msgLength);
    }
    break;
  
  case MESSAGE:
    //Serial.println(interruptTick - lastStateChange);
    if ((interruptTick - lastStateChange) < (manchesterTicksReceiver * 1.5))
      break;
    lastStateChange = interruptTick;
    registerMsgData(!inputPinValue);
    if (!msgMask) {
      byteBuffer[byteCount++] = msgBuffer;
      msgBuffer = 0b00000000;
      msgMask = 0b00000001;
      if (byteCount == msgLength) {
        state = CRC;
      }
    }
    break;

    case CRC:
      if ((interruptTick - lastStateChange) < (manchesterTicksReceiver * 1.5))
        break;
      lastStateChange = interruptTick;
      registerCRCData(!inputPinValue);
      if (!crcMask) {
        state = END;
      }
      break;
    
    case END:
      if ((interruptTick - lastStateChange) < (manchesterTicksReceiver * 1.5))
        break;
      lastStateChange = interruptTick;
      registerEndByteData(!inputPinValue);
      if (!endByteMask) {
        state = WAITING;
        Serial.println((char*)byteBuffer);
        Serial.println(crcBuffer);
        Serial.println(endByteBuffer);
        resetMEF();
      }
      break;
  }
}

void sendingThreadFunction(void *param) {
  system_tick_t lastThreadTime = 0;
  uint16_t msgCrc;
	while(true) {
		msgCrc = crc16((uint8_t*)"Un message", 11);
    preambule();
    sendByte(0b01111110);
    sendByte(0b00000000); //flags
    sendByte(11); //length
    sendBytes((uint8_t*)"Un message", 11);
    sendDualByte(msgCrc);
    sendByte(0b01111110);
    delay(1000);

    msgCrc = crc16((uint8_t*)"Un message?", 12);
    preambule();
    sendByte(0b01111110);
    sendByte(0b00000000); //flags
    sendByte(12); //length
    sendBytes((uint8_t*)"Un message?", 12);
    sendDualByte(msgCrc);
    sendByte(0b01111110);
    delay(1000);
	}
}

void registerHeaderData(bool data) {
  if (data)
    header |= headerMask;
  headerMask <<= 1;
}

void registerMsgData(bool data) {
  if (data)
    msgBuffer |= msgMask;
  msgMask <<= 1;
}

void registerCRCData(bool data) {
  if (data)
    crcBuffer |= crcMask;
  crcMask <<= 1;
}

void registerEndByteData(bool data) {
  if (data)
    endByteBuffer |= endByteMask;
  endByteMask <<= 1;
}

void sendBytes(uint8_t* bytes, uint8_t length) {
  for (int i = 0; i < length; ++i) {
    for (int j = 0; j < 8; ++j) {
      sendManchesterBit(bytes[i] & (0b00000001 << j));
    }
  }
}

void sendByte(uint8_t byte) {
  for (int j = 0; j < 8; ++j) {
      sendManchesterBit(byte & (0b00000001 << j));
  }
}

void sendDualByte(uint16_t byte) {
  for (int j = 0; j < 16; ++j) {
      sendManchesterBit(byte & (0b0000000000000001 << j));
  }
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

void sendManchesterBit(bool value) {
  if (value) {
    sendManchesterHIGH();
    return;
  }
  sendManchesterLOW();
}

uint16_t crc16(uint8_t *input_str, uint8_t length ) {
	uint8_t x;
    uint16_t crc = 0xFFFF;

    while (length--){
        x = crc >> 8 ^ *input_str++;
        x ^= x>>4;
        crc = (crc << 8) ^ ((uint8_t)(x << 12)) ^ ((uint8_t)(x <<5)) ^ ((uint8_t)x);
    }
    return crc;
}