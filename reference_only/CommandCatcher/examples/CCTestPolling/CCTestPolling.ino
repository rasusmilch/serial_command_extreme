#include <BufferedTrace.h>
#include <CommandCatcher.h>

/**
 ** This example program for CommandCatcher shows you how to use polling mode to 
 ** check for command availability. 
 */


void setup() {
  
  Serial.begin(115200);
  Trace.init();
  CCatcher.init();
  CCatcher.addListener(aListener);
  Trace.itrace("initialized everything.");

}

void loop() {
  Trace.open();

  CCatcher.update(false);
  if (CCatcher.ready()) {
    Trace.trace("ready");
    Trace.trace("cmd", CCatcher.getCommand());
    Trace.trace("param", CCatcher.getParameter());
    CCatcher.close();
  } else {
    // Trace.trace("not ready.");
  }

  Trace.close();
}


void aListener(char* cmd, char* param) {
  Trace.trace("al-cmd", CCatcher.getCommand());
  Trace.trace("al-param", CCatcher.getParameter());
}