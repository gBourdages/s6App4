#include "Particle.h"

SYSTEM_MODE(SEMI_AUTOMATIC);

SYSTEM_THREAD(ENABLED);

#define OUTPUT_PIN D2
#define INPUT_PIN D4
#define MANCHESTER_TIME 2500

void receivingThreadFunction(void *param);

Thread thread("receivingThread", receivingThreadFunction);

uint32_t manchesterTicks = System.ticksPerMicrosecond() * MANCHESTER_TIME;

void setup() {
	Serial.begin(9600);
  pinMode(OUTPUT_PIN, OUTPUT_OPEN_DRAIN);
  pinMode(INPUT_PIN, INPUT_PULLUP);
}

void loop() {
  preambule();
  startEndByte();
  entete(1,1);
  message();
  startEndByte();

  pinSetFast(OUTPUT_PIN);
  delay(10000);
}

void receivingThreadFunction(void *param) {
	while(true) {
		
	}
}

void message() {
  sendManchesterLOW();
  sendManchesterLOW();
  sendManchesterHIGH();
  sendManchesterLOW();
  sendManchesterHIGH();
  sendManchesterLOW();
  sendManchesterHIGH();
  sendManchesterLOW();
}

void entete(uint8_t flags, uint8_t length) {
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
  sendManchesterLOW();
  sendManchesterLOW();
  sendManchesterLOW();
  sendManchesterHIGH();
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
