#include <WiFi.h>
#include <PubSubClient.h> // MQTT library
#include <ArduinoJson.h>
#include "config.h"
HardwareSerial printerSerial(0);  // Use UART2 for printer communication

WiFiClient espClient;
PubSubClient client(espClient);

// Singleton style to guarantee only one instance is used, RAII approach
struct Style { 
  
  enum class AlignMode {
    Left = 0,
    Center = 1,
    Right = 2
  };

  static Style& getInstance(int width = 1, int height = 1, AlignMode alignMode = AlignMode::Left) {
    static Style instance(width, height, alignMode);
    return instance;
  }

  // no copy or move allowed
  Style(const Style&) = delete;
  Style& operator=(const Style&) = delete;
  Style(Style&&) = delete;
  Style& operator=(Style&&) = delete;
  private:
  Style (int width, int height, AlignMode alignMode) {
    setFontSize(width, height);
    align(alignMode);
  }
  ~Style() {
    setFontSize(1, 1); // Reset to normal size
    align(AlignMode::Left); // Reset to left alignment
  }

  // Function to set font size for text
  void setFontSize(int width, int height) {
    printerSerial.write(0x1D);  // ESC
    printerSerial.write(0x21);  // Font size command
    printerSerial.write(((width - 1) << 4) | (height - 1));
  }
  
  // Function to set text alignment
  void align(AlignMode mode) {
    printerSerial.write(0x1B);  // ESC
    printerSerial.write(0x61);  // Alignment command
    printerSerial.write(static_cast<uint8_t>(mode));
  }
  
};

struct SetBold {
  SetBold() {
    printBold(true);  // Enable bold
  }
  ~SetBold() {
    printBold(false); // Disable bold when going out of scope
  }
  
  // no copy or move allowed
  SetBold(const SetBold&) = delete;
  SetBold& operator=(const SetBold&) = delete;
  SetBold(SetBold&&) = delete;
  SetBold& operator=(SetBold&&) = delete;

  private:
  void printBold(bool enable) {
    printerSerial.write(0x1B);
    printerSerial.write(0x45);
    printerSerial.write(enable ? 1 : 0);
  }
};

// Function to parse the JSON payload and print the content
void parseAndPrint(String payload) {
  Serial.println("parseAndPring, Payload:" + payload);
  // Create a JSON document
  StaticJsonDocument<MQTT_PACKET_SIZE + 1> doc;
  
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.println("Failed to parse JSON");
    return;
  }
  //printCenteredText("-----------------------------");

  // Extract data from JSON
  
  String title = doc["title"] | "";
  String text = doc["text"] | "";

  // Optional: Convert icon bitset string to actual bitmap
#ifdef ICON
  String icon = doc["icon"] | "";  // Optional

  if (icon.length() > 0) {
    printIcon(icon);  // Print icon if available
  }
#endif
  
  // Print title (bold, centered, bigger)
  printCenteredBoldTitle(title);

  // Print text (normal format, centered)
  if (text.length() > 0) {
    printCenteredText(text);
  }

  feedLines(1);
  printCenteredText("-----------------------------");

}

#ifdef ICON
// Function to handle printing of the icon (bitmap from bitset)
void printIcon(String iconBitset) {
  int width = 16;  // Define width of icon (you can adjust this based on actual icon size)
  int height = 16; // Define height of icon
  uint8_t iconBitmap[height];

  // Convert bitset (string) to bitmap
  for (int i = 0; i < height; i++) {
    byte row = 0;
    for (int j = 0; j < width; j++) {
      if (iconBitset[i * width + j] == '1') {
        row |= (1 << (7 - j));  // Set corresponding bit
      }
    }
    iconBitmap[i] = row;
  }

  printBitmap(iconBitmap, width, height);  // Call function to print the bitmap
}

// Function to print bitmap (icon) on the thermal printer
void printBitmap(uint8_t* bitmap, int width, int height) {
  for (int i = 0; i < height; i++) {
    printerSerial.write(0x1D);
    printerSerial.write(0x76);
    printerSerial.write(0x30);
    printerSerial.write(0x00);
    printerSerial.write(width % 256); // Width (low byte)
    printerSerial.write(width / 256); // Width (high byte)
    printerSerial.write(height % 256); // Height (low byte)
    printerSerial.write(height / 256); // Height (high byte)

    printerSerial.write(bitmap[i]);
  }
  feedLines(2);  // Feed a few lines after printing the icon
}
#endif // ICON

// Function to print the title (centered, bold, larger font)
void printCenteredBoldTitle(String title) {
  Style& style = Style::getInstance(2, 2, Style::AlignMode::Center); // Large font, centered
  SetBold bold; // RAII style to enable bold for the scope of this block
  printerSerial.println(title);  // Print the title
  feedLines(2);  // Add a line feed after the title
}

// Function to print the text (normal, centered)
void printCenteredText(String text) {
  Style& style = Style::getInstance(1, 1, Style::AlignMode::Center); // Normal font, centered
  printerSerial.println(text);  // Print the text
  feedLines(2);  // Add a few line feeds after the text
}

// Function to feed lines (spacing)
void feedLines(int n) {
  for (auto i = 0; i < n; ++i){
    printerSerial.write(0x0A);
    delay(50);
  }
}

// WiFi connection function
void connectWiFi() {
  Serial.print("Connecting to WiFi...");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println(" connected. IP: " + WiFi.localIP().toString());
}

// Connect to MQTT Broker
void connectMQTT() {
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);
  client.setBufferSize(MQTT_PACKET_SIZE); 

  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");

    if (client.connect("PrinterClient", mqtt_user, mqtt_pass)) {
      Serial.println("Connected to MQTT Broker");
      client.subscribe(mqtt_topic); // Subscribe to the print task topic
    } 
    else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      delay(2000);
    }
  }
}

// MQTT callback to handle incoming print tasks
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.println("[[mqttCallback]]");
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.println("Received task: " + message);
  parseAndPrint(message);  // Parse and print the task
}

// Reset printer (optional, based on your setup)
void resetPrinter() {
  // If you have specific reset commands for your printer, add them here.
}

void setup() {
  Serial.begin(115200);
  printerSerial.begin(9600, SERIAL_8N1, RXD2, TXD2);
  connectWiFi();
  connectMQTT();
  
  resetPrinter();

  Serial.println("Printer is ready and listening for tasks...");
}

void loop() {
  // Repeat it just in case WiFi or MQTT connection drops
  if (WiFi.isConnected() == false)
    connectWiFi();

  if (client.connected() == false)
    connectMQTT();

  client.loop();  // Keep the MQTT connection alive
}