#include <BufferedTrace.h>
#include <CommandCatcher.h>
#include "MyClass.h"

// register myself and say hi
MyClass::MyClass(){
  Trace.itrace("hi!");
  CCatcher.addListener(this->notifyStatic);
  CCatcher.addListener(this);
  Trace.itrace("I registered myself!");
}

void MyClass::doSomething() {
  Trace.itrace("did something.");
}

// a static member listener function
void MyClass::notifyStatic(char* cmd, char* param) {
  Trace.trace("smf-c", cmd);
  Trace.trace("smf-p", param);
}

// a non-static member listener function
void MyClass::notify(char* cmd, char* param) {
  Trace.trace("nsmf-c", cmd);
  Trace.trace("nsmf-p", param);
}