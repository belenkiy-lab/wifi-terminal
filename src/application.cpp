#include "application.hpp"

#ifdef NETWORK_ESP8266_ASYNC
#undef NETWORK_ESP8266_ASYNC
#endif
#ifdef NETWORK_ESP8266
#undef NETWORK_ESP8266
#endif
#ifdef NETWORK_W5100
#undef NETWORK_W5100
#endif
#ifdef NETWORK_ENC28J60
#undef NETWORK_ENC28J60
#endif
#ifdef NETWORK_ESP32
#undef NETWORK_ESP32
#endif
#ifdef NETWORK_ESP32_ETH
#undef NETWORK_ESP32_ETH
#endif
#include <SimpleFTPServer.h>

static uint32_t uartRxOverrunCount = 0;

enum class TerminalResponseType : uint8_t
{
    None,
    CursorPosition,
    DeviceStatus,
    DeviceAttributes
};

static const unsigned long TERMINAL_RESPONSE_TIMEOUT_MS = 500;
static uint8_t pendingCursorPositionResponses = 0;
static uint8_t pendingDeviceStatusResponses = 0;
static uint8_t pendingDeviceAttributeResponses = 0;
static unsigned long lastCursorPositionQueryMs = 0;
static unsigned long lastDeviceStatusQueryMs = 0;
static unsigned long lastDeviceAttributeQueryMs = 0;
static uint8_t terminalQueryParserState = 0;
static char terminalQueryParams[16];
static size_t terminalQueryParamsLength = 0;

static void noteExpectedTerminalResponse(TerminalResponseType type)
{
    unsigned long now = millis();
    switch (type)
    {
        case TerminalResponseType::CursorPosition:
            if (pendingCursorPositionResponses < 0xff)
                pendingCursorPositionResponses++;
            lastCursorPositionQueryMs = now;
            break;
        case TerminalResponseType::DeviceStatus:
            if (pendingDeviceStatusResponses < 0xff)
                pendingDeviceStatusResponses++;
            lastDeviceStatusQueryMs = now;
            break;
        case TerminalResponseType::DeviceAttributes:
            if (pendingDeviceAttributeResponses < 0xff)
                pendingDeviceAttributeResponses++;
            lastDeviceAttributeQueryMs = now;
            break;
        default:
            break;
    }
}

static bool terminalQueryParamsEqual(const char *value)
{
    size_t length = strlen(value);
    return terminalQueryParamsLength == length &&
           memcmp(terminalQueryParams, value, length) == 0;
}

static void observeTerminalQueries(const uint8_t *data, size_t length, bool tcpPrimaryActive)
{
    if (!data || !length)
        return;

    for (size_t i = 0; i < length; i++)
    {
        uint8_t value = data[i];

        if (terminalQueryParserState == 0)
        {
            if (value == 0x1b)
                terminalQueryParserState = 1;
            continue;
        }

        if (terminalQueryParserState == 1)
        {
            if (value == '[')
            {
                terminalQueryParserState = 2;
                terminalQueryParamsLength = 0;
            }
            else
            {
                if (value == 'Z' && tcpPrimaryActive)
                    noteExpectedTerminalResponse(TerminalResponseType::DeviceAttributes);
                terminalQueryParserState = value == 0x1b ? 1 : 0;
            }
            continue;
        }

        if (value == 0x1b)
        {
            terminalQueryParserState = 1;
            terminalQueryParamsLength = 0;
            continue;
        }

        if (value >= 0x30 && value <= 0x3f)
        {
            if (terminalQueryParamsLength < sizeof(terminalQueryParams))
                terminalQueryParams[terminalQueryParamsLength++] = static_cast<char>(value);
            continue;
        }

        if (value >= 0x20 && value <= 0x2f)
            continue;

        if (value >= 0x40 && value <= 0x7e)
        {
            if (tcpPrimaryActive)
            {
                if (value == 'n')
                {
                    if (terminalQueryParamsEqual("6") || terminalQueryParamsEqual("?6"))
                        noteExpectedTerminalResponse(TerminalResponseType::CursorPosition);
                    else if (terminalQueryParamsEqual("5") || terminalQueryParamsEqual("?5"))
                        noteExpectedTerminalResponse(TerminalResponseType::DeviceStatus);
                }
                else if (value == 'c')
                {
                    noteExpectedTerminalResponse(TerminalResponseType::DeviceAttributes);
                }
            }
            terminalQueryParserState = 0;
            terminalQueryParamsLength = 0;
            continue;
        }

        terminalQueryParserState = 0;
        terminalQueryParamsLength = 0;
    }
}

static TerminalResponseType classifyWebTerminalResponse(const uint8_t *data, size_t length)
{
    if (!data || length < 4 || data[0] != 0x1b || data[1] != '[')
        return TerminalResponseType::None;

    size_t pos = 2;
    bool privatePrefix = false;
    if (data[pos] == '?' || data[pos] == '>')
    {
        privatePrefix = true;
        pos++;
    }

    if (pos >= length - 1)
        return TerminalResponseType::None;

    bool sawDigit = false;
    bool sawSemicolon = false;
    for (size_t i = pos; i < length - 1; i++)
    {
        if (data[i] >= '0' && data[i] <= '9')
        {
            sawDigit = true;
            continue;
        }
        if (data[i] == ';')
        {
            sawSemicolon = true;
            continue;
        }
        return TerminalResponseType::None;
    }

    if (!sawDigit)
        return TerminalResponseType::None;

    uint8_t finalByte = data[length - 1];
    if (finalByte == 'R' && sawSemicolon)
        return TerminalResponseType::CursorPosition;
    if (finalByte == 'n')
        return TerminalResponseType::DeviceStatus;
    if (finalByte == 'c' && privatePrefix)
        return TerminalResponseType::DeviceAttributes;

    return TerminalResponseType::None;
}

static bool consumePendingResponse(uint8_t &pending, unsigned long lastQueryMs)
{
    if (!pending)
        return false;

    if (millis() - lastQueryMs > TERMINAL_RESPONSE_TIMEOUT_MS)
    {
        pending = 0;
        return false;
    }

    pending--;
    return true;
}

static bool shouldSuppressWebTerminalResponse(const uint8_t *data, size_t length, bool tcpPrimaryActive)
{
    if (!tcpPrimaryActive)
    {
        pendingCursorPositionResponses = 0;
        pendingDeviceStatusResponses = 0;
        pendingDeviceAttributeResponses = 0;
        return false;
    }

    switch (classifyWebTerminalResponse(data, length))
    {
        case TerminalResponseType::CursorPosition:
            return consumePendingResponse(pendingCursorPositionResponses, lastCursorPositionQueryMs);
        case TerminalResponseType::DeviceStatus:
            return consumePendingResponse(pendingDeviceStatusResponses, lastDeviceStatusQueryMs);
        case TerminalResponseType::DeviceAttributes:
            return consumePendingResponse(pendingDeviceAttributeResponses, lastDeviceAttributeQueryMs);
        default:
            return false;
    }
}

void changeBuilinLedState()
{
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
}

Application::Application()
{
    _settings = new Configuration();
    _terminalServer = new WiFiServer(DEFAULT_TERMINAL_SERVER_PORT);

    _webSockServer = new WebSocketsServer(WEBSOCKET_PORT_);

    _WebServer = new ESP8266WebServer(WEB_SERVER_PORT);
    _FTPServer = new FtpServer();
    _blinker = new Ticker();
}
bool Application::startAP()
{
    bool result = false;
    WiFi.mode(WIFI_AP);

    if (!WiFi.softAPConfig(_settings->APaddress, DEFAULT_AP_GATEWAY, DEFAULT_AP_MASK))
    {
        logger->println("AP Config Failed");
    }
    else if (WiFi.softAP(_settings->APSSID, _settings->APPassword, _settings->APchannel))
    {
        logger->println("\nnetwork " + _settings->APSSID + " running");
        logger->println("AP IP address: " + WiFi.softAPIP().toString());
        result = true;
    }
    else
    {
        logger->println("starting AP failed");
    }
    return result;
}
void Application::halt()
{
    _blinker->attach(BLINK_SPEED_FAST, changeBuilinLedState);
    logger->println("fatal error - rebooting...");
    delay(REBOOT_DELAY);

    ESP.reset();
}
String Application::getContentType(const String &filename)
{
    if (filename.endsWith(".html"))
        return "text/html";

    else if (filename.endsWith(".css"))
        return "text/css";

    else if (filename.endsWith(".js"))
        return "application/javascript";

    else if (filename.endsWith(".ico"))
        return "image/x-icon";

    return HTTP_TEXT_PLAIN;
}
void Application::handleNotFound()
{
    if (!handleFileRead(_WebServer->uri()))
    {
        _WebServer->send(HTTP_SERVER_NOT_FOUND_, FPSTR(HTTP_TEXT_PLAIN), FPSTR(HTTP_NOT_FOUND_TEXT));
    }
}
bool Application::handleFileRead(String path)
{
    bool success = false;
    logger->println("handle file read: " + path);
    if (path.endsWith("/"))
        path += "index.html";

    String contentType = getContentType(path);
    if (LittleFS.exists(path))
    {
        File file = LittleFS.open(path, "r");
        _WebServer->streamFile(file, contentType);
        file.close();

        logger->println(String("Sent file: ") + path);
        success = true;
    }
    else
        logger->println(String("File Not Found: ") + path);

    return success;
}
void Application::handleSettingsSave()
{
    logger->println("handle settings save");
    std::map<String, String> settingsMap;
    for (int i = 0; i < _WebServer->args(); i++)
    {
        settingsMap[_WebServer->argName(i)] = _WebServer->arg(i);
    }
    _settings->fromMapping(settingsMap);
    JSONConfig::write(FPSTR(CONFIG_FILENAME), *_settings, CONFIG_SIZE);
    _WebServer->send(HTTP_SERVER_OK_, FPSTR(HTTP_TEXT_PLAIN), FPSTR(AFTER_SAVING_MSG));

    delay(REBOOT_DELAY);
    ESP.reset();
}
void Application::handleGetSettings()
{
    logger->println("handle get settings");
    String serializedData;
    DynamicJsonDocument doc(CONFIG_SIZE);

    _settings->serialize(doc);
    serializeJson(doc, serializedData);
    _WebServer->send(HTTP_SERVER_OK_, FPSTR(HTTP_TEXT_PLAIN), serializedData);
}
size_t Application::getDebugLogSize()
{
    size_t size = _uartDebugLogBytes + _uartDebugBufferLen;
    if (!_uartDebugEnabled && LittleFS.exists(FPSTR(DEBUG_LOG_FILENAME)))
    {
        File file = LittleFS.open(FPSTR(DEBUG_LOG_FILENAME), "r");
        if (file)
        {
            size = file.size();
            file.close();
        }
    }
    return size;
}
void Application::handleGetStatus()
{
    FSInfo fsInfo;
    LittleFS.info(fsInfo);

    String serializedData;
    StaticJsonDocument<512> doc;
    doc["uptime"] = millis() / 1000UL;
    doc["reset_reason"] = ESP.getResetReason();
    doc["uart_rx_overruns"] = uartRxOverrunCount;
    doc["littlefs_total"] = fsInfo.totalBytes;
    doc["littlefs_used"] = fsInfo.usedBytes;
    doc["littlefs_free"] = fsInfo.totalBytes >= fsInfo.usedBytes ? fsInfo.totalBytes - fsInfo.usedBytes : 0;
    doc["debug_enabled"] = _uartDebugEnabled;
    doc["debug_limit_reached"] = _uartDebugLimitReached;
    doc["debug_log_bytes"] = getDebugLogSize();
    doc["debug_log_limit"] = _uartDebugLogLimit;
    doc["debug_dropped_records"] = _uartDebugDroppedRecords;
    serializeJson(doc, serializedData);
    _WebServer->send(HTTP_SERVER_OK_, FPSTR(HTTP_APPLICATION_JSON), serializedData);
}
void Application::appendDebugText(const char *text)
{
    if (!_uartDebugEnabled || !text)
        return;

    size_t length = strlen(text);
    if (length >= DEBUG_RAM_BUFFER_SIZE)
    {
        _uartDebugDroppedRecords++;
        return;
    }

    if (_uartDebugBufferLen + length > DEBUG_RAM_BUFFER_SIZE)
        flushDebugLog(true);

    if (!_uartDebugEnabled || _uartDebugBufferLen + length > DEBUG_RAM_BUFFER_SIZE)
    {
        _uartDebugDroppedRecords++;
        return;
    }

    memcpy(_uartDebugBuffer + _uartDebugBufferLen, text, length);
    _uartDebugBufferLen += length;
}
void Application::appendDebugRecord(const char *source, const uint8_t *data, size_t length)
{
    if (!_uartDebugEnabled || !data || !length)
        return;

    const size_t bytesPerRecord = 96;
    size_t offset = 0;
    while (offset < length && _uartDebugEnabled)
    {
        size_t chunkLength = std::min(bytesPerRecord, length - offset);
        char line[640];
        size_t pos = 0;
        unsigned long now = millis();
        int headerLen = snprintf(line, sizeof(line), "%lu.%03lu %-7s ", now / 1000UL, now % 1000UL, source);
        if (headerLen < 0)
            return;
        pos = static_cast<size_t>(headerLen);

        for (size_t i = 0; i < chunkLength && pos + 5 < sizeof(line); i++)
        {
            uint8_t value = data[offset + i];
            if (value == '\\' || value == '"')
            {
                line[pos++] = '\\';
                line[pos++] = static_cast<char>(value);
            }
            else if (value == '\r')
            {
                line[pos++] = '\\'; line[pos++] = 'r';
            }
            else if (value == '\n')
            {
                line[pos++] = '\\'; line[pos++] = 'n';
            }
            else if (value == '\t')
            {
                line[pos++] = '\\'; line[pos++] = 't';
            }
            else if (value >= 0x20 && value <= 0x7e)
            {
                line[pos++] = static_cast<char>(value);
            }
            else
            {
                int written = snprintf(line + pos, sizeof(line) - pos, "\\x%02X", value);
                if (written < 0)
                    break;
                pos += static_cast<size_t>(written);
            }
        }
        line[pos++] = '\n';
        line[pos] = '\0';
        appendDebugText(line);
        offset += chunkLength;
    }
}
void Application::flushDebugLog(bool force)
{
    if (!_uartDebugEnabled || !_uartDebugBufferLen)
        return;

    unsigned long now = millis();
    if (!force && _uartDebugBufferLen < DEBUG_FLUSH_THRESHOLD &&
        now - _uartDebugLastFlushMs < DEBUG_FLUSH_INTERVAL_MS)
        return;

    if (_uartDebugLogBytes >= _uartDebugLogLimit)
    {
        _uartDebugBufferLen = 0;
        _uartDebugLimitReached = true;
        _uartDebugEnabled = false;
        return;
    }

    size_t remaining = _uartDebugLogLimit - _uartDebugLogBytes;
    size_t writeLength = std::min(_uartDebugBufferLen, remaining);
    File file = LittleFS.open(FPSTR(DEBUG_LOG_FILENAME), "a");
    if (!file)
    {
        _uartDebugDroppedRecords++;
        _uartDebugBufferLen = 0;
        _uartDebugEnabled = false;
        return;
    }

    size_t written = file.write(reinterpret_cast<const uint8_t *>(_uartDebugBuffer), writeLength);
    file.close();
    _uartDebugLogBytes += written;
    _uartDebugLastFlushMs = now;

    if (written < writeLength)
    {
        _uartDebugDroppedRecords++;
        _uartDebugEnabled = false;
    }

    if (writeLength < _uartDebugBufferLen || _uartDebugLogBytes >= _uartDebugLogLimit)
    {
        _uartDebugLimitReached = true;
        _uartDebugEnabled = false;
    }
    _uartDebugBufferLen = 0;
}
void Application::handleDebugStart()
{
    if (_uartDebugEnabled)
    {
        _WebServer->send(HTTP_SERVER_OK_, FPSTR(HTTP_APPLICATION_JSON), "{\"enabled\":true}");
        return;
    }

    if (LittleFS.exists(FPSTR(DEBUG_LOG_FILENAME)))
        LittleFS.remove(FPSTR(DEBUG_LOG_FILENAME));

    FSInfo fsInfo;
    LittleFS.info(fsInfo);
    size_t freeBytes = fsInfo.totalBytes >= fsInfo.usedBytes ? fsInfo.totalBytes - fsInfo.usedBytes : 0;
    size_t availableForLog = freeBytes > DEBUG_FS_RESERVE_BYTES ? freeBytes - DEBUG_FS_RESERVE_BYTES : 0;
    _uartDebugLogLimit = std::min(DEBUG_MAX_LOG_BYTES, availableForLog);
    _uartDebugLogBytes = 0;
    _uartDebugBufferLen = 0;
    _uartDebugDroppedRecords = 0;
    _uartDebugLimitReached = false;
    _uartDebugLastFlushMs = millis();

    if (!_uartDebugLogLimit)
    {
        _WebServer->send(507, FPSTR(HTTP_APPLICATION_JSON), "{\"enabled\":false,\"error\":\"not enough LittleFS space\"}");
        return;
    }

    _uartDebugEnabled = true;
    appendDebugText("# Wireless Terminal UART debug session\n");
    appendDebugText("# format: seconds.millis SOURCE escaped-bytes\n");
    _WebServer->send(HTTP_SERVER_OK_, FPSTR(HTTP_APPLICATION_JSON), "{\"enabled\":true}");
}
void Application::handleDebugStop()
{
    if (_uartDebugEnabled)
    {
        flushDebugLog(true);
        _uartDebugEnabled = false;
    }
    _WebServer->send(HTTP_SERVER_OK_, FPSTR(HTTP_APPLICATION_JSON), "{\"enabled\":false}");
}
void Application::handleTerminalClient()
{
    if (_terminalServer->hasClient())
    {
        if (_terminalClient.connected())
        {
            _terminalClient.println("new client connected, current connection aborted");
            _terminalClient.stop();
        }
        _terminalClient = _terminalServer->accept();
        logger->println("Client connected to telnet server");
        const uint8_t wakeup = '\r';
        appendDebugRecord("TX SYS", &wakeup, 1);
        Serial.write(wakeup);
    }

    size_t tcpToSerial = 0;
    uint8_t debugBuffer[TCP_TO_SERIAL_MAX_PER_LOOP];
    while (_terminalClient.available() && Serial.availableForWrite() > 0 &&
           tcpToSerial < TCP_TO_SERIAL_MAX_PER_LOOP)
    {
        uint8_t value = static_cast<uint8_t>(_terminalClient.read());
        debugBuffer[tcpToSerial] = value;
        Serial.write(value);
        tcpToSerial++;
    }
    if (tcpToSerial)
        appendDebugRecord("TX TCP", debugBuffer, tcpToSerial);
}
void Application::handleSerialInput()
{
    size_t bufferLen = std::min((size_t)Serial.available(), STACK_MAX_SIZE);
    if (!bufferLen)
        return;

    if (_terminalClient.connected())
    {
        int tcpAvailable = _terminalClient.availableForWrite();
        if (tcpAvailable <= 0)
            return;
        bufferLen = std::min(bufferLen, static_cast<size_t>(tcpAvailable));
    }

    uint8_t buffer[STACK_MAX_SIZE];
    size_t serialGotBytesCount = Serial.readBytes(buffer, bufferLen);
    if (!serialGotBytesCount)
        return;

    appendDebugRecord("RX UART", buffer, serialGotBytesCount);
    observeTerminalQueries(buffer, serialGotBytesCount, _terminalClient.connected());

    if (_terminalClient.connected())
    {
        size_t sent = _terminalClient.write(buffer, serialGotBytesCount);
        if (sent != serialGotBytesCount)
        {
            logger->printf("tcp write mismatch: serial-read:%zd tcp-write:%zd\n",
                           serialGotBytesCount, sent);
        }
    }

    if (!_webSockServer->broadcastBIN(buffer, serialGotBytesCount))
    {
        logger->println("websocket broadcast failed");
    }
}
void Application::handleWebSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
    if (type == WStype_CONNECTED)
    {
        logger->printf("ws client #%u connected from:", num);
        logger->println(_webSockServer->remoteIP(num));
        const uint8_t wakeup = '\r';
        appendDebugRecord("TX SYS", &wakeup, 1);
        Serial.write(wakeup);
    }
    else if (type == WStype_DISCONNECTED)
    {
        logger->printf("ws client #%u disconnected\n", num);
    }
    else if (type == WStype_TEXT || type == WStype_BIN)
    {
        if (shouldSuppressWebTerminalResponse(payload, length, _terminalClient.connected()))
            return;

        logger->write(payload, length);
        appendDebugRecord("TX WEB", payload, length);
        Serial.write(payload, length);
    }
}
void Application::initialize()
{
    pinMode(LED_BUILTIN, OUTPUT);
    logger->begin(DEFAULT_BAUD_LOGGER);
    logger->println(FPSTR(WELCOME_STRING));

    if (!LittleFS.begin())
    {
        logger->println("failed to mount FS");
        halt();
    }
    JSONConfig::read(FPSTR(CONFIG_FILENAME), *_settings, CONFIG_SIZE);
    Serial.begin(_settings->serialBaud);
    Serial.swap();
    Serial.flush();
    Serial.setRxBufferSize(RX_BUFFER_SIZE);

    if (startAP())
        _blinker->attach(BLINK_SPEED_MIDDLE, changeBuilinLedState);
    else
        halt();

    _WebServer->on(FPSTR(HTTP_SAVE_LINK), [&]() mutable { this->handleSettingsSave(); });
    _WebServer->on(FPSTR(HTTP_CONF_LINK), [&]() mutable { this->handleGetSettings(); });
    _WebServer->on(FPSTR(HTTP_STATUS_LINK), [&]() mutable { this->handleGetStatus(); });
    _WebServer->on(FPSTR(HTTP_DEBUG_START_LINK), HTTP_POST, [&]() mutable { this->handleDebugStart(); });
    _WebServer->on(FPSTR(HTTP_DEBUG_STOP_LINK), HTTP_POST, [&]() mutable { this->handleDebugStop(); });
    _WebServer->onNotFound([&]() mutable { handleNotFound();});
    _WebServer->begin();

    _terminalServer->begin();
    _terminalServer->setNoDelay(true);
    _FTPServer->begin(FTP_LOGIN_, FTP_PASSWORD_);
    _blinker->detach();

    _webSockServer->begin();
    _webSockServer->onEvent([&](uint8_t num, WStype_t type, uint8_t *payload, size_t length) mutable {
         this->handleWebSocketEvent(num, type, payload, length); });
}
void Application::mainloop()
{
    _webSockServer->loop();
    _FTPServer->handleFTP();
    _WebServer->handleClient();
    this->handleTerminalClient();
    this->handleSerialInput();
    flushDebugLog();

    if (Serial.hasOverrun())
    {
        uartRxOverrunCount++;
    }
}