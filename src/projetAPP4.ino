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

volatile bool crc_tab16_init = false;
volatile uint16_t crc_tab16[256];

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
      msgLength = (header & 0b1111111100000000) >> 8;
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
      msgLength = (header & 0b1111111100000000) >> 8;
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
    uint16_t crc = crc16((uint8_t*)"Un message", 11);

		preambule();
    sendByte(0b01111110);
    sendByte(0b00000000); //flags
    sendByte(11); //length
    sendBytes((uint8_t*)"Un message", 11);
    sendDualByte(crc);
    sendByte(0b01111110);

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

uint16_t crc16(uint8_t *input_str, uint8_t num_bytes ) {

	uint16_t crc;
	const unsigned char *ptr;
	size_t a;

	if ( ! crc_tab16_init ) init_crc16_tab();

	crc = 0x0000;
	ptr = input_str;

	if ( ptr != NULL ) for (a=0; a<num_bytes; a++) {

		crc = (crc >> 8) ^ crc_tab16[ (crc ^ (uint16_t) *ptr++) & 0x00FF ];
	}

	return crc;

}

void init_crc16_tab( void ) {

	uint16_t i;
	uint16_t j;
	uint16_t crc;
	uint16_t c;

	for (i=0; i<256; i++) {

		crc = 0;
		c   = i;

		for (j=0; j<8; j++) {

			if ( (crc ^ c) & 0x0001 ) crc = ( crc >> 1 ) ^ 0xA001;
			else                      crc =   crc >> 1;

			c = c >> 1;
		}

		crc_tab16[i] = crc;
	}

	crc_tab16_init = true;
}  