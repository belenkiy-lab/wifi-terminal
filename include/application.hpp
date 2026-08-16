#ifndef WIRELESS_TERMINAL_APPLICATION_H_INCLUDED
#define WIRELESS_TERMINAL_APPLICATION_H_INCLUDED
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <WebSocketsServer.h>
#include <DNSServer.h>
#include <Ticker.h>
#include <memory>
#include <map>

#include "logger.hpp"
#include "jsonconfig.hpp"

class FtpServer;

static const char AFTER_SAVING_MSG[] PROGMEM = "Rebooting...\n"
"Please plug-in 'Wireless Terminal' to device and connect with terminal (23 port)";

static const char WELCOME_STRING[] PROGMEM = "\nWireless terminal (SW version: 0.9)";
static const char INDEX_PAGE_FILENAME[] PROGMEM = "index.html";
static const char CONFIG_FILENAME[] PROGMEM = "/config.json";
static const char DEBUG_LOG_FILENAME[] PROGMEM = "/debug.log";
static const char HTTP_TEXT_PLAIN[] PROGMEM = "text/plain";
static const char HTTP_TEXT_HTML[] PROGMEM = "text/html";
static const char HTTP_APPLICATION_JSON[] PROGMEM = "application/json";
static const char HTTP_NOT_FOUND_TEXT[] PROGMEM = "Not Found";
static const char HTTP_CONF_LINK[] PROGMEM = "/configure";
static const char HTTP_STATUS_LINK[] PROGMEM = "/status";
static const char HTTP_DEBUG_START_LINK[] PROGMEM = "/debug/start";
static const char HTTP_DEBUG_STOP_LINK[] PROGMEM = "/debug/stop";
static const char HTTP_SAVE_LINK[] PROGMEM = "/save";
static const char HTTP_ROOT_LINK[] PROGMEM = "/";
static const char FTP_LOGIN_[] = "admin";
static const char FTP_PASSWORD_[] = "admin";

const IPAddress DEFAULT_AP_GATEWAY(0, 0, 0, 0);
const IPAddress DEFAULT_AP_MASK(255, 255, 255, 0);
const size_t STACK_MAX_SIZE = 512;
const size_t TCP_TO_SERIAL_MAX_PER_LOOP = 64;
const size_t DEBUG_RAM_BUFFER_SIZE = 4096;
const size_t DEBUG_FLUSH_THRESHOLD = 2048;
const size_t DEBUG_FS_RESERVE_BYTES = 64 * 1024;
const size_t DEBUG_MAX_LOG_BYTES = 256 * 1024;
const unsigned long DEBUG_FLUSH_INTERVAL_MS = 250;
#define DEFAULT_TERMINAL_SERVER_PORT 23
#define WEB_SERVER_PORT 80
#define DNS_SERVER_PORT 53
#define CONFIG_SIZE 2048
#define BLINK_SPEED_FAST 0.1f
#define BLINK_SPEED_MIDDLE 0.5f
#define BLINK_SPEED_SLOW 0.8f
#define HTTP_SERVER_OK_ 200
#define HTTP_SERVER_NOT_FOUND_ 404
#define REBOOT_DELAY 5000
#define RX_BUFFER_SIZE 1024
#define LINE_MAX 80
#define WEBSOCKET_PORT_ 81

void changeBuilinLedState();

class Application
{
public:
    Application();
    void initialize();
    void mainloop();

protected:
    Configuration *_settings;
    WiFiClient _terminalClient;
    WiFiServer *_terminalServer;
    ESP8266WebServer *_WebServer;

    WebSocketsServer *_webSockServer;

    FtpServer *_FTPServer;
    Ticker *_blinker;

    void handleWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);
    String getContentType(const String &filename);
    bool handleFileRead(String path);
    void handleNotFound();
    void handleSerialInput();

    void handleTerminalClient();
    void handleSettingsSave();
    void handleGetSettings();
    void handleGetStatus();
    void handleDebugStart();
    void handleDebugStop();
    bool handleRoot();
    bool startAP();
    void halt();

    void appendDebugRecord(const char *source, const uint8_t *data, size_t length);
    void appendDebugText(const char *text);
    void flushDebugLog(bool force = false);
    size_t getDebugLogSize();

private:
    bool _terminalClientAlreadyConnected = false;
    bool _uartDebugEnabled = false;
    bool _uartDebugLimitReached = false;
    size_t _uartDebugLogBytes = 0;
    size_t _uartDebugLogLimit = 0;
    size_t _uartDebugBufferLen = 0;
    uint32_t _uartDebugDroppedRecords = 0;
    unsigned long _uartDebugLastFlushMs = 0;
    char _uartDebugBuffer[DEBUG_RAM_BUFFER_SIZE];
};

#endif