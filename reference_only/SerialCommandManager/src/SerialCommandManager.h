#pragma once


#include <stdlib.h>
#include <Arduino.h>


#if (defined(ARDUINO) && ARDUINO >= 155) || defined(ESP8266)
 #define YIELD yield();
#else
 #define YIELD
#endif

const uint8_t MaximumParameterCount = 5;
const uint8_t DefaultMaxCommandLength = 20;
const uint8_t DefaultMaxParamKeyLength = 10;
const uint8_t DefaultMaxParamValueLength = 64;
const uint8_t DefaultMaxMessageLength = 128;

/**
 * @brief Structure representing a key/value parameter pair.
 * 
 * This structure is used to hold individual parameters parsed from a command message.
 */
typedef struct StringKeyValue {
    char key[DefaultMaxParamKeyLength + 1];
    char value[DefaultMaxParamValueLength + 1];
} keyAndValue;

/**
 * @brief Callback function type for message reception.
 * 
 * @param sender Pointer to the SerialCommandManager instance that received the message.
 */
typedef void (*MessageReceivedCallback)(class SerialCommandManager* sender);

/**
 * @brief Interface for handling serial commands.
 * 
 * This is an interface class that defines how command handlers should behave. It's meant to be implemented by any class that wants to handle specific serial commands.
 *
 * Responsibilities:
 * Command Handling: Implements handleCommand() to define how to respond when a command is received.
 *
 * Command Support Declaration: Implements supportedCommands() to list which commands the handler can process.
 *
 * Command Matching: Includes a helper method supportsCommand() to check if a given command is supported.
 *
 * Use Case:
 * You'd create one or more classes that inherit from ISerialCommandHandler to handle different command types. For example, 
 * a MotorHandler might respond to "MOVE" or "STOP" commands.
*/
class ISerialCommandHandler {
public:
    /**
     * @brief Called when a command matching one of the supported commands arrives.
     * 
     * @param sender Pointer to the SerialCommandManager instance that received the command.
     * @param command The command string that was received.
     * @param params Array of key-value parameter pairs.
     * @param paramCount Number of parameters in the array.
     * @return true if the command was handled successfully, false otherwise.
     */
    virtual bool handleCommand(SerialCommandManager* sender, const char* command, const StringKeyValue params[], uint8_t paramCount) = 0;

    /**
     * @brief Returns a list of supported command tokens.
     * 
     * @param count Reference to store the number of supported commands.
     * @return const char* const* Array of supported commands (uppercase, trimmed).
     */
    virtual const char* const* supportedCommands(size_t& count) const = 0;

    /**
     * @brief Checks if this handler supports a specific command.
     * 
     * @param command The command string to check.
     * @return true if the command is supported, false otherwise.
     */
    virtual bool supportsCommand(const char* command) const {
        if (!command) return false;
        
        size_t count;
        const char* const* cmds = supportedCommands(count);
        for (size_t i = 0; i < count; ++i) {
            if (strcmp(cmds[i], command) == 0) return true;
        }
        return false;
    }

    virtual ~ISerialCommandHandler() {}
};

/**
 * @brief Manages serial command parsing and dispatching to registered handlers.
 * 
 * This is the core class that manages serial communication, parses incoming messages, and dispatches commands to the appropriate handlers.
 *
 * Responsibilities:
 * Message Reception: Reads from a Stream (like Serial) and parses incoming messages.
 *
 * Command Parsing: Extracts the command and its parameters from the message.
 *
 * Handler Dispatching: Calls the appropriate handler based on the command received.
 *
 * Timeout Handling: Detects if a message took too long to arrive.
 *
 * Message Sending: Can send debug, error, or command messages back over the serial port.
 *
 * Handler Registration: Accepts an array of ISerialCommandHandler objects to delegate command handling.
 *
 * Key Features:
 * Supports up to 5 parameters per command.
 *
 * Customizable message format using terminator, command separator, and parameter separator.
 *
 * Optional debug mode for verbose output.
 *
 * Callback mechanism (MessageReceivedCallback) for external notification when a message is received.
 */
class SerialCommandManager
{
    friend class DebugHandler;
private:
    ISerialCommandHandler** _handlerObjects = nullptr;
    size_t _handlerCount = 0;
    bool _readingMessage = false;
    bool _isParsingCommand = true;
    bool _isParsingParamName = true;
    unsigned long _lastCharTime = 0;
    
    // Buffer management
    char* _incomingMessage;        // Dynamic buffer for incoming message
    char* _command;                // Dynamic buffer for parsed command
    char* _rawMessage;             // Dynamic buffer for raw message
    char* _sendBuffer;             // Reusable send buffer (avoids repeated heap allocation)
    uint8_t _maxCommandLength;     // Max command buffer size
    uint8_t _maxMessageLength;     // Max message buffer size
    
    Stream* _serialPort;
    StringKeyValue _params[MaximumParameterCount];
    uint8_t _paramCount;
    unsigned long _serialTimeout;
    bool _messageTimeout;
    char _terminator;
    char _commandSeparator;
    char _paramSeparator;
	char _keyValueSeparator;
    bool _isDebug;
    MessageReceivedCallback _messageReceivedCallback;

    /**
     * @brief Processes the incoming message and dispatches to handlers.
     * 
     * @return true if the message was processed successfully, false otherwise.
     */
    bool processMessage();

    /**
     * @brief Sends a message over the serial port.
     * 
     * @param messageType The type of message (e.g., "DEBUG", "ERROR").
     * @param message The message content.
     * @param identifier Optional identifier for the message.
     */
    void sendMessage(const char* messageType, const char* message, const char* identifier);

public:
    /**
     * @brief Constructs a SerialCommandManager instance.
     * 
     * @param serialPort Pointer to the Stream object for serial communication.
     * @param commandReceived Callback function for received commands.
     * @param terminator Character that terminates a command message.
     * @param commandSeparator Character that separates command from parameters.
     * @param paramSeparator Character that separates parameters.
	 * @param keyValueSeparator Character that separates keys from values in parameters.
     * @param timeoutMilliseconds Timeout for receiving a complete message.
     * @param maxCommandLength Maximum length for command names (default 20).
     * @param maxMessageLength Maximum total message length (default 128).
     */
    SerialCommandManager(Stream* serialPort, MessageReceivedCallback commandReceived, 
        char terminator = '\n', char commandSeparator = ':', char paramSeparator = ';', 
		char keyValueSeparator = '=',
        unsigned long timeoutMilliseconds = 500, 
        uint8_t maxCommandLength = DefaultMaxCommandLength,
        uint8_t maxMessageLength = DefaultMaxMessageLength);

    /**
     * @brief Destructor for SerialCommandManager.
     */
    ~SerialCommandManager();

    /**
     * @brief Registers an array of command handler objects.
     * 
     * @param handlers Array of pointers to ISerialCommandHandler objects.
     * @param handlerCount Number of handlers in the array.
     */
    void registerHandlers(ISerialCommandHandler** handlers, size_t handlerCount);

    /**
     * @brief Reads and processes incoming serial commands.
     */
    void readCommands();

    /**
     * @brief Checks if the last message reception timed out.
     * 
     * @return true if a timeout occurred, false otherwise.
     */
    bool isTimeout();

    /**
     * @brief Gets the parsed command string from the last message.
     * 
     * @return Pointer to the command string buffer.
     */
    const char* getCommand();

    /**
     * @brief Gets a parsed key/value argument by index.
     * 
     * @param index Index of the argument to retrieve.
     * @return Pointer to the key/value pair at the specified index, or nullptr if invalid.
     */
    const StringKeyValue* getArgs(uint8_t index);

    /**
     * @brief Gets the number of parsed arguments in the last message.
     * 
     * @return The argument count.
     */
    uint8_t getArgCount();

    /**
     * @brief Gets the raw message string as received.
     * 
     * @return Pointer to the raw message buffer.
     */
    const char* getRawMessage();

    /**
     * @brief Sends a command message over the serial port.
     * 
     * @param header The command header string.
     * @param message The message content.
     * @param identifier Optional identifier for the message.
     * @param params Optional array of key/value parameters.
     * @param argLength Number of parameters in the array.
     */
    void sendCommand(const char* header, const char* message, const char* identifier = "", const StringKeyValue* params = nullptr, uint8_t argLength = 0);

    /**
     * @brief Sends a debug message over the serial port.
     * 
     * @param message The debug message content.
     * @param identifier Optional identifier for the message.
     */
    void sendDebug(const char* message, const char* identifier = "");

    /**
     * @brief Sends a debug message over the serial port using Flash string.
     *
     * @param message The debug message content stored in program memory.
     * @param identifier Optional identifier for the message stored in program memory.
     */
    void sendDebug(const __FlashStringHelper* message, const __FlashStringHelper* identifier = nullptr);

    /**
     * @brief Sends a debug message over the serial port with Flash string identifier.
     *
     * @param message The debug message content.
     * @param identifier Identifier for the message stored in program memory.
     */
    void sendDebug(const char* message, const __FlashStringHelper* identifier);

    /**
     * @brief Sends an error message over the serial port.
     * 
     * @param message The error message content.
     * @param identifier Optional identifier for the message.
     */
    void sendError(const char* message, const char* identifier = "");

    /**
     * @brief Sends an error message over the serial port using Flash string.
     *
     * @param message The error message content stored in program memory.
     * @param identifier Optional identifier for the message stored in program memory.
     */
    void sendError(const __FlashStringHelper* message, const __FlashStringHelper* identifier = nullptr);

    /**
     * @brief Sends an error message over the serial port with Flash string identifier.
     *
     * @param message The error message content.
     * @param identifier Identifier for the message stored in program memory.
     */
    void sendError(const char* message, const __FlashStringHelper* identifier);

    /**
     * @brief Enables or disables debug mode programmatically.
     *
     * @param enabled true to enable debug output, false to disable.
     */
    void setDebug(bool enabled);
};

