#include <Arduino.h>
#include "SerialCommandManager.h"

// ------------------------------
// Example command handlers
// ------------------------------
#include "SerialCommandManager.h"

// example commands would be
// MOVE:direction=REVERSE;speed=180
class MotorHandler : public ISerialCommandHandler {
private:
    int _pinDirection;
    int _pinSpeed;

public:
    MotorHandler(int directionPin, int speedPin)
        : _pinDirection(directionPin), _pinSpeed(speedPin) {
        pinMode(_pinDirection, OUTPUT);
        pinMode(_pinSpeed, OUTPUT);
    }

    bool handleCommand(SerialCommandManager* sender, const char* command, const StringKeyValue params[], uint8_t paramCount) override {
        Serial.println(sender->getRawMessage());
        (void)command;
        char direction[10] = "FORWARD";
        int speed = 0;

        for (int i = 0; i < paramCount; ++i) {
            if (strcmp(params[i].key, "direction") == 0) {
                strncpy(direction, params[i].value, sizeof(direction) - 1);
                direction[sizeof(direction) - 1] = '\0';
            } else if (strcmp(params[i].key, "speed") == 0) {
                speed = atoi(params[i].value);
            }
        }

        PinStatus pinStatus = strcmp(direction, "FORWARD") == 0 ? HIGH : LOW;
        digitalWrite(_pinDirection, pinStatus);
        analogWrite(_pinSpeed, constrain(speed, 0, 255));

        char debugMsg[DefaultMaxMessageLength];
        snprintf_P(debugMsg, sizeof(debugMsg), "Motor moved %s at speed %d", direction, speed);
        sender->sendDebug(debugMsg, "MotorHandler");
        return true;
    }

    const char* const* supportedCommands(size_t& count) const override {
        static const char* cmds[] = { "MOVE" };
        count = sizeof(cmds) / sizeof(cmds[0]);
        return cmds;
    }
};


// Fallback handler for unrecognized commands
void handleUnknown(SerialCommandManager* mgr)
{
    Serial.print("Unknown command received: ");
    Serial.println(mgr->getCommand());
    Serial.print("Raw message: ");
    Serial.println(mgr->getRawMessage());
    Serial.print("Param Count: ");
    Serial.println(mgr->getArgCount());
}

// ------------------------------
// Create SerialCommandManager
// ------------------------------
MotorHandler motorHandler(5, 6); // Direction on pin 5, speed on pin 6

SerialCommandManager commandManager(&Serial, handleUnknown, '\n', ':', ';', '=', 300, 64, 128);

// ------------------------------
// Setup
// ------------------------------
void setup()
{
    ISerialCommandHandler* handlers[] = { &motorHandler };
    commandManager.registerHandlers(handlers, 1);

    Serial.begin(115200);
    delay(500);
    Serial.println("SerialCommandManager Example Started");


    Serial.println("Ready to receive commands...");
    Serial.println("Examples:");
    Serial.println("  DEBUG:ON;");
    Serial.println("  DEBUG:OFF");
    Serial.println("  MOVE:direction=REVERSE;speed=180;");
}

// ------------------------------
// Loop
// ------------------------------
void loop()
{
    // Continuously read and process serial input
    commandManager.readCommands();

    // Example of sending debug message periodically
    static unsigned long lastTime = 0;
    if (millis() - lastTime > 5000)
    {
        lastTime = millis();
        commandManager.sendDebug(F("Heartbeat alive"), F("Loop"));
    }
}
