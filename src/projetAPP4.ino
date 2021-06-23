#include "Particle.h"

SYSTEM_MODE(SEMI_AUTOMATIC);

SYSTEM_THREAD(ENABLED);

#define OUTPUT_PIN D2
#define INPUT_PIN D4

//SENDER----------------------------------------------------------------------------------

//En microsecondes. La période d'un bit manchester correspond à 2 fois cette valeur
#define MANCHESTER_TIME 750 
uint32_t manchesterTicks = System.ticksPerMicrosecond() * MANCHESTER_TIME;

void sendingThreadFunction(void *param);
os_mutex_t transmitMutex;
Thread sendingThread;

volatile uint16_t crcAck = 0;

void sendingThreadFunction(void *param) {
	while(true) {
    sendMessage((uint8_t*)"Message Gab 1", 14, 0b00000000, 500);
    delay(250);

    sendMessage((uint8_t*)"Message Gab 2", 14, 0b00000000, 500);
    delay(250);
	}
}

void sendMessage(uint8_t* message, uint8_t length, uint8_t flags, uint32_t ackDelay) {
  uint16_t msgCrc = crc16(message, length);
  while (crcAck != msgCrc) {
    os_mutex_lock(transmitMutex);
    preambule();
    sendByte(0b01111110);
    sendByte(flags);
    sendByte(length);
    sendBytes(message, length);
    sendDualByte(msgCrc);
    sendByte(0b01111110);
    os_mutex_unlock(transmitMutex);
    delay(ackDelay);
  }
}

void sendAck(uint16_t crc) {
  uint8_t crcSplit[2] = {(uint8_t)(crc & 0b1111111100000000 >> 8), (uint8_t)(crc & 0b0000000011111111)};
  uint16_t crCeption = crc16(crcSplit, 2);
  os_mutex_lock(transmitMutex);
  preambule();
  sendByte(0b01111110);
  sendByte(0b00000001); //flags
  sendByte(2);          //length
  sendDualByte(crc);
  sendDualByte(crCeption);
  sendByte(0b01111110);
  os_mutex_unlock(transmitMutex);
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


//RECEIVER--------------------------------------------------------------------------------
//Différents états de la MEF
#define WAITING 0
#define PREAMBULE 1
#define START 2
#define HEADER 3
#define MESSAGE 4
#define CRC 5
#define END 6
#define ACK 7

//Variables nécéssaire au fonctionnement de la MEF
volatile uint32_t state = WAITING;
volatile uint32_t lastStateChange;
volatile bool inputPinValue;
volatile uint32_t interruptTick = 0;
volatile uint8_t byteCount = 0;

//Variables pour le calcul de la clock
volatile uint32_t preambuleStateTimes = 0;
volatile uint32_t periodStart = 0;
volatile uint32_t period = 0;
volatile uint32_t manchesterTicksReceiver;

//Masque et buffer pour l'enregistrement de différentes données
volatile uint16_t header = 0b0000000000000000;
volatile uint16_t headerMask = 0b0000000000000001;
volatile uint8_t msgMask = 0b00000001;
volatile uint8_t msgBuffer = 0b00000000;
volatile uint16_t ackMask = 0b0000000000000001;
volatile uint16_t ackBuffer = 0b0000000000000000;
volatile uint16_t crcMask = 0b0000000000000001;
volatile uint16_t crcBuffer = 0b0000000000000000;
volatile uint8_t endByteMask = 0b00000001;
volatile uint8_t endByteBuffer = 0b00000000;
volatile uint8_t startByteMask = 0b00000001;
volatile uint8_t startByteBuffer = 0b00000000;

//Variables contenant des différentes données recues
volatile bool newMessage = false;
volatile uint8_t msgLength = 0;
volatile uint8_t byteBuffer[255] = {};
volatile uint16_t receivedCrc = 0;

//Variables pour la gestion d'erreur
volatile uint32_t lastWaitingTick = 0;
volatile bool error = false;

void setup() {
	Serial.begin(9600);
  pinMode(OUTPUT_PIN, OUTPUT_OPEN_DRAIN);
  pinMode(INPUT_PIN, INPUT_PULLUP);
  attachInterrupt(INPUT_PIN, interrupt, CHANGE);
  os_mutex_create(&transmitMutex);
  sendingThread = Thread("sendingThread", sendingThreadFunction);
}

void loop() {
  if (newMessage) {
    newMessage = false;

    if(receivedCrc == crc16((uint8_t*)byteBuffer, msgLength)) {

      sendAck(receivedCrc);

      Serial.print("Message : ");
      Serial.println((char*)byteBuffer);

      Serial.print("Crc : ");
      Serial.println(receivedCrc);
    } else {
      Serial.println("BAD MESSAGE RECEIVED");
    }

  }

  if((state != WAITING) && (((System.ticks() - lastWaitingTick) / System.ticksPerMicrosecond()) > 1000000)) {
    resetMEF();
    Serial.println("WAITING TIMEOUT");
  }

  if (error) {
    error = false;
    delay(random(250, 750));
    resetMEF();
    interrupts();
  }
}

void interrupt() {
  interruptTick = System.ticks();
  inputPinValue = pinReadFast(INPUT_PIN);
  switch (state) {
  case WAITING:
    lastWaitingTick = interruptTick;
    if(inputPinValue)
      break;
    state = PREAMBULE;
    break;

  case PREAMBULE:
    if(!inputPinValue) {
      period = interruptTick - periodStart;
      periodStart = interruptTick;
    }
    if (++preambuleStateTimes >= 8) {
      state = START;
      manchesterTicksReceiver = period / 4;
      lastStateChange = interruptTick;
    }
    break;

  case START:
    if ((interruptTick - lastStateChange) < (manchesterTicksReceiver * 1.5))
        break;
      lastStateChange = interruptTick;
      registerStartByteData(!inputPinValue);
      if (!startByteMask) {
        if(startByteBuffer != 0b01111110) {
          triggError();
          break;
        }
        state = HEADER;
      }
    break;
  
  case HEADER:
    if ((interruptTick - lastStateChange) < (manchesterTicksReceiver * 1.5))
      break;
    lastStateChange = interruptTick;
    registerHeaderData(!inputPinValue);
    if (!headerMask) {
      if (header & 0b0000000000000001)
        state = ACK;
      else
        state = MESSAGE;
      msgLength = (header & 0b1111111100000000) >> 8;
    }
    break;
  
  case MESSAGE:
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

    case ACK:
    if ((interruptTick - lastStateChange) < (manchesterTicksReceiver * 1.5))
      break;
    lastStateChange = interruptTick;
    registerAckData(!inputPinValue);
    if (!ackMask) {
      state = CRC;
      crcAck = ackBuffer;
    }
    break;

    case CRC:
      if ((interruptTick - lastStateChange) < (manchesterTicksReceiver * 1.5))
        break;
      lastStateChange = interruptTick;
      registerCRCData(!inputPinValue);
      if (!crcMask) {
        receivedCrc = crcBuffer;
        state = END;
      }
      break;
    
    case END:
      if ((interruptTick - lastStateChange) < (manchesterTicksReceiver * 1.5))
        break;
      lastStateChange = interruptTick;
      registerEndByteData(!inputPinValue);
      if (!endByteMask) {
        if(endByteBuffer != 0b01111110) {
          triggError();
          break;
        }
        newMessage = true;
        state = WAITING;
        resetMEF();
      }
      break;
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

void registerStartByteData(bool data) {
  if (data)
    startByteBuffer |= startByteMask;
  startByteMask <<= 1;
}

void registerAckData(bool data) {
  if (data)
    ackBuffer |= ackMask;
  ackMask <<= 1;
}


void triggError() {
  noInterrupts();
  error = true;
  Serial.println("ERROR TRIGGED");
}

void resetMEF() {
  state = WAITING;
  byteCount = 0;
  preambuleStateTimes = 0;

  headerMask = 0b0000000000000001;
  header = 0b0000000000000000;

  msgMask = 0b00000001;
  msgBuffer = 0b00000000;

  crcMask = 0b0000000000000001;
  crcBuffer = 0b0000000000000000;

  endByteMask = 0b00000001;
  endByteBuffer = 0b00000000;

  startByteMask = 0b00000001;
  startByteBuffer = 0b00000000;
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