#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#define FASTLED_ESP8266_RAW_PIN_ORDER
#include <FastLED.h>


const char* ssid = "YourWifiName";
const char* password = "YourWifiPassword";
IPAddress apIP(192, 168, 1, 1);

const byte DNS_PORT = 53;

const char* USAGE = "SimpleGlass - WiFi edition\nUsage:\n"
                    "\th   - show this help\n"
                    "\tw   - show white\n"
                    "\tc   - show color\n"
                    "\tr   - show rotation\n"
                    "\tn/m - decrease/increase brightness\n";

struct State {
  enum class Mode {WHITE, COLOR, ROT};
  Mode mode;
  int brightness;
  unsigned char phase;
  unsigned char pos;

  State() :
    mode(Mode::COLOR), brightness(255), phase(0), pos(0)
  {}
};

const int MAX_SRV_CLIENTS = 5;
const int PORT = 12346;
const int LED_COUNT  = 17;
const int PHASE_STEP = 1;
const int PHASE_INC  = 1;
const int DELAY      = 25;
const int BRIGT_STEP = 20;

ESP8266WebServer web_server(80);
WiFiServer server(12346);
WiFiClient serverClients[MAX_SRV_CLIENTS];
DNSServer dnsServer;

CRGB leds[LED_COUNT];
State state;

void handle_root() {
  for ( uint8_t i = 0; i < web_server.args(); i++ ) {
    if (web_server.argName(i) != "cmd")
      continue;
    String arg = web_server.arg(i);
    if (arg == "up") {
      state.brightness += BRIGT_STEP;
      if (state.brightness > 255)
        state.brightness = 255;
    }
    else if (arg == "down") {
      state.brightness -= BRIGT_STEP;
      if (state.brightness < 0)
        state.brightness = 0;
    }
    else if (arg == "color") {
      state.mode = State::Mode::COLOR;
    }
    else if (arg == "white") {
      state.mode = State::Mode::WHITE;
    }
    else if (arg == "rot") {
      state.mode = State::Mode::ROT;
    }
  }
  
  const char* content =
  "<html data-ember-extension=\"1\"><head>\n"
  "        <meta charset=\"UTF-8\">\n"
  "        <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
  "\n"
  "        <style type=\"text/css\">\n"
  "        body{\n"
  "            background: #34495e;\n"
  "            font-family: Helvetica;\n"
  "            -webkit-appearance: none;\n"
  "          color: white;\n"
  "            text-align: center;\n"
  "        }\n"
  "\n"
  "       \n"
  "        input{\n"
  "            width: 100%;\n"
  "            border:0;\n"
  "            padding: 15px;\n"
  "            font-size: 20px;\n"
  "            border-radius: 6px;\n"
  "            background: #1ddab4;\n"
  "            color:white;\n"
  "            -webkit-appearance: none;\n"
  "        }\n"
  "\n"
  "        input:hover{\n"
  "            background: #1fceaa;\n"
  "            color:white;\n"
  "        }\n"
  "        \n"
  "        </style>\n"
  "    </head>\n"
  "<body>\n"
  "    <h1>Lucčina sklenička</h1>\n"
  "    <form action=\"\" method=\"GET\">\n"
  "        <input name=\"cmd\" value=\"up\" type=\"hidden\">\n"
  "        <input value=\"Jas +\" type=\"submit\">\n"
  "    </form>    \n"
  "\n"
  "    <form action=\"\" method=\"GET\">\n"
  "        <input name=\"cmd\" value=\"down\" type=\"hidden\">\n"
  "        <input value=\"Jas -\" type=\"submit\">\n"
  "    </form>  \n"
  "    <form action=\"\" method=\"GET\">\n"
  "        <input name=\"cmd\" value=\"color\" type=\"hidden\">\n"
  "        <input value=\"Svítit barevně\" class=\"color\" type=\"submit\">\n"
  "    </form>  \n"
  "\n"
  "    <form action=\"\" method=\"GET\">\n"
  "        <input name=\"cmd\" value=\"rot\" type=\"hidden\">\n"
  "        <input value=\"Svítit rotačně\" class=\"color\" type=\"submit\">\n"
  "    </form>  \n"
  "\n"
  "    <form action=\"\" method=\"GET\">\n"
  "        <input name=\"cmd\" value=\"white\" class=\"white\" type=\"hidden\">\n"
  "        <input value=\"Svítit bíle\" type=\"submit\">\n"
  "    </form>\n"
  "\n"
  "    \n"
  "\n"
  "    <p>\n"
  "    Lucčina sklenice v1.0<br>\n"
  "    </p><hr>\n"
  "    Hardware design: Jiří \"Connery\" Vácha a Jarek Páral <br>\n"
  "    Software: Honza Mrázek. <br>\n"
  "    Design: Martin Mikšík<br>\n"
  "    Sklenici v Tescu koupil: Jakub Streit.\n"
  "    <p></p>\n"
  "\n"
  "</body>\n"
  "</html>";
  
  web_server.send(200, "text/html", content);
}

void handle_not_found() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += web_server.uri();
  message += "\nMethod: ";
  message += ( web_server.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += web_server.args();
  message += "\n";

  for ( uint8_t i = 0; i < web_server.args(); i++ ) {
    message += " " + web_server.argName ( i ) + ": " + web_server.arg ( i ) + "\n";
  }

  web_server.send ( 404, "text/plain", message );
}

void do_step(State& state) {
  if (state.mode == State::Mode::WHITE) {
    for (int i = 0; i != LED_COUNT; i++) {
      leds[i] = CHSV(0, 0, state.brightness);
    }
  }

  if (state.mode == State::Mode::COLOR) {
    for (int i = 0; i != LED_COUNT; i++) {
      leds[i] = CHSV(state.phase + i * PHASE_INC, 255, state.brightness);
    }
    state.phase += PHASE_INC;
  }

  if (state.mode == State::Mode::ROT) {
    const int length = 10;
    int count = LED_COUNT - 1;
    for(int i = 0; i != LED_COUNT; i++) {
      leds[i] = CRGB(0, 0, 0);
    }

    for (int i = 0; i != length; i++) {
      int brightness1 = state.brightness * i * i / (length * length);
      brightness1 *= brightness1;
      int brightness2 = state.brightness * (2 * i + 1) * (2 * i + 1) / (4 * length * length);
      brightness2 *= brightness2;
      leds[(i + state.pos / 2) % count] = CHSV(state.phase, 255, sqrt((brightness1 + brightness2) / 2));
    }
    leds[LED_COUNT - 1] = CHSV(state.phase, 255, state.brightness);

    state.phase += PHASE_INC;
    state.pos++;
    if (state.pos == 2 * count)
      state.pos = 0;
  }
  FastLED.show();
  delay(DELAY);
}

void handle_input(char in, int client, State& state) {
  Serial.print("Command: "); Serial.print(in);
  Serial.print(", "); Serial.println((int) in);
  switch (in) {
    case 'h':
      Serial.println("Help invoked");
      serverClients[client].print(USAGE);
      break;
    case 'w':
      Serial.println("White invoked");
      state.mode = State::Mode::WHITE;
      break;
    case 'c':
      Serial.println("Color invoked");
      state.mode = State::Mode::COLOR;
      break;
    case 'r':
      Serial.println("Rotation invoked");
      state.mode = State::Mode::ROT;
      break;
    case 'm':
      Serial.println("Brightness invoked");
      state.brightness += BRIGT_STEP;
      if (state.brightness > 255)
        state.brightness = 255;
      break;
    case 'n':
      Serial.println("Brightness invoked");
      state.brightness -= BRIGT_STEP;
      if (state.brightness < 0)
        state.brightness = 0;
      break;
    case 13:
    case ' ':
    case '\n':
      break;
    default:
      Serial.println("Unknown command");
      serverClients[client].print("Uknonw command '");
      serverClients[client].print(in);
      serverClients[client].println("'. Press 'h' for help");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Booting");
  WiFi.hostname("sklenicka");
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(ssid, password);

  if(!dnsServer.start(DNS_PORT, "*", apIP)) {
    Serial.println("DNS failed!");
  }
  
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();

  web_server.on("/", handle_root);
  web_server.onNotFound(handle_not_found);
  web_server.begin();

  server.begin();
  server.setNoDelay(true);

  Serial.print("Ready! Use 'telnet ");
  Serial.print(WiFi.softAPIP());
  Serial.print(" "); Serial.print(PORT);
  Serial.println("' to connect");

  Serial.println("Setting up LEDs");
  FastLED.addLeds<NEOPIXEL, 12>(leds, LED_COUNT);
}

void loop() {
  ArduinoOTA.handle();
  do_step(state);
  web_server.handleClient();
  dnsServer.processNextRequest();

  uint8_t i;
  if (server.hasClient()) {
    Serial.print("New client!");
    for (i = 0; i < MAX_SRV_CLIENTS; i++) {
      if (!serverClients[i] || !serverClients[i].connected()) {
        if (serverClients[i])
          serverClients[i].stop();
        serverClients[i] = server.available();
        serverClients[i].print(USAGE);
        continue;
      }
    }
    //no free spot
    WiFiClient serverClient = server.available();
    serverClient.stop();
  }

  for (i = 0; i < MAX_SRV_CLIENTS; i++) {
    if (serverClients[i] && serverClients[i].connected()) {
      if (serverClients[i].available()) {
        while (serverClients[i].available()) {
          char c = serverClients[i].read();
          handle_input(c, i, state);
          Serial.write(c);
        }
      }
    }
  }
}
