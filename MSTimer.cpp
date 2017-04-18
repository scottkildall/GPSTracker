/*
  MSTimer: Simple timer class for Arduino, makes code a lot cleaner
  
  
*/

#include "Arduino.h"
#include "MSTimer.h"


MSTimer::MSTimer() {
  duration = 0;
}

int MSTimer::isExpired() {
  return( (startTime + duration) < millis() );
}

void MSTimer::start() {
   startTime = millis();
}
void MSTimer::setTimer(unsigned long _duration) {
   duration = _duration;
   start(); 
}



