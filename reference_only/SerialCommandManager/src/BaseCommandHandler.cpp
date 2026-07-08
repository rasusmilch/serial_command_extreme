#include "BaseCommandHandler.h"

void BaseCommandHandler::sendAckOk(SerialCommandManager* sender, const char* cmd, const StringKeyValue* param, uint8_t paramCount)
{
    if (!sender || !cmd)
        return;

    // Format acknowledgement as specified in Commands.md:
    // ACK:<command>=<result>
    // If no explicit msg provided, use "ok" (lowercase per examples)

    // Build payload: cmd + "=ok"
    char payload[64]; // ACK payload buffer
    snprintf(payload, sizeof(payload), "%s=ok", cmd);

    if (param == nullptr)
        sender->sendCommand("ACK", payload);
    else
        sender->sendCommand("ACK", payload, "", const_cast<StringKeyValue*>(param), paramCount);
}

void BaseCommandHandler::sendAckErr(SerialCommandManager* sender, const char* cmd, const char* err, const StringKeyValue* param, uint8_t paramCount)
{
    if (!sender || !cmd)
        return;

    // Errors are also reported via ACK with the error text after '=' per Commands.md examples:
    // ACK:<command>=<error message>

    char payload[64]; // ACK payload buffer
    if (err && err[0] != '\0')
        snprintf(payload, sizeof(payload), "%s=%s", cmd, err);
    else
        snprintf(payload, sizeof(payload), "%s=error", cmd);

    if (param == nullptr)
        sender->sendCommand("ACK", payload);
    else
        sender->sendCommand("ACK", payload, "", param, paramCount);
}

void BaseCommandHandler::sendAckErr(SerialCommandManager* sender, const char* cmd, const __FlashStringHelper* err, const StringKeyValue* param, uint8_t paramCount)
{
    if (!sender || !cmd)
        return;

    // Errors are also reported via ACK with the error text after '=' per Commands.md examples:
    // ACK:<command>=<error message>

    // For FlashStringHelper, we need to read from program memory
    // Create a buffer to hold the error message from flash
    char errorBuf[64];
    if (err)
    {
        // Copy from program memory to RAM
        strncpy_P(errorBuf, (const char*)err, sizeof(errorBuf) - 1);
        errorBuf[sizeof(errorBuf) - 1] = '\0';
    }
    else
    {
        strcpy(errorBuf, "error");
    }

    char payload[64]; // ACK payload buffer
    snprintf(payload, sizeof(payload), "%s=%s", cmd, errorBuf);

    if (param == nullptr)
        sender->sendCommand("ACK", payload);
    else
        sender->sendCommand("ACK", payload, "", param, paramCount);
}

StringKeyValue BaseCommandHandler::makeParam(uint8_t key, uint8_t value)
{
    StringKeyValue param = {};
    itoa(key, param.key, 10);
    itoa(value, param.value, 10);
    return param;
}

StringKeyValue BaseCommandHandler::makeParam(uint8_t key, const char* value)
{
    StringKeyValue param = {};
    itoa(key, param.key, 10);
    if (value != nullptr)
    {
        strncpy(param.value, value, DefaultMaxParamValueLength);
        param.value[DefaultMaxParamValueLength] = '\0';
    }
    return param;
}

StringKeyValue BaseCommandHandler::makeParam(const char* key, uint8_t value)
{
    StringKeyValue param = {};
    if (key != nullptr)
    {
        strncpy(param.key, key, DefaultMaxParamKeyLength);
        param.key[DefaultMaxParamKeyLength] = '\0';
    }
    itoa(value, param.value, 10);
    return param;
}

StringKeyValue BaseCommandHandler::makeParam(const char* key, int value)
{
    StringKeyValue param = {};
    if (key != nullptr)
    {
        strncpy(param.key, key, DefaultMaxParamKeyLength);
        param.key[DefaultMaxParamKeyLength] = '\0';
    }

    itoa(value, param.value, 10);
    return param;
}

StringKeyValue BaseCommandHandler::makeParam(const char* key, char* value)
{
    StringKeyValue param = {};

    if (key != nullptr)
    {
        strncpy(param.key, key, DefaultMaxParamKeyLength);
        param.key[DefaultMaxParamKeyLength] = '\0';
    }

	if (value != nullptr)
    {
        strncpy(param.value, value, DefaultMaxParamValueLength);
        param.value[DefaultMaxParamValueLength] = '\0';
    }

    return param;
}