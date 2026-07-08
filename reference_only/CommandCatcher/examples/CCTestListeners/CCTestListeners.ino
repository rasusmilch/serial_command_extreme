#include <BufferedTrace.h>
#include <CommandCatcher.h>
#include "MyClass.h"

/**
 ** This example program for CommandCatcher shows you how to use polling mode to 
 ** check for command availability. 
 */


void setup() {

  Serial.begin(115200);
  Trace.init();
  CCatcher.init();
  CCatcher.addListener(aListener);
  CCatcher.addListener(bListener);
  Trace.itrace("initialized everything.");
  
}

void loop() {
  static MyClass mcl;
  Trace.open();
  CCatcher.update();
  Trace.close();
}


void aListener(char* cmd, char* param) {
  Trace.trace("al-c", cmd);
  Trace.trace("al-p", param);
}