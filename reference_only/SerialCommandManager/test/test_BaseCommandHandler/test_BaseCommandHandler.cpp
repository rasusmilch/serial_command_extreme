#include <gtest/gtest.h>
#include <ArduinoFake.h>
#include <string.h>
#include "BaseCommandHandler.h"
#include "SerialCommandManager.h"

// Concrete handler for testing makeParam methods
class TestCommandHandler : public BaseCommandHandler {
public:
    bool handleCommand(SerialCommandManager* sender, const char* command, 
                      const StringKeyValue params[], uint8_t paramCount) override {
        return true;
    }

    const char* const* supportedCommands(size_t& count) const override {
        static const char* cmds[] = { "TEST" };
        count = 1;
        return cmds;
    }

    // Expose protected methods for testing
    using BaseCommandHandler::makeParam;
};

class BaseCommandHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        ArduinoFakeReset();
        handler = new TestCommandHandler();
    }

    void TearDown() override {
        delete handler;
    }

    TestCommandHandler* handler;
};

// ============================================================================
// makeParam Tests - These are safe to test without mocking
// ============================================================================

TEST_F(BaseCommandHandlerTest, MakeParam_UInt8KeyAndValue_CreatesValidParam) {
    StringKeyValue param = handler->makeParam((uint8_t)1, (uint8_t)42);
    
    EXPECT_STREQ(param.key, "1");
    EXPECT_STREQ(param.value, "42");
}

TEST_F(BaseCommandHandlerTest, MakeParam_UInt8KeyAndValue_ZeroValues) {
    StringKeyValue param = handler->makeParam((uint8_t)0, (uint8_t)0);
    
    EXPECT_STREQ(param.key, "0");
    EXPECT_STREQ(param.value, "0");
}

TEST_F(BaseCommandHandlerTest, MakeParam_UInt8KeyAndValue_MaxValues) {
    StringKeyValue param = handler->makeParam((uint8_t)255, (uint8_t)255);
    
    EXPECT_STREQ(param.key, "255");
    EXPECT_STREQ(param.value, "255");
}

TEST_F(BaseCommandHandlerTest, MakeParam_UInt8KeyAndStringValue_CreatesValidParam) {
    StringKeyValue param = handler->makeParam((uint8_t)2, "test_value");
    
    EXPECT_STREQ(param.key, "2");
    EXPECT_STREQ(param.value, "test_value");
}

TEST_F(BaseCommandHandlerTest, MakeParam_UInt8KeyAndNullString_HandlesGracefully) {
    StringKeyValue param = handler->makeParam((uint8_t)3, static_cast<const char*>(nullptr));
    
    EXPECT_STREQ(param.key, "3");
    EXPECT_STREQ(param.value, "");
}

TEST_F(BaseCommandHandlerTest, MakeParam_UInt8KeyAndEmptyString_CreatesValidParam) {
    StringKeyValue param = handler->makeParam((uint8_t)4, "");
    
    EXPECT_STREQ(param.key, "4");
    EXPECT_STREQ(param.value, "");
}

TEST_F(BaseCommandHandlerTest, MakeParam_StringKeyAndUInt8Value_CreatesValidParam) {
    // FIX: Use key that fits within DefaultMaxParamKeyLength (10 chars)
    StringKeyValue param = handler->makeParam("temp", (uint8_t)25);
    
    EXPECT_STREQ(param.key, "temp");
    EXPECT_STREQ(param.value, "25");
}

TEST_F(BaseCommandHandlerTest, MakeParam_StringKeyAndUInt8Value_ZeroValue) {
    StringKeyValue param = handler->makeParam("count", (uint8_t)0);
    
    EXPECT_STREQ(param.key, "count");
    EXPECT_STREQ(param.value, "0");
}

TEST_F(BaseCommandHandlerTest, MakeParam_StringKeyAndIntValue_PositiveNumber) {
    StringKeyValue param = handler->makeParam("sensor", 12345);
    
    EXPECT_STREQ(param.key, "sensor");
    EXPECT_STREQ(param.value, "12345");
}

TEST_F(BaseCommandHandlerTest, MakeParam_StringKeyAndIntValue_NegativeNumber) {
    StringKeyValue param = handler->makeParam("sensor", -100);
    
    EXPECT_STREQ(param.key, "sensor");
    EXPECT_STREQ(param.value, "-100");
}

TEST_F(BaseCommandHandlerTest, MakeParam_StringKeyAndIntValue_Zero) {
    StringKeyValue param = handler->makeParam("value", 0);
    
    EXPECT_STREQ(param.key, "value");
    EXPECT_STREQ(param.value, "0");
}

TEST_F(BaseCommandHandlerTest, MakeParam_StringKeyAndIntValue_LargePositive) {
    StringKeyValue param = handler->makeParam("count", 32767);
    
    EXPECT_STREQ(param.key, "count");
    EXPECT_STREQ(param.value, "32767");
}

TEST_F(BaseCommandHandlerTest, MakeParam_StringKeyAndIntValue_LargeNegative) {
    StringKeyValue param = handler->makeParam("offset", -32768);
    
    EXPECT_STREQ(param.key, "offset");
    EXPECT_STREQ(param.value, "-32768");
}

TEST_F(BaseCommandHandlerTest, MakeParam_StringKeyAndCharPtrValue_CreatesValidParam) {
    char value[] = "dynamic_value";
    StringKeyValue param = handler->makeParam("name", value);
    
    EXPECT_STREQ(param.key, "name");
    EXPECT_STREQ(param.value, "dynamic_value");
}

TEST_F(BaseCommandHandlerTest, MakeParam_StringKeyAndCharPtrValue_EmptyString) {
    char value[] = "";
    StringKeyValue param = handler->makeParam("empty", value);
    
    EXPECT_STREQ(param.key, "empty");
    EXPECT_STREQ(param.value, "");
}

TEST_F(BaseCommandHandlerTest, MakeParam_NullStringKey_HandlesGracefully) {
    StringKeyValue param = handler->makeParam(static_cast<const char*>(nullptr), (uint8_t)10);
    
    EXPECT_STREQ(param.key, "");
    EXPECT_STREQ(param.value, "10");
}

TEST_F(BaseCommandHandlerTest, MakeParam_EmptyStringKey_CreatesValidParam) {
    StringKeyValue param = handler->makeParam("", (uint8_t)10);
    
    EXPECT_STREQ(param.key, "");
    EXPECT_STREQ(param.value, "10");
}

TEST_F(BaseCommandHandlerTest, MakeParam_LongStringKey_Truncates) {
    char longKey[50];
    memset(longKey, 'K', sizeof(longKey) - 1);
    longKey[sizeof(longKey) - 1] = '\0';
    
    StringKeyValue param = handler->makeParam(longKey, (uint8_t)1);
    
    EXPECT_EQ(strlen(param.key), DefaultMaxParamKeyLength);
    EXPECT_STREQ(param.value, "1");
    // Verify null termination
    EXPECT_EQ(param.key[DefaultMaxParamKeyLength], '\0');
}

TEST_F(BaseCommandHandlerTest, MakeParam_LongStringValue_Truncates) {
    char longValue[100];
    memset(longValue, 'V', sizeof(longValue) - 1);
    longValue[sizeof(longValue) - 1] = '\0';
    
    StringKeyValue param = handler->makeParam("key", longValue);
    
    EXPECT_STREQ(param.key, "key");
    EXPECT_EQ(strlen(param.value), DefaultMaxParamValueLength);
    // Verify null termination
    EXPECT_EQ(param.value[DefaultMaxParamValueLength], '\0');
}

TEST_F(BaseCommandHandlerTest, MakeParam_ExactlyMaxKeyLength_Works) {
    char exactKey[DefaultMaxParamKeyLength + 1];
    memset(exactKey, 'K', DefaultMaxParamKeyLength);
    exactKey[DefaultMaxParamKeyLength] = '\0';
    
    StringKeyValue param = handler->makeParam(exactKey, (uint8_t)1);
    
    EXPECT_EQ(strlen(param.key), DefaultMaxParamKeyLength);
}

TEST_F(BaseCommandHandlerTest, MakeParam_ExactlyMaxValueLength_Works) {
    char exactValue[DefaultMaxParamValueLength + 1];
    memset(exactValue, 'V', DefaultMaxParamValueLength);
    exactValue[DefaultMaxParamValueLength] = '\0';
    
    StringKeyValue param = handler->makeParam("key", exactValue);
    
    EXPECT_EQ(strlen(param.value), DefaultMaxParamValueLength);
}

TEST_F(BaseCommandHandlerTest, MakeParam_SpecialCharactersInKey_PreservesChars) {
    // FIX: Use key that fits within 10 chars and test that special chars are preserved
    StringKeyValue param = handler->makeParam("key-sp.ch", (uint8_t)1);
    
    EXPECT_STREQ(param.key, "key-sp.ch");
    EXPECT_STREQ(param.value, "1");
}

TEST_F(BaseCommandHandlerTest, MakeParam_SpecialCharactersInValue_PreservesChars) {
    char value[] = "value-with_special.chars!@#";
    StringKeyValue param = handler->makeParam("key", value);
    
    EXPECT_STREQ(param.value, "value-with_special.chars!@#");
}

TEST_F(BaseCommandHandlerTest, MakeParam_NullCharPtrValue_HandlesGracefully) {
    StringKeyValue param = handler->makeParam("key", static_cast<char*>(nullptr));
    
    EXPECT_STREQ(param.key, "key");
    EXPECT_STREQ(param.value, "");
}

// ============================================================================
// Test key truncation behavior specifically
// ============================================================================

TEST_F(BaseCommandHandlerTest, MakeParam_KeyExceedsMaxLength_TruncatesCorrectly) {
    // "temperature" is 11 chars, should be truncated to 10
    StringKeyValue param = handler->makeParam("temperature", (uint8_t)25);
    
    // Should be truncated to first 10 characters: "temperatur"
    EXPECT_STREQ(param.key, "temperatur");
    EXPECT_STREQ(param.value, "25");
    EXPECT_EQ(strlen(param.key), DefaultMaxParamKeyLength);
}

TEST_F(BaseCommandHandlerTest, MakeParam_VeryLongKeyWithSpecialChars_TruncatesAndPreserves) {
    // "key_with-special.chars" is 22 chars, should be truncated to 10
    StringKeyValue param = handler->makeParam("key_with-special.chars", (uint8_t)1);
    
    // Should be truncated to first 10 characters: "key_with-s"
    EXPECT_STREQ(param.key, "key_with-s");
    EXPECT_STREQ(param.value, "1");
    EXPECT_EQ(strlen(param.key), DefaultMaxParamKeyLength);
}

// ============================================================================
// Interface Tests
// ============================================================================

TEST_F(BaseCommandHandlerTest, SupportsCommand_RegisteredCommand_ReturnsTrue) {
    EXPECT_TRUE(handler->supportsCommand("TEST"));
}

TEST_F(BaseCommandHandlerTest, SupportsCommand_UnregisteredCommand_ReturnsFalse) {
    EXPECT_FALSE(handler->supportsCommand("UNKNOWN"));
}

TEST_F(BaseCommandHandlerTest, SupportsCommand_NullCommand_ReturnsFalse) {
    EXPECT_FALSE(handler->supportsCommand(nullptr));
}

TEST_F(BaseCommandHandlerTest, SupportsCommand_EmptyCommand_ReturnsFalse) {
    EXPECT_FALSE(handler->supportsCommand(""));
}

TEST_F(BaseCommandHandlerTest, SupportsCommand_CaseSensitive) {
    // Assuming commands are case-sensitive
    EXPECT_FALSE(handler->supportsCommand("test"));
    EXPECT_FALSE(handler->supportsCommand("Test"));
}

TEST_F(BaseCommandHandlerTest, SupportedCommands_ReturnsCorrectCount) {
    size_t count = 0;
    const char* const* cmds = handler->supportedCommands(count);
    
    EXPECT_EQ(count, 1);
    EXPECT_NE(cmds, nullptr);
}

TEST_F(BaseCommandHandlerTest, SupportedCommands_ReturnsValidArray) {
    size_t count = 0;
    const char* const* cmds = handler->supportedCommands(count);
    
    ASSERT_EQ(count, 1);
    EXPECT_STREQ(cmds[0], "TEST");
}

// ============================================================================
// Run all tests
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}