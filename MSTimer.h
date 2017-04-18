/*
  MSTimer: Simple timer class for Arduino, makes code a lot cleaner
  
  Written by Scott Kildall
  https://github.com/scottkildall
  www.kildall.com
*/


#ifndef CLASS_MSTimer
#define CLASS_MSTimer

class MSTimer
{
  public:
    MSTimer();
    
    int isExpired();
    void setTimer( unsigned long _duration );  // will also start the time
    void start();
    
    unsigned long startTime;
    unsigned long duration;
  private:
    
};

#endif // CLASS_MSTimer



