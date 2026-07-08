# CommandCatcher - a simple Arduino library for getting commands through the serial interface

by [Martin Scott Nicklous](https://github.com/msnicklous/)
| 
[Github Project Page](https://github.com/msnicklous/CommandCatcher/)

Released under the MIT License

---
## Overview

Sometimes when your arduino is connected to a serial interface, you want to be able to obtain and process commands through
that interface. For example, yo u might want to send commands to a test harness to cause certain actions to happen.

If you have a more complicated program that maybe uses a number of class and header files, all compiled and linked together,
you might want individual classes or modules to process their own commands in order to retain separation of concerns.

The `CommandCatcher` can help you do such things.

See [API Documentation](https://msnicklous.github.io/CommandCatcher/)

---

## Concepts

### General Operation

The `CommandCatcher` monitors the serial interface, receiving ASCII characters as they arrive. When the terminating character 
('\n', configurable) arrives, the `CommandCatcher` parses the command into command and parameter strings and calls registered
listeners for command processing. Each listener should each handle the commands it's interested in while ignoring 
the rest by simply returning. After all listeners have been called, the internal buffer will be reinitialized for the 
subsequent command. 

If no listeners are registered, the sketch can poll `CommandCatcher` to determine if a 
command is available for processing and use `CommandCatcher` methods to obtain the information and also to mark command 
processing as completed.

If a command arrives, but is not processed before the next command begins arriving, the first command will be overwritten
and lost. This is usually only a problem if no command listeners are registered.

The sketch calls the `CommandCatcher.update()` method at the top of the loop() function in order to make the magic work. 

The `CommandCatcher` expects to be the only module monitoring serial input. If other code is also monitoring and manipulating 
incoming serial data, there might occur tricky-to-find problems.

### Command Format

The `CommandCatcher` imposes a structure on the data received. In general, it looks like this:
```
<ASCII characters (command)><separator><ASCII characters (parameter string)><terminator>
```
The first separator character encountered separates the command from the parameter string, so the parameter string may
also contain separator characters without causing problems.

By default, the separator character is a blank ' ' and the terminator is a new line character '\n', so the following would
be a valid command string:
```
"Greeting Hello Bob!\n"
```

---

## Arduino Code


### Getting Started
To begin, include the `CommandCatcher` Library at the top of your sketch.

```
#include <CommandCatcher.h>
```

This will provide a global instance of the `CommandCatcher` class called CCatcher for your convenience. By default,
`CommandCatcher` uses the standard `Serial` stream for input, but a method is provided for using a different stream
if you need to. `CommandCatcher` needs to be initialized during the `setup()` function of your sketch, and since it 
uses `Serial` (or other stream), that needs to be initialized as well:

```
setup() {
  ...
  Serial.begin(115200);
  CCatcher.init();
  ...
}
```

If the init() function is not called, commands will neither be received nor processed. Calls to other `CommandCatcher` methods 
will be ignored.

If needed, you can also suppy a serial stream other than the standard `Serial` object to `CommandCatcher` during `init()`.
Also, `CommandCatcher` uses a default buffer size of 16 characters, but if you want a different size, you can specify that
in the init call as well:

```
setup() {
  ...
  SoftwareSerial mySerial (rxPin, txPin);
  mySerial.begin(115200);
  CCatcher.init(mySerial, 32);
  ...
}
```

At the top of the `loop()` function, before executing anything that might process a command, you need to call the 
`CommandCatcher.update()` method to process the serial queue. This method will not block or wait for input.   

```
loop() {
  ...
  CCatcher.update();
  ...
}
```

The values of the command and parameters will be discarded when you 
exit the listener function, so if you need them after returning, you need to save them yourself. 

You can add multiple listener functions. This might be useful for a larger project consisting of multiple independent
modules. By default, the `CommandCatcher`can accept up to 4 listeners. 

When you add multiple listeners, each listener will be called for every command. It is up to each listener 
to determine which commands it wants to process. 

The `CommandCatcher`can be used in **Polling** mode or in **Notification** mode.


### Polling Mode

In polling mode, the sketch uses the `update(false)` to prevent the command from bieng closed automatically, and then
uses `ready()` function to determine if a command is available.

```
void setup() {
  
  Serial.begin(115200);
  CCatcher.init();

}

void loop() {

  CCatcher.update(false);
  if (CCatcher.ready()) {
    char* cmd = CCatcher.getCommand();
    char* param = CCatcher.getParameter();
    ... process command ...
    CCatcher.close();
  } else {
    ... do something else ...
  }

}

```

Note that the `close()` function discards the contents of the command and parameter buffer.

### Notification Mode

To use notification mode, the sketch registers a listener function during `setup()` and calls `update(true)` (true is the 
default value of the parameter, so you can use `update()` just as well). When a command is available, the listener is called.
The listener function must accept two `char*` parameters and return `void`. The first parameter is the command, and the 
second is the parameter string. 

```
void notify(char* cmd, char* param);
```

You can add any free function that satisfies the method signature:

```
void setup() {
  Serial.begin(115200);
  CCatcher.init();
  CCatcher.addListener(aListener);
}

void loop() {
  CCatcher.update();
}

void aListener(char* cmd, char* param) {
  ... process the command ...
}
```

If you want to use a non-static class method as a listener, cou can do that by having your class extend the 
`CommandListener` abstract class and implement its `notify` method.

```
class MyClass  : public CommandListener {
  public:
  MyClass();
  void notify(char* cmd, char* param) override;
};
```
You can then add an instance of this class as a command listener.

```
MyClass myClass;
void setup() {
  Serial.begin(115200);
  CCatcher.init();
  CCatcher.addListener(myClass);
}

void loop() {
  CCatcher.update();
}
```