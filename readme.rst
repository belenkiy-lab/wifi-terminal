====================================================================
Wireless Terminal - configurable ESP8266 Wi-Fi to UART/RS232 bridge
====================================================================

Wireless Terminal is an ESP8266-based transparent serial-console bridge. It provides a browser terminal, a raw TCP terminal, FTP file access, web configuration, live status, Wi-Fi client mode, and a recovery access point.

The current firmware is developed with **Visual Studio Code** and **PlatformIO** and uses **LittleFS** for web assets and runtime configuration.

Main interfaces
---------------

Default service ports::

    Web UI / Status     TCP 80
    Raw TCP terminal    TCP 23
    WebSocket terminal  TCP 81
    FTP                 TCP 21

The browser terminal is based on **xterm.js 5.5.0** with **@xterm/addon-fit 0.10.0**. Terminal traffic between the ESP8266 and browser is carried as binary WebSocket data, so arbitrary UART bytes are not forced through UTF-8 text frames.

Hardware and UART
-----------------

The PlatformIO target is ``d1_mini`` and the firmware is used with Wemos D1 Mini compatible ESP8266 boards, including the tested **TTGO T-OI ESP8266**.

The firmware calls ``Serial.swap()``. UART0 is therefore connected as follows::

    GPIO15  TX
    GPIO13  RX

Default serial settings are **9600 8N1**. Baud rate and framing can be changed from the Settings page.

Custom RS232 shield
~~~~~~~~~~~~~~~~~~~

The project was designed for a MAX3232-based Wemos D1 Mini RS232 shield. The tested custom board uses the following MAX3232 connections::

    GPIO15 -> MAX3232 pin 11 (T1IN)
    MAX3232 pin 14 (T1OUT) -> custom DB9 pin 3 (TX)
    custom DB9 pin 4 (RX) -> MAX3232 pin 13 (R1IN)
    MAX3232 pin 12 (R1OUT) -> GPIO13

**Important: the DB9 pinout on this custom terminal board is NON-STANDARD.**

Viewed from the mating/contact face of the custom DB9 male connector::

    pin 1 = GND
    pin 3 = TX
    pin 4 = RX

A local loopback test is made by connecting custom DB9 pins **3 and 4**.

When connecting a standard PC-style RS232 adapter, wire by signal rather than by identical pin number. The tested ORIENT USS-101 connection was::

    standard adapter pin 3 TX -> custom terminal pin 4 RX
    standard adapter pin 2 RX <- custom terminal pin 3 TX
    standard adapter pin 5 GND -> custom terminal pin 1 GND

Do not connect RS232 voltage levels directly to ESP8266 GPIO. Use a level converter such as MAX3232.

RS232 shield schematic/PCB: https://easyeda.com/clericJ/wemos-d1mini-rs232shield

First start and Recovery Access Point
-------------------------------------

If no Wi-Fi client credentials are stored, the device starts its Recovery Access Point.

Factory defaults::

    SSID       WirelessTerminal
    Password   123456789
    IP         192.168.4.1
    Channel    4

Open ``http://192.168.4.1/`` to configure the device. The Recovery Access Point SSID, password, channel, and IP address can be changed in Settings.

Wi-Fi client mode and automatic fallback
----------------------------------------

When Wi-Fi client credentials are configured, startup logic is automatic:

* the device tries to connect to the saved Wi-Fi network for up to **15 seconds**;
* on success it runs in **Wi-Fi client** mode only and receives its address from the network DHCP server;
* if the initial connection fails, it starts the configured Recovery Access Point;
* if an established Wi-Fi connection is lost, the device allows **15 seconds** for recovery;
* if the connection does not recover, it switches to the Recovery Access Point and stays there for the rest of that boot;
* the next reboot again tries the saved Wi-Fi client credentials.

The **DELETE WI-FI SETTINGS** button removes only the saved Wi-Fi client SSID/password and reboots into the Recovery Access Point. UART, AP, FTP, and other settings are not deleted.

A saved Wi-Fi password is shown as ``********``. Saving the form with that mask leaves the existing password unchanged.

Forced AP recovery - triple RESET
---------------------------------

If valid Wi-Fi settings prevent convenient access to the device, the Recovery Access Point can be forced without deleting the saved credentials.

Press the hardware **RESET button three times with about a 1 second interval**. The interval between two adjacent presses must not exceed **10 seconds**. A short built-in LED flash indicates that the firmware has registered the reset.

After the third accepted reset:

* the device starts directly in the Recovery Access Point for that boot;
* saved Wi-Fi client credentials are preserved;
* the next normal reboot tries the saved Wi-Fi network again.

Software restarts initiated by **SAVE AND REBOOT** are not counted as hardware RESET presses.

Measured boot timing
~~~~~~~~~~~~~~~~~~~~

The following values were observed on the tested TTGO T-OI ESP8266 and are measurements, not guaranteed timing for every board::

    Reset detector ready    72-75 ms
    Recovery AP ready       about 1.3-1.7 s
    Wi-Fi client ready      about 4.7-5.2 s

Settings
--------

The Settings page provides:

* UART baud rate and framing;
* browser-terminal scrollback;
* temporary UART debug logging;
* FTP login and password;
* Wi-Fi client SSID/password;
* Recovery Access Point SSID/password/channel/IP.

Settings are stored in ``/config.json`` in LittleFS.

FTP access
----------

Factory FTP credentials are::

    Login      admin
    Password   admin

The login and password can be changed from Settings and should be changed before using the device on a normal LAN.

A saved FTP password is returned to the browser only as ``********``. Saving that mask leaves the current password unchanged. A newly entered FTP password must contain **8-64 printable characters**; the FTP login accepts **1-32 printable characters**. The short factory password ``admin`` is retained only as the backwards-compatible default until it is changed.

FTP is not encrypted. Use it only on a trusted network.

Web terminal
------------

Open::

    http://DEVICE_IP/terminal.html

The current terminal uses xterm.js and supports:

* binary WebSocket transport;
* automatic terminal fit on browser resize;
* browser-local scrollback, configurable from 0 to 10000 lines;
* selectable Enter line ending: **CR**, **LF**, or **CR+LF**;
* a local **Clear** button that clears the browser display without sending data to UART;
* automatic ``CR`` sent by the firmware when a WebSocket or raw TCP terminal connects.

The raw TCP terminal remains available on port 23, for example with PuTTY in **Raw** mode.

Web terminal and raw TCP/PuTTY can be connected simultaneously. While a raw TCP client is connected, it is treated as primary for terminal capability/autodetection replies. Matching Web-terminal replies are suppressed so devices such as RouterOS do not receive conflicting cursor-position or terminal-capability answers from two emulators.

UART debug
----------

**UART debug until reboot** can be enabled from Settings. It records diagnostic traffic to ``/debug.log`` with sources such as::

    RX UART
    TX WEB
    TX TCP
    TX SYS

Starting a new debug session replaces the previous log. Debug mode itself is not persistent across reboot, but the generated log file remains in LittleFS until it is replaced, deleted by a filesystem rewrite, or otherwise removed.

The logger dynamically limits the file size, keeps a **64 KiB LittleFS reserve**, and caps the debug log at **256 KiB**. Because debug logging writes to flash, it is intended for troubleshooting rather than permanent continuous logging.

Status page
-----------

Open ``http://DEVICE_IP/status.html`` for live status. It reports, among other fields:

* current network mode;
* station SSID, IP and RSSI;
* Recovery AP IP and fallback reason;
* reset-detector and network-ready timing;
* uptime and reset reason;
* UART RX overrun counter;
* LittleFS total/used/free space;
* UART debug state, log size and dropped-record count.

Build and flashing
------------------

The reproducible PlatformIO environment is defined in ``platformio.ini``. Important pinned dependencies include::

    platformio/espressif8266  4.2.1
    ArduinoJson               6.21.5
    SimpleFTPServer           3.0.1
    WebSockets                2.7.3

Build firmware with PlatformIO **Build**, then upload it with **Upload**.

The web filesystem is LittleFS. Upload it with **Upload Filesystem Image**.

The ``scripts/web_assets.py`` pre-build script prepares xterm.js assets when a filesystem target is built. If ``data/xterm.js``, ``data/xterm.css`` or ``data/addon-fit.js`` are missing, the first filesystem build needs Internet access to download the pinned package versions. The ESP8266 itself does **not** need Internet access at runtime; all web assets are served locally.

Important LittleFS notes
~~~~~~~~~~~~~~~~~~~~~~~~

**Upload Filesystem Image rewrites LittleFS.** Runtime-created ``/config.json`` is therefore removed when a new filesystem image is uploaded. This is expected during development. Re-enter UART, Wi-Fi, FTP, and other runtime settings after such an upload.

When upgrading an old SPIFFS-based firmware to this LittleFS version, upload the **new LittleFS-aware firmware first**, then upload the **LittleFS image**. Booting an incompatible old SPIFFS firmware after writing a LittleFS image may cause the filesystem to be treated as invalid and formatted.

On the tested TTGO T-OI unit, normal PlatformIO flashing works automatically with the battery connected; manual GPIO0-to-GND flashing steps are not normally required. Keeping the battery connected was important for reliable flashing on that unit.

Known build notes
-----------------

SimpleFTPServer 3.0.1 currently produces three ESP8266-core deprecation warnings related to ``WiFiServer::available()``. They are third-party compatibility warnings and did not prevent successful builds or the tested FTP operation.

Tested hardware / interoperability
----------------------------------

The current firmware has been tested with:

* TTGO T-OI ESP8266;
* the custom MAX3232 RS232 shield described above;
* ORIENT USS-101 USB-RS232 adapter;
* MikroTik hEX PoE lite (RB750UPr2) RouterOS serial console.

The tested MikroTik session used **115200 8N1**. Bidirectional operation was verified through both the browser terminal and the raw TCP terminal, including simultaneous Web + PuTTY operation.

Screenshots
-----------

The repository still contains the original interface screenshot. It may differ from the current xterm.js/dark web interface.

.. image:: img/interface.png
    :scale: 50%

Wemos D1 Mini RS232 shield
--------------------------

.. image:: img/wemos-rs232-shield.png
    :scale: 50%


============================================================
Wireless Terminal - настраиваемый ESP8266 мост Wi-Fi-UART/RS232
============================================================

Wireless Terminal - прозрачный мост последовательной консоли на ESP8266. Устройство предоставляет терминал в браузере, raw TCP-терминал, FTP-доступ к файлам, web-настройки, страницу состояния, режим Wi-Fi клиента и аварийную точку доступа.

Текущая прошивка собирается в **Visual Studio Code / PlatformIO** и использует **LittleFS** для web-файлов и runtime-конфигурации.

Основные интерфейсы
-------------------

Порты по умолчанию::

    Web UI / Status     TCP 80
    Raw TCP terminal    TCP 23
    WebSocket terminal  TCP 81
    FTP                 TCP 21

Браузерный терминал работает на **xterm.js 5.5.0** с **@xterm/addon-fit 0.10.0**. Данные UART передаются в браузер бинарными WebSocket-кадрами, поэтому произвольные байты последовательного порта не требуется интерпретировать как UTF-8 текст.

Аппаратная часть и UART
-----------------------

В PlatformIO используется target ``d1_mini``. Прошивка подходит для совместимых с Wemos D1 Mini плат ESP8266; основной проверенный вариант - **TTGO T-OI ESP8266**.

Прошивка вызывает ``Serial.swap()``, поэтому UART0 используется так::

    GPIO15  TX
    GPIO13  RX

Настройки последовательного порта по умолчанию - **9600 8N1**. Скорость и формат можно изменить на странице Settings.

Кастомный RS232 shield
~~~~~~~~~~~~~~~~~~~~~~

Устройство проектировалось для RS232-shield на MAX3232. На проверенной плате соединения следующие::

    GPIO15 -> MAX3232 pin 11 (T1IN)
    MAX3232 pin 14 (T1OUT) -> custom DB9 pin 3 (TX)
    custom DB9 pin 4 (RX) -> MAX3232 pin 13 (R1IN)
    MAX3232 pin 12 (R1OUT) -> GPIO13

**Важно: распиновка DB9 на этой кастомной плате НЕСТАНДАРТНАЯ.**

Если смотреть на контактную часть кастомного разъёма DB9 male::

    pin 1 = GND
    pin 3 = TX
    pin 4 = RX

Для локального loopback на этой плате соединяются контакты **3 и 4**.

При подключении стандартного PC-style RS232-адаптера соединять нужно по назначению сигналов, а не одинаковые номера контактов. Проверенное подключение ORIENT USS-101::

    стандартный адаптер pin 3 TX -> terminal pin 4 RX
    стандартный адаптер pin 2 RX <- terminal pin 3 TX
    стандартный адаптер pin 5 GND -> terminal pin 1 GND

Нельзя подавать уровни RS232 непосредственно на GPIO ESP8266. Необходим преобразователь уровней, например MAX3232.

Схема/плата RS232 shield: https://easyeda.com/clericJ/wemos-d1mini-rs232shield

Первый запуск и Recovery Access Point
-------------------------------------

Если настройки Wi-Fi клиента отсутствуют, устройство запускает Recovery Access Point.

Заводские значения::

    SSID       WirelessTerminal
    Password   123456789
    IP         192.168.4.1
    Channel    4

Для настройки откройте ``http://192.168.4.1/``. SSID, пароль, канал и IP Recovery Access Point можно изменить на странице Settings.

Wi-Fi client и автоматический fallback
--------------------------------------

Если сохранены настройки Wi-Fi клиента, алгоритм запуска полностью автоматический:

* устройство пытается подключиться к сохранённой Wi-Fi сети до **15 секунд**;
* при успехе работает только в режиме **Wi-Fi client** и получает IP от DHCP сети;
* если при загрузке подключиться не удалось, запускается настроенная Recovery Access Point;
* если уже установленное Wi-Fi соединение пропало, на восстановление даётся **15 секунд**;
* если связь не восстановилась, устройство переходит в Recovery Access Point до следующей перезагрузки;
* при следующей загрузке сохранённые Wi-Fi credentials снова используются для попытки STA-подключения.

Кнопка **DELETE WI-FI SETTINGS** удаляет только SSID/пароль Wi-Fi клиента и перезагружает устройство в Recovery Access Point. UART, параметры AP, FTP и остальные настройки не удаляются.

Сохранённый Wi-Fi пароль отображается как ``********``. Если сохранить форму с этой маской, текущий пароль не изменится.

Аварийный вход в AP - тройной RESET
-----------------------------------

Если сохранённые Wi-Fi настройки работают, но нужно принудительно получить доступ к Recovery Access Point, удалять credentials не требуется.

Нажмите аппаратную кнопку **RESET три раза с интервалом примерно 1 секунду**. Интервал между двумя соседними нажатиями не должен превышать **10 секунд**. Короткая вспышка встроенного LED показывает, что прошивка зарегистрировала сброс.

После третьего принятого RESET:

* устройство запускается напрямую в Recovery Access Point только на эту загрузку;
* сохранённые Wi-Fi credentials не удаляются;
* следующая обычная перезагрузка снова пытается подключиться к сохранённой Wi-Fi сети.

Программная перезагрузка через **SAVE AND REBOOT** не засчитывается как аппаратное нажатие RESET.

Измеренные времена загрузки
~~~~~~~~~~~~~~~~~~~~~~~~~~~

На проверенном TTGO T-OI ESP8266 получены следующие значения. Это результаты измерений, а не гарантированные параметры для любой платы::

    Reset detector ready    72-75 ms
    Recovery AP ready       примерно 1.3-1.7 s
    Wi-Fi client ready      примерно 4.7-5.2 s

Settings
--------

Страница Settings позволяет настраивать:

* скорость и формат UART;
* scrollback браузерного терминала;
* временный UART debug;
* FTP login/password;
* SSID/password Wi-Fi клиента;
* SSID/password/channel/IP Recovery Access Point.

Настройки хранятся в ``/config.json`` в LittleFS.

FTP
---

Заводские FTP credentials::

    Login      admin
    Password   admin

Логин и пароль можно изменить в Settings. После первого запуска рекомендуется заменить заводские значения перед использованием устройства в обычной локальной сети.

Сохранённый FTP-пароль браузеру выдаётся только как ``********``. Сохранение этой маски оставляет текущий пароль без изменений. Новый FTP-пароль должен содержать **8-64 печатных символа**, логин - **1-32 печатных символа**. Короткий заводской пароль ``admin`` сохраняется только как совместимое значение по умолчанию, пока пользователь его не изменит.

FTP не использует шифрование, поэтому его следует применять только в доверенной сети.

Web Terminal
------------

Открыть терминал::

    http://DEVICE_IP/terminal.html

Текущий Web Terminal использует xterm.js и поддерживает:

* бинарный WebSocket;
* автоматическую подгонку размера терминала при изменении окна браузера;
* локальный для браузера scrollback от 0 до 10000 строк;
* выбор окончания строки для Enter: **CR**, **LF** или **CR+LF**;
* кнопку **Clear**, которая очищает только окно браузера и ничего не отправляет в UART;
* автоматическую отправку ``CR`` прошивкой при подключении WebSocket или raw TCP клиента.

Raw TCP terminal доступен на порту 23, например через PuTTY в режиме **Raw**.

Web Terminal и raw TCP/PuTTY могут работать одновременно. Пока подключён raw TCP клиент, он считается primary для ответов terminal capability/autodetection. Совпадающие автоматические ответы Web Terminal подавляются, чтобы устройство, например RouterOS, не получало одновременно разные ответы о позиции курсора или возможностях терминала от двух эмуляторов.

UART debug
----------

Опция **UART debug until reboot** в Settings включает диагностическую запись в ``/debug.log``. В журнал попадают источники::

    RX UART
    TX WEB
    TX TCP
    TX SYS

Новая debug-сессия заменяет предыдущий лог. Сам режим debug после reboot выключается, но созданный файл остаётся в LittleFS, пока не будет заменён, удалён при перезаписи filesystem или удалён другим способом.

Размер журнала ограничивается динамически: прошивка сохраняет резерв **64 KiB LittleFS**, а максимальный размер ``debug.log`` составляет **256 KiB**. Поскольку debug пишет во flash, режим предназначен для диагностики, а не для постоянного непрерывного логирования.

Status
------

Страница ``http://DEVICE_IP/status.html`` показывает в реальном времени, в частности:

* текущий network mode;
* SSID, IP и RSSI Wi-Fi клиента;
* IP Recovery Access Point и причину fallback;
* время готовности reset detector и сети;
* uptime и причину последней перезагрузки;
* счётчик UART RX overruns;
* занятое/свободное место LittleFS;
* состояние UART debug, размер лога и dropped records.

Сборка и прошивка
-----------------

Воспроизводимая конфигурация PlatformIO находится в ``platformio.ini``. Основные закреплённые версии::

    platformio/espressif8266  4.2.1
    ArduinoJson               6.21.5
    SimpleFTPServer           3.0.1
    WebSockets                2.7.3

Прошивка собирается командой/действием PlatformIO **Build** и загружается через **Upload**.

Web filesystem использует LittleFS и загружается через **Upload Filesystem Image**.

Pre-build script ``scripts/web_assets.py`` подготавливает xterm.js assets при сборке filesystem. Если отсутствуют ``data/xterm.js``, ``data/xterm.css`` или ``data/addon-fit.js``, для первой сборки filesystem нужен Internet, чтобы скачать закреплённые версии пакетов. Самому ESP8266 Internet во время работы не требуется - все web-файлы обслуживаются локально.

Важное про LittleFS
~~~~~~~~~~~~~~~~~~~

**Upload Filesystem Image полностью перезаписывает LittleFS.** Поэтому созданный во время работы ``/config.json`` удаляется при загрузке нового filesystem image. Для разработки это ожидаемое поведение. После такой загрузки нужно заново задать UART, Wi-Fi, FTP и остальные runtime-настройки.

При переходе со старой SPIFFS-версии прошивки сначала загрузите **новую LittleFS-aware firmware**, а уже затем **LittleFS image**. Если после записи LittleFS загрузится несовместимая старая SPIFFS-прошивка, она может посчитать filesystem некорректной и отформатировать её.

На проверенном TTGO T-OI обычный PlatformIO Upload работает автоматически при подключённом аккумуляторе; вручную замыкать GPIO0 на GND для обычной прошивки не требуется. На этом экземпляре подключённый аккумулятор был важен для надёжной прошивки.

Известные замечания сборки
--------------------------

SimpleFTPServer 3.0.1 выдаёт три deprecation warning ESP8266 Core, связанные с ``WiFiServer::available()``. Это предупреждения сторонней библиотеки: они не мешали успешной сборке и проверенной работе FTP.

Проверенное оборудование
------------------------

Текущая версия проверена с:

* TTGO T-OI ESP8266;
* кастомным RS232 shield на MAX3232;
* USB-RS232 адаптером ORIENT USS-101;
* MikroTik hEX PoE lite (RB750UPr2), serial console RouterOS.

Для теста MikroTik использовалось **115200 8N1**. Двунаправленная работа подтверждена и через Web Terminal, и через raw TCP terminal, в том числе при одновременном подключении Web + PuTTY.

Скриншоты
---------

В репозитории сохранён исходный скриншот интерфейса. Он может отличаться от текущего тёмного интерфейса/xterm.js.

.. image:: img/interface.png
    :scale: 50%

Wemos D1 Mini RS232 shield
--------------------------

.. image:: img/wemos-rs232-shield.png
    :scale: 50%
