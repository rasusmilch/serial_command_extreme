#include <gtest/gtest.h>
#include <ArduinoFake.h>
#include <string.h>
#include "SerialCommandManager.h"

// ============================================================================
// Test ISerialCommandHandler Interface
// ============================================================================

class SimpleTestHandler : public ISerialCommandHandler {
public:
    bool wasCalled;
    char lastCommand[32];
    uint8_t lastParamCount;

    SimpleTestHandler() : wasCalled(false), lastParamCount(0) {
        lastCommand[0] = '\0';
    }

    bool handleCommand(SerialCommandManager* sender, const char* command, 
                      const StringKeyValue params[], uint8_t paramCount) override {
        wasCalled = true;
        if (command) {
            strncpy(lastCommand, command, sizeof(lastCommand) - 1);
            lastCommand[sizeof(lastCommand) - 1] = '\0';
        }
        lastParamCount = paramCount;
        return true;
    }

    const char* const* supportedCommands(size_t& count) const override {
        static const char* cmds[] = { "TEST", "ECHO", "PING" };
        count = 3;
        return cmds;
    }
};

// ============================================================================
// Handler Interface Tests (No SerialCommandManager needed)
// ============================================================================

TEST(HandlerInterfaceTest, SupportsCommand_RegisteredCommand_ReturnsTrue) {
    SimpleTestHandler handler;
    
    EXPECT_TRUE(handler.supportsCommand("TEST"));
    EXPECT_TRUE(handler.supportsCommand("ECHO"));
    EXPECT_TRUE(handler.supportsCommand("PING"));
}

TEST(HandlerInterfaceTest, SupportsCommand_UnregisteredCommand_ReturnsFalse) {
    SimpleTestHandler handler;
    
    EXPECT_FALSE(handler.supportsCommand("UNKNOWN"));
    EXPECT_FALSE(handler.supportsCommand("INVALID"));
}

TEST(HandlerInterfaceTest, SupportsCommand_NullCommand_ReturnsFalse) {
    SimpleTestHandler handler;
    
    EXPECT_FALSE(handler.supportsCommand(nullptr));
}

TEST(HandlerInterfaceTest, SupportsCommand_EmptyCommand_ReturnsFalse) {
    SimpleTestHandler handler;
    
    EXPECT_FALSE(handler.supportsCommand(""));
}

TEST(HandlerInterfaceTest, SupportsCommand_CaseSensitive) {
    SimpleTestHandler handler;
    
    // Commands should be case-sensitive
    EXPECT_FALSE(handler.supportsCommand("test"));
    EXPECT_FALSE(handler.supportsCommand("Test"));
    EXPECT_FALSE(handler.supportsCommand("TeSt"));
}

TEST(HandlerInterfaceTest, SupportedCommands_ReturnsCorrectCount) {
    SimpleTestHandler handler;
    size_t count = 0;
    
    const char* const* cmds = handler.supportedCommands(count);
    
    EXPECT_EQ(count, 3);
    EXPECT_NE(cmds, nullptr);
}

TEST(HandlerInterfaceTest, SupportedCommands_ReturnsValidCommands) {
    SimpleTestHandler handler;
    size_t count = 0;
    
    const char* const* cmds = handler.supportedCommands(count);
    
    ASSERT_EQ(count, 3);
    EXPECT_STREQ(cmds[0], "TEST");
    EXPECT_STREQ(cmds[1], "ECHO");
    EXPECT_STREQ(cmds[2], "PING");
}

TEST(HandlerInterfaceTest, HandleCommand_SimpleCall_SetsFlags) {
    SimpleTestHandler handler;
    StringKeyValue params[1];
    
    bool result = handler.handleCommand(nullptr, "TEST", params, 0);
    
    EXPECT_TRUE(result);
    EXPECT_TRUE(handler.wasCalled);
    EXPECT_STREQ(handler.lastCommand, "TEST");
    EXPECT_EQ(handler.lastParamCount, 0);
}

TEST(HandlerInterfaceTest, HandleCommand_WithParams_StoresCount) {
    SimpleTestHandler handler;
    StringKeyValue params[3];
    strncpy(params[0].key, "key1", sizeof(params[0].key));
    strncpy(params[0].value, "val1", sizeof(params[0].value));
    strncpy(params[1].key, "key2", sizeof(params[1].key));
    strncpy(params[1].value, "val2", sizeof(params[1].value));
    strncpy(params[2].key, "key3", sizeof(params[2].key));
    strncpy(params[2].value, "val3", sizeof(params[2].value));
    
    bool result = handler.handleCommand(nullptr, "ECHO", params, 3);
    
    EXPECT_TRUE(result);
    EXPECT_TRUE(handler.wasCalled);
    EXPECT_STREQ(handler.lastCommand, "ECHO");
    EXPECT_EQ(handler.lastParamCount, 3);
}

TEST(HandlerInterfaceTest, HandleCommand_NullCommand_HandlesGracefully) {
    SimpleTestHandler handler;
    StringKeyValue params[1];
    
    EXPECT_NO_THROW({
        handler.handleCommand(nullptr, nullptr, params, 0);
    });
    
    EXPECT_TRUE(handler.wasCalled);
}

TEST(HandlerInterfaceTest, HandleCommand_MaxParams_HandlesCorrectly) {
    SimpleTestHandler handler;
    StringKeyValue params[MaximumParameterCount];
    
    for (uint8_t i = 0; i < MaximumParameterCount; i++) {
        snprintf(params[i].key, sizeof(params[i].key), "k%d", i);
        snprintf(params[i].value, sizeof(params[i].value), "v%d", i);
    }
    
    bool result = handler.handleCommand(nullptr, "TEST", params, MaximumParameterCount);
    
    EXPECT_TRUE(result);
    EXPECT_EQ(handler.lastParamCount, MaximumParameterCount);
}

// ============================================================================
// StringKeyValue Structure Tests
// ============================================================================

TEST(StringKeyValueTest, Structure_HasCorrectSizes) {
    StringKeyValue param;
    
    // Verify the structure has expected sizes
    EXPECT_EQ(sizeof(param.key), DefaultMaxParamKeyLength + 1);
    EXPECT_EQ(sizeof(param.value), DefaultMaxParamValueLength + 1);
}

TEST(StringKeyValueTest, Structure_CanBeInitialized) {
    StringKeyValue param = {};
    
    EXPECT_EQ(param.key[0], '\0');
    EXPECT_EQ(param.value[0], '\0');
}

TEST(StringKeyValueTest, Structure_CanStoreLongKey) {
    StringKeyValue param = {};
    
    char longKey[DefaultMaxParamKeyLength + 10];
    memset(longKey, 'K', sizeof(longKey) - 1);
    longKey[sizeof(longKey) - 1] = '\0';
    
    strncpy(param.key, longKey, DefaultMaxParamKeyLength);
    param.key[DefaultMaxParamKeyLength] = '\0';
    
    EXPECT_EQ(strlen(param.key), DefaultMaxParamKeyLength);
}

TEST(StringKeyValueTest, Structure_CanStoreLongValue) {
    StringKeyValue param = {};
    
    char longValue[DefaultMaxParamValueLength + 10];
    memset(longValue, 'V', sizeof(longValue) - 1);
    longValue[sizeof(longValue) - 1] = '\0';
    
    strncpy(param.value, longValue, DefaultMaxParamValueLength);
    param.value[DefaultMaxParamValueLength] = '\0';
    
    EXPECT_EQ(strlen(param.value), DefaultMaxParamValueLength);
}

TEST(StringKeyValueTest, Structure_CanBeArrayed) {
    StringKeyValue params[MaximumParameterCount];
    
    for (uint8_t i = 0; i < MaximumParameterCount; i++) {
        snprintf(params[i].key, sizeof(params[i].key), "key%d", i);
        snprintf(params[i].value, sizeof(params[i].value), "value%d", i);
    }
    
    // Verify all were set correctly
    for (uint8_t i = 0; i < MaximumParameterCount; i++) {
        char expected_key[20];
        char expected_value[20];
        snprintf(expected_key, sizeof(expected_key), "key%d", i);
        snprintf(expected_value, sizeof(expected_value), "value%d", i);
        
        EXPECT_STREQ(params[i].key, expected_key);
        EXPECT_STREQ(params[i].value, expected_value);
    }
}

// ============================================================================
// Constants Tests
// ============================================================================

TEST(ConstantsTest, MaximumParameterCount_IsReasonable) {
    EXPECT_GT(MaximumParameterCount, 0);
    EXPECT_LE(MaximumParameterCount, 10); // Shouldn't be too large
}

TEST(ConstantsTest, DefaultMaxParamKeyLength_IsReasonable) {
    EXPECT_GT(DefaultMaxParamKeyLength, 0);
    EXPECT_LE(DefaultMaxParamKeyLength, 50);
}

TEST(ConstantsTest, DefaultMaxParamValueLength_IsReasonable) {
    EXPECT_GT(DefaultMaxParamValueLength, 0);
    EXPECT_LE(DefaultMaxParamValueLength, 255);
}

TEST(ConstantsTest, DefaultMaxCommandLength_IsReasonable) {
    EXPECT_GT(DefaultMaxCommandLength, 0);
    EXPECT_LE(DefaultMaxCommandLength, 50);
}

TEST(ConstantsTest, DefaultMaxMessageLength_IsReasonable) {
    EXPECT_GT(DefaultMaxMessageLength, 0);
    EXPECT_LE(DefaultMaxMessageLength, 512);
}

TEST(ConstantsTest, ParamKeyLength_IsSmallerThanValueLength) {
    EXPECT_LT(DefaultMaxParamKeyLength, DefaultMaxParamValueLength);
}

// ============================================================================
// Run all tests
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}