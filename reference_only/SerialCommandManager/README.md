# SerialCommandManager
SerialCommandManager is a lightweight Arduino/ESP library that parses structured serial commands with optional key/value parameters, routes them to registered handlers, and provides debug, error, and fallback support for robust serial communication.

# How To Use
This section shows how to use the SerialCommandManager library with Arduino or ESP boards.

It covers:
- Creating the manager
- Registering commands
- Using key/value parameters
- Handling unrecognized commands
- Sending debug and error messages

## Include the Library

`
#include <Arduino.h>
#include "SerialCommandManager.h"
`

## Define Command Handlers

Handlers receive:
- command – the command string
- params – array of key/value pairs
- paramCount – number of parameters
- context – optional user pointer



`
// Handles DEBUG command
void handleDebug(const String& command, StringKeyValue* params, int paramCount, void* context)
{
    if (paramCount > 0 && params[0].key.equalsIgnoreCase("ON"))
    {
        Serial.println("Debug mode ENABLED");
    }
    else if (paramCount > 0 && params[0].key.equalsIgnoreCase("OFF"))
    {
        Serial.println("Debug mode DISABLED");
    }
}

// Handles LED control command
void handleLED(const String& command, StringKeyValue* params, int paramCount, void* context)
{
    int pin = 13;
    bool state = false;

    for (int i = 0; i < paramCount; i++)
    {
        if (params[i].key.equalsIgnoreCase("pin")) pin = params[i].value.toInt();
        if (params[i].key.equalsIgnoreCase("state")) state = params[i].value.equalsIgnoreCase("ON");
    }

    pinMode(pin, OUTPUT);
    digitalWrite(pin, state ? HIGH : LOW);

    Serial.print("LED command processed: pin=");
    Serial.print(pin);
    Serial.print(", state=");
    Serial.println(state ? "ON" : "OFF");
}

// Handles PING command
void handlePing(const String& command, StringKeyValue* params, int paramCount, void* context)
{
    Serial.println("PONG;");
}

// Default handler for unrecognized commands
void handleUnknown(SerialCommandManager* mgr)
{
    Serial.print("Unknown command received: ");
    Serial.println(mgr->getCommand());
    Serial.print("Raw message: ");
    Serial.println(mgr->getRawMessage());
}
`
## Create the SerialCommandManager

`
// Arguments:
// &Serial       -> serial port
// handleUnknown -> fallback callback for unknown commands
// '\n'          -> terminator
// ':'           -> command/parameter separator
// '='           -> key/value separator
// 500           -> timeout in ms
// 256           -> maximum message size
SerialCommandManager commandMgr(&Serial, handleUnknown, '\n', ':', '=', 500, 256);
`

## Register Handlers in setup()

`
void setup()
{
    Serial.begin(115200);
    delay(1000);
    Serial.println("SerialCommandManager Example Started");

    // Register commands
    commandMgr.registerHandler("DEBUG", handleDebug);
    commandMgr.registerHandler("LED", handleLED);
    commandMgr.registerHandler("PING", handlePing);

    Serial.println("Ready to receive commands...");
    Serial.println("Examples:");
    Serial.println("  DEBUG:ON;");
    Serial.println("  LED:pin=13,state=ON;");
    Serial.println("  PING;");
}
`

## Read Commands in loop()
`
void loop()
{
    commandMgr.readCommands();  // Continuously read serial input
}
`

## Example Serial Inputs

| Input                   | Expected Behavior                                 |
| ----------------------- | ------------------------------------------------- |
| `DEBUG:ON;`             | Enables debug messages                            |
| `DEBUG:OFF;`            | Disables debug messages                           |
| `LED:pin=13,state=ON;`  | Turns on LED on pin 13                            |
| `LED:pin=13,state=OFF;` | Turns off LED on pin 13                           |
| `PING;`                 | Responds with `PONG;`                             |
| `FOO:bar=baz;`          | Triggers default handler → prints unknown command |

## Sending Messages from Arduino

### Debug messages:
`
commandMgr.sendDebug("Status update", "Loop1");
`

### Error messages:
`
commandMgr.sendError("Invalid command", "LED");
`

### Custom formatted commands:
`
StringKeyValue params[2] = { {"pin", "12"}, {"state", "ON"} };
commandMgr.sendCommand("LED", "Update", "Controller1", params, 2);
`

## Notes

- Handlers are case-insensitive for both commands and keys.
- Dynamic parameter arrays allow flexibility; you can parse as many key/value pairs as needed.
- The default/fallback callback ensures no message is ignored.
- Works on all Arduino and ESP platforms.
