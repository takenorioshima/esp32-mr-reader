#include <BLEMIDI_Transport.h>
#include <hardware/BLEMIDI_ESP32_NimBLE.h>

#include <JC_Button.h> // Ref: https://github.com/JChristensen/JC_Button
#include <jled.h>      // Ref: https://github.com/jandelgado/jled

#include <SSD1306Wire.h> // Ref: https://github.com/ThingPulse/esp8266-oled-ssd1306
#include <Wire.h>
#include <Adafruit_MCP4725.h>

// Pin Definitions.
// Safe GPIO pins for switch/button input on ESP32:
// GPIO1-21, GPIO35-48
const int PIN_BLE_CONNECTION_LED = 48; // On-board LED pin
const int PIN_GATE_IN = 21;
const int MIDI_TX = 17;
const int MIDI_RX = 16;

// ADC Pins
// GPIO1-7, GPIO10-13, GPIO15-18

// I2C Pins
const int PIN_SDA = 4;
const int PIN_SCL = 5;

// MIDI.
const int MIDI_CH = 1;
MIDI_CREATE_INSTANCE(HardwareSerial, Serial2, MIDI);
BLEMIDI_CREATE_INSTANCE("MR. READER", MIDI_BLE);

// DAC
Adafruit_MCP4725 dac;
const float VDD = 3.3; // VDD
const int DAC_MAX = 4095; // 12-bit DAC

const int REF_NOTE = 60; // C4 (MIDI note 60)
const float REF_VOLTAGE = 0.0; // 0V for C4

Button gateInButton(PIN_GATE_IN);

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

// Sequencer
bool isNoteOn = false;
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

float midiToVolts(int midiNote)
{
  float octavesFromRef = (midiNote - REF_NOTE) / 12.0;
  return REF_VOLTAGE + octavesFromRef * 1.0;  // 1V/oct
}

uint16_t voltsToDac(float volts) {
  if (volts < 0.0) volts = 0.0;
  if (volts > VDD) volts = VDD;
  return round((volts / VDD) * DAC_MAX);
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

  gateInButton.read();
  if (gateInButton.isPressed()) {
    if (!isNoteOn) {
      isNoteOn = true;
      MIDI.sendNoteOn(seq.notes[seq.index], 127, MIDI_CH);
      MIDI_BLE.sendNoteOn(seq.notes[seq.index], 127, MIDI_CH);
      Serial.println("Gate In - Note On: " + String(seq.notes[seq.index]));
      // Control voltage output
      float volts = midiToVolts(seq.notes[seq.index]);
      uint16_t value = voltsToDac(volts);
      dac.setVoltage(value, false);
      Serial.printf("note=%d volts=%.3fV dac=%d\n", seq.notes[seq.index], volts, value);
    }
    return; // Hold the note while gate is high
  } else {
    if (isNoteOn) {
      isNoteOn = false;
      MIDI.sendNoteOff(seq.notes[seq.index], 127, MIDI_CH);
      MIDI_BLE.sendNoteOff(seq.notes[seq.index], 127, MIDI_CH);
      dac.setVoltage(0, false);
      Serial.println("Gate Out - Note Off: " + String(seq.notes[seq.index]));

      // Move to the next note
      seq.index = (seq.index + 1) % seq.size;
    }
  }
}

void drawSequence(const String &posCode, const NoteSequence &seq, int yOffset) {
  int boxWidth = 8;
  int boxHeight = 12;
  int highlightWidth = boxWidth - 4;
  int highlightHeight = boxHeight - 4;
  int spacing = 9;
  
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

  // DAC
  Wire.begin(PIN_SDA, PIN_SCL);
  dac.begin(0x60);
  gateInButton.begin();

  display.init();
  display.flipScreenVertically();
  display.setContrast(255);
}

void loop()
{
  stepSequence(seqA);
  // stepSequence(seqB); // Comment out to play only seqA
  updateDisplay();
}