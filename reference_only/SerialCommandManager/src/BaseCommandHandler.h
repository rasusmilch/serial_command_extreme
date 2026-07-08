#pragma once
#include <Arduino.h>
#include <SerialCommandManager.h>

/**
 * @brief Small helper base class that centralizes ACK formatting used by command handlers.
 *
 * Handlers can inherit from `BaseCommandHandler` to get protected `sendAckOk` / `sendAckErr`
 * helpers that delegate to `SerialCommandManager::sendCommand(...)` while keeping the
 * communications layer abstract.
 *
 * This follows the typical Arduino library comment style (Doxygen-compatible) so the
 * documentation can be generated or read inline in sketches.
 */
class BaseCommandHandler : public ISerialCommandHandler
{
protected:
    /**
     * @brief Send an acknowledgement indicating the command completed successfully.
     *
     * @param sender Pointer to the `SerialCommandManager` that will perform the send.
     * @param cmd The command header/string to include in the ACK.
     * @param param Optional key/value parameter to include with the ACK (may be `nullptr`).
     * @param paramCount Number of parameters if `param` is an array, default 1.
     */
    void sendAckOk(SerialCommandManager* sender, const char* cmd, const StringKeyValue* param = nullptr, uint8_t paramCount = 1);

    /**
     * @brief Send an acknowledgement indicating the command failed.
     *
     * @param sender Pointer to the `SerialCommandManager` that will perform the send.
     * @param cmd The command header/string to include in the ACK.
     * @param err A human-readable error string describing the failure.
     * @param param Optional key/value parameter to include with the error ACK (may be `nullptr`).
	 * @param paramCount Number of parameters if `param` is an array, default 1.
     */
    void sendAckErr(SerialCommandManager* sender, const char* cmd, const char* err, const StringKeyValue* param = nullptr, uint8_t paramCount = 1);

    /**
     * @brief Send an acknowledgement indicating the command failed.
     *
     * @param sender Pointer to the `SerialCommandManager` that will perform the send.
     * @param cmd The command header/string to include in the ACK.
     * @param err A human-readable error string describing the failure.
     * @param param Optional key/value parameter to include with the error ACK (may be `nullptr`).
     * @param paramCount Number of parameters if `param` is an array, default 1.
     */
    void sendAckErr(SerialCommandManager* sender, const char* cmd, const __FlashStringHelper* err, const StringKeyValue* param = nullptr, uint8_t paramCount = 1);

    /**
     * @brief Create a StringKeyValue from two uint8_t values.
     *
     * @param key The key as a uint8_t value (will be converted to string).
     * @param value The value as a uint8_t value (will be converted to string).
     * @return StringKeyValue with both key and value converted to strings.
     */
    static StringKeyValue makeParam(uint8_t key, uint8_t value);

    /**
     * @brief Create a StringKeyValue from a uint8_t key and a string value.
     *
     * @param key The key as a uint8_t value (will be converted to string).
     * @param value The value as a null-terminated string.
     * @return StringKeyValue with key converted to string and value copied.
     */
    static StringKeyValue makeParam(uint8_t key, const char* value);

    /**
     * @brief Create a StringKeyValue from a string key and a uint8_t value.
     *
     * @param key The key as a null-terminated string.
     * @param value The value as an int value (will be converted to string).
     * @return StringKeyValue with key copied and value converted to string.
     */
    static StringKeyValue makeParam(const char* key, int value);

    /**
     * @brief Create a StringKeyValue from a string key and a uint8_t value.
     *
     * @param key The key as a null-terminated string.
     * @param value The value as a uint8_t value (will be converted to string).
     * @return StringKeyValue with key copied and value converted to string.
     */
    static StringKeyValue makeParam(const char* key, uint8_t value);

    /**
     * @brief Create a StringKeyValue from a string key and a uint8_t value.
     *
     * @param key The key as a null-terminated string.
     * @param value The value as a uint8_t value (will be converted to string).
     * @return StringKeyValue with key copied and value converted to string.
     */
    static StringKeyValue makeParam(const char* key, char* value);

};