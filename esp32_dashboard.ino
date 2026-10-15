/*
  ESP32-S3 N16R8 dual-OLED PC dashboard
  OLED 1 (Wire,  SDA 8,  SCL 9 ): CPU / GPU / RAM / SSD / battery
  OLED 2 (Wire1, SDA 17, SCL 18): Spotify now playing (scrolling title, progress bar)

  Libraries (Library Manager): Adafruit SSD1306, Adafruit GFX, ArduinoJson (v7)
  Board: "ESP32S3 Dev Module", PSRAM: "OPI PSRAM", Flash: 16MB.
  If you use the native "USB" port, set "USB CDC On Boot: Enabled".
*/
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>

#define SDA1 8
#define SCL1 9
#define SDA2 17
#define SCL2 18
#define OLED_ADDR 0x3C   // try 0x3D if a screen stays blank

Adafruit_SSD1306 oled1(128, 64, &Wire, -1);
Adafruit_SSD1306 oled2(128, 64, &Wire1, -1);

struct Stats { int cpuT, cpuU, gpuT, gpuU, ramU, ssdT, bat; } st = {-1, -1, -1, -1, -1, -1, -1};
struct Song  { String title, artist; bool valid = false, playing = false; int pos = 0, dur = 0; } song;

unsigned long lastRx = 0, lastDraw1 = 0, lastDraw2 = 0;
String line;

// ------------------------------------------------------------- helpers ---
int jint(JsonVariant v) { return v.isNull() ? -1 : v.as<int>(); }

void printVal(Adafruit_SSD1306 &d, int v, const char *suffix) {
  if (v < 0) d.print("--");
  else { d.print(v); d.print(suffix); }
}

void drawRow(int y, const char *label, int temp, int use) {
  oled1.setCursor(0, y);   oled1.print(label);
  oled1.setCursor(28, y);  printVal(oled1, temp, "C");
  oled1.setCursor(72, y);  printVal(oled1, use, "%");
  oled1.drawRect(0, y + 10, 128, 5, SSD1306_WHITE);
  if (use > 0) oled1.fillRect(0, y + 10, constrain(use, 0, 100) * 128 / 100, 5, SSD1306_WHITE);
}

void scrollText(Adafruit_SSD1306 &d, const String &s, int y, int size) {
  d.setTextSize(size);
  int16_t x1, y1; uint16_t w, h;
  d.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  d.setCursor(0, y);
  if (w <= 128) { d.print(s); return; }
  int gap = 40, cycle = w + gap;
  int off = (millis() / 35) % cycle;
  d.setCursor(-off, y);               d.print(s);
  d.setCursor(-off + cycle, y);       d.print(s);
}

String mmss(int sec) {
  char b[8]; snprintf(b, sizeof(b), "%d:%02d", sec / 60, sec % 60); return String(b);
}

// -------------------------------------------------------------- screens ---
void drawStats() {
  oled1.clearDisplay();
  oled1.setTextSize(1);
  oled1.setTextColor(SSD1306_WHITE);
  if (millis() - lastRx > 4000) {
    oled1.setCursor(14, 28); oled1.print("Waiting for PC...");
  } else {
    drawRow(0,  "CPU", st.cpuT, st.cpuU);
    drawRow(16, "GPU", st.gpuT, st.gpuU);
    drawRow(32, "RAM", -1,      st.ramU);
    oled1.setCursor(0, 54);  oled1.print("SSD "); printVal(oled1, st.ssdT, "C");
    oled1.setCursor(72, 54); oled1.print("BAT "); printVal(oled1, st.bat, "%");
  }
  oled1.display();
}

void drawSong() {
  oled2.clearDisplay();
  oled2.setTextColor(SSD1306_WHITE);
  oled2.setTextSize(1);
  if (millis() - lastRx > 4000 || !song.valid || song.title.length() == 0) {
    oled2.setCursor(20, 28); oled2.print("Nothing playing");
    oled2.display();
    return;
  }
  // header: play / pause icon + label
  if (song.playing) oled2.fillTriangle(0, 0, 0, 8, 7, 4, SSD1306_WHITE);
  else { oled2.fillRect(0, 0, 3, 8, SSD1306_WHITE); oled2.fillRect(5, 0, 3, 8, SSD1306_WHITE); }
  oled2.setCursor(14, 0); oled2.print("Spotify");

  scrollText(oled2, song.title, 14, 2);
  scrollText(oled2, song.artist, 36, 1);

  int pos = song.pos;
  if (song.playing) pos += (millis() - lastRx) / 1000;
  if (song.dur > 0) {
    pos = min(pos, song.dur);
    oled2.drawRect(0, 49, 128, 5, SSD1306_WHITE);
    oled2.fillRect(0, 49, (long)pos * 128 / song.dur, 5, SSD1306_WHITE);
    oled2.setCursor(0, 56);   oled2.print(mmss(pos));
    String d = mmss(song.dur);
    oled2.setCursor(128 - 6 * d.length(), 56); oled2.print(d);
  }
  oled2.display();
}

// --------------------------------------------------------------- serial ---
void handleLine(const String &l) {
  JsonDocument doc;
  if (deserializeJson(doc, l)) return;
  JsonObject s = doc["s"];
  st.cpuT = jint(s["cpu_t"]); st.cpuU = jint(s["cpu_u"]);
  st.gpuT = jint(s["gpu_t"]); st.gpuU = jint(s["gpu_u"]);
  st.ramU = jint(s["ram_u"]); st.ssdT = jint(s["ssd_t"]); st.bat = jint(s["bat"]);
  JsonObject m = doc["m"];
  song.valid = !m.isNull();
  if (song.valid) {
    song.title   = m["title"].as<String>();
    song.artist  = m["artist"].as<String>();
    song.playing = m["playing"] | false;
    song.pos     = m["pos"] | 0;
    song.dur     = m["dur"] | 0;
  }
  lastRx = millis();
  drawStats();
}

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA1, SCL1);   Wire.setClock(400000);
  Wire1.begin(SDA2, SCL2);  Wire1.setClock(400000);
  if (!oled1.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) Serial.println("OLED1 not found");
  if (!oled2.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) Serial.println("OLED2 not found");
  drawStats();
  drawSong();
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') { handleLine(line); line = ""; }
    else if (c != '\r' && line.length() < 700) line += c;
  }
  unsigned long now = millis();
  if (now - lastDraw1 > 1000) { lastDraw1 = now; drawStats(); }   // refreshes "waiting" state
  if (now - lastDraw2 > 50)   { lastDraw2 = now; drawSong(); }    // ~20 fps for smooth scroll
}
