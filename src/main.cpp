#include <BLEMIDI_Transport.h>             // Ref: https://github.com/lathoub/Arduino-BLE-MIDI
#include <hardware/BLEMIDI_ESP32_NimBLE.h> // Ref: https://github.com/h2zero/NimBLE-Arduino

#include <JC_Button.h> // Ref: https://github.com/JChristensen/JC_Button
#include <jled.h>      // Ref: https://github.com/jandelgado/jled

// Pin Definitions.
// Safe GPIO pins for switch/button input on ESP32:
// 2, 4, 5, 13, 14, 15, 16, 17, 18, 19, 21, 23, 24, 25, 26, 27, 32, 33
const int PIN_BLE_CONNECTION_LED = 2;
const int MIDI_TX = 17;
const int MIDI_RX = 16;

// MIDI.
const int MIDI_CH = 1;
MIDI_CREATE_INSTANCE(HardwareSerial, Serial2, MIDI)
BLEMIDI_CREATE_INSTANCE("MR. READER", MIDI_BLE);

void handleBLEMIDIConnected()
{
  digitalWrite(PIN_BLE_CONNECTION_LED, HIGH);
}

void handleBLEMIDIDisconnected()
{
  digitalWrite(PIN_BLE_CONNECTION_LED, LOW);
}

const int MAX_NOTES = 16;
int currentIndex = 0;
int currentNotes[MAX_NOTES];
int currentNotesSize = 0;

// Sample POS CODE
String posCode = "4969757161616";
int baseNote = 60; // C4

int quantizeNote(int digit)
{
  int scaleIntervals[10] = {0, 2, 4, 5, 7, 9, 11, 12, 14, 16}; // Major scale intervals
  int note = baseNote + scaleIntervals[digit];
  return note;
}

void posToNote()
{
  String formattedPosCode = posCode.substring(2);                           // Remove country code
  char checkDigit = formattedPosCode.charAt(formattedPosCode.length() - 1); // Get check digit(Last digit)
  currentNotesSize = checkDigit - '0';
  for (int i = 0; i < currentNotesSize; i++)
  {
    currentNotes[i] = quantizeNote(formattedPosCode.charAt(i) - '0'); // Convert char to int and quantize to MIDI note number
  }
}

void playNote()
{
  MIDI.sendNoteOff(currentNotes[currentIndex], 127, MIDI_CH);
  currentIndex = (currentIndex + 1) % currentNotesSize;
  MIDI.sendNoteOn(currentNotes[currentIndex], 127, MIDI_CH);
  Serial.println("MIDI Note[" + String(currentIndex) + "]: " + String(currentNotes[currentIndex]));
}

void setup()
{
  Serial.begin(115200);

  // MIDI
  Serial2.begin(31250, SERIAL_8N1, MIDI_RX, MIDI_TX);
  MIDI.begin();

  MIDI_BLE.begin();
  BLEMIDI_BLE.setHandleConnected(handleBLEMIDIConnected); // Ref: https://github.com/lathoub/Arduino-BLE-MIDI/issues/76
  BLEMIDI_BLE.setHandleDisconnected(handleBLEMIDIDisconnected);
}

void loop()
{
  // put your main code here, to run repeatedly:
  posToNote();
  playNote();
  delay(500);
}