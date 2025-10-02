#include <BLEMIDI_Transport.h>
#include <hardware/BLEMIDI_ESP32_NimBLE.h>

#include <JC_Button.h> // Ref: https://github.com/JChristensen/JC_Button
#include <jled.h>      // Ref: https://github.com/jandelgado/jled

#include <SSD1306Wire.h> // Ref: https://github.com/ThingPulse/esp8266-oled-ssd1306

// Pin Definitions.
// Safe GPIO pins for switch/button input on ESP32:
// GPIO1-21, GPIO35-48
const int PIN_BLE_CONNECTION_LED = 48; // On-board LED pin
const int MIDI_TX = 17;
const int MIDI_RX = 16;

// ADC Pins
// GPIO1-7, GPIO10-13, GPIO15-18

// I2C Pins
const int PIN_SDA = 8;
const int PIN_SCL = 9;

// MIDI.
const int MIDI_CH = 1;
MIDI_CREATE_INSTANCE(HardwareSerial, Serial2, MIDI);
BLEMIDI_CREATE_INSTANCE("MR. READER", MIDI_BLE);

// OLED
SSD1306Wire display(0x3c, PIN_SDA, PIN_SCL);
unsigned long oledLastUpdatedAt = 0;
const unsigned long oledUpdateInterval = 1000 / 30; // = 30Hz.

void handleBLEMIDIConnected()
{
  digitalWrite(PIN_BLE_CONNECTION_LED, HIGH);
}

void handleBLEMIDIDisconnected()
{
  digitalWrite(PIN_BLE_CONNECTION_LED, LOW);
}

const int MAX_NOTES = 16;

struct NoteSequence {
  int notes[MAX_NOTES];
  int size = 0;
  int index = 0;
};

NoteSequence seqA;
NoteSequence seqB;

// Sample POS CODE
String posCodeA = "4969757161615";
String posCodeB = "4512345678909";
int baseNote = 60; // C4

int quantizeNote(int digit)
{
  int scaleIntervals[10] = {0, 2, 4, 5, 7, 9, 11, 12, 14, 16}; // Major scale intervals
  int note = baseNote + scaleIntervals[digit];
  return note;
}

void posToNotes(const String &posCode, NoteSequence &seq)
{
  String formattedPosCode = posCode.substring(2); // Remove country code
  
  char checkDigit = formattedPosCode.charAt(formattedPosCode.length() - 1); // Get check digit(Last digit)
  seq.size = checkDigit - '0';
  for (int i = 0; i < seq.size; i++)
  {
    int digit = formattedPosCode.charAt(i) - '0';
    seq.notes[i] = quantizeNote(digit);
  }
  seq.index = 0;
}

void stepSequence(NoteSequence &seq) {
  if (seq.size == 0) return;

  MIDI.sendNoteOff(seq.notes[seq.index], 127, MIDI_CH);
  MIDI_BLE.sendNoteOff(seq.notes[seq.index], 127, MIDI_CH);

  seq.index = (seq.index + 1) % seq.size;

  MIDI.sendNoteOn(seq.notes[seq.index], 127, MIDI_CH);
  MIDI_BLE.sendNoteOn(seq.notes[seq.index], 127, MIDI_CH);

  Serial.println("Note[" + String(seq.index) + "] : " + String(seq.notes[seq.index]));
}

void drawSequence(const String &posCode, const NoteSequence &seq, int yOffset) {
  int boxWidth = 9;
  int boxHeight = 12;
  int highlightWidth = boxWidth - 4;
  int highlightHeight = boxHeight - 4;
  int spacing = 11;
  
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);

  display.drawString(64, yOffset, posCode);

  for (int i = 0; i < 10; i++) {
    int x = i * spacing;
    int y = yOffset + 15;

    if (i < seq.size) {
      display.fillRect(x, y, boxWidth, boxHeight);
    } else {
      display.drawRect(x, y, boxWidth, boxHeight);
    }

    if (i == seq.index) {
      display.setColor(BLACK);
      int hx = x + (boxWidth - highlightWidth) / 2;
      int hy = y + (boxHeight - highlightHeight) / 2;
      display.fillRect(hx, hy, highlightWidth, highlightHeight);
      display.setColor(WHITE);
    }
  }

  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.drawString(128, yOffset + 15, String(seq.notes[seq.index]));
}

void updateDisplay()
{
  unsigned long now = millis();
  if( now - oledLastUpdatedAt < oledUpdateInterval){
    return;
  }
  display.clear();
  
  drawSequence(posCodeA, seqA, 0);
  drawSequence(posCodeB, seqB, 32); 

  display.display();
  oledLastUpdatedAt = now;
}

void setup()
{
  Serial.begin(115200);

  Serial2.begin(31250, SERIAL_8N1, MIDI_RX, MIDI_TX);
  MIDI.begin();

  MIDI_BLE.begin();
  BLEMIDI_BLE.setHandleConnected(handleBLEMIDIConnected);  // Ref: https://github.com/lathoub/Arduino-BLE-MIDI/issues/76
  BLEMIDI_BLE.setHandleDisconnected(handleBLEMIDIDisconnected);

  // Test: Setup sequences
  posToNotes(posCodeA, seqA);
  posToNotes(posCodeB, seqB);

  display.init();
  display.flipScreenVertically();
  display.setContrast(255);
}

void loop()
{
  stepSequence(seqA);
  stepSequence(seqB);
  updateDisplay();
  delay(500);
}