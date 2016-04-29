// Electric Flute MIDI controller
// Copyright (C) 2016 Jussi Salin <salinjus@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <CapacitiveSensor.h>

// Configuration
#define DEBUG_PORT Serial
#define MIDI_PORT Serial
#define DEBUG_BPS 115200      // Baud rate in debug mode
#define MIDI_BPS 115200       // Baud rate in MIDI mode (31250 if real MIDI)
#define CAP_SAMPLES 30        // How many samples to take per key per check (30)
#define INTERVAL 1            // Execute main loop this often (ms)
#define AIR_PIN 14            // Pin for air flow sensor (A0)
#define AIR_TRESHOLD 4        // How much the air value has to exceed minimum to be considered
#define AIR_MULTIPLIER 4      // To make reasonable blowing to result in max (127) velocity
//#define ALLOW_INHALING 1      // Should inhaling also generate a note
#define KEYS 8                // How many keys the instrument has
#define SEND_PIN0 2           // Pin for sending a capacitive pulse (D2)
#define SEND_PIN1 11          // Another pin for sending a capacitive pulse (D11)
#define SEND_PIN2 12          // Third pin for sending a capacitive pulse (D12)
#define RECV_PIN0 3           // Receive pins (D3 to D10)
#define RECV_PIN1 4
#define RECV_PIN2 5
#define RECV_PIN3 6
#define RECV_PIN4 7
#define RECV_PIN5 8
#define RECV_PIN6 9
#define RECV_PIN7 10
#define KEY_TRESHOLD 20       // Values over this are considered a touching of sensor
//#define DEBUG 1               // Define when wanting to debug
#define MIDI 1                // Define when wanting to use as midi device
#define NOTES 22              // How many key combinations (notes) are available
#define CHANNEL 1             // MIDI channel to use (1 to 16)
#define ALWAYS_MAX_VELOCITY 1 // Always max velocity. Good with some synths like Yamaha XG.
#define KEEP_PLAYING 1        // Keep playing a note even if combination changes to unknown.
                              // In this case stopping air flow stops the note.

const unsigned char note_keys[NOTES] =
  {
  0b11111111, // Low C (octave 5)
  0b11111110, // Low D (octave 5)
  0b11111100, // Low E (octave 5)
  0b11111011, // Low F (octave 5)
  0b11111000, // Low F also (octave 5)
  0b11110110, // Low F# (octave 5)
  0b11110000, // Low G (octave 5)
  0b11100000, // Low A (octave 5)
  0b11011000, // Low Bb (H or A#) (octave 5)
  0b11000000, // Low B (octave 5)
  0b10100000, // C (octave 6)
  0b00100000, // D (octave 6)
  0b01111100, // E (octave 6)
  0b11110001, // Low G (octave 5) (lowest hole covered as support)
  0b11100001, // Low A (octave 5) (lowest hole covered as support)
  0b11010001, // Low Bb (H or A#) (octave 5) (lowest hole covered as support)
  0b11000001, // Low B (octave 5) (lowest hole covered as support)
  0b10100001, // C (octave 6) (lowest hole covered as support)
  0b00100001, // D (octave 6) (lowest hole covered as support)
  0b01111101, // E (octave 6) (lowest hole covered as support)
  0b01100000, // C#/Db (octave 6)
  0b11101110, // Low G# (octave 5)
  };
const unsigned char note_values[NOTES] =
  {60, 62, 64, 65, 65, 66, 67, 69, 70, 71, 72, 74, 76, 67, 69, 70, 71, 72, 74, 76, 73, 68};
unsigned char last_note = 0; // Last note that is playing, so it can be stopped
int last_velocity = 0;

// Array of the holes/keys/sensors which simulate holes on a real flute
// Change based on what your wiring happens to be!
//const unsigned char recv_pins[KEYS] = {RECV_PIN0, RECV_PIN1, RECV_PIN2, RECV_PIN3, RECV_PIN4, RECV_PIN5, RECV_PIN6, RECV_PIN7};
//const unsigned char send_pins[KEYS] = {SEND_PIN0, SEND_PIN0, SEND_PIN0, SEND_PIN0, SEND_PIN1, SEND_PIN1, SEND_PIN2, SEND_PIN2};
const unsigned char recv_pins[KEYS] = {RECV_PIN3, RECV_PIN2, RECV_PIN1, RECV_PIN0, RECV_PIN7, RECV_PIN5, RECV_PIN6, RECV_PIN4};
const unsigned char send_pins[KEYS] = {SEND_PIN0, SEND_PIN0, SEND_PIN0, SEND_PIN0, SEND_PIN2, SEND_PIN1, SEND_PIN2, SEND_PIN1};
long key_cal[KEYS]; // Calibration values
CapacitiveSensor *keys[KEYS];
bool key_touched[KEYS];

// Some global variables
int old_air = 0;              // To see if air value is changing
int min_air = 0;              // Air calibration
String global_msg = "";       // For logging
unsigned long last_ms= 0;     // For main loop to run at exact intervals
unsigned char old_note_value; // Old note value, for use with KEEP_PLAYING

/**
 * Arduino setup function to initialize everything
 */
void setup()                    
{
  // Begin serial debug
#ifdef DEBUG
  DEBUG_PORT.begin(DEBUG_BPS);
#endif

  // Begin serial MIDI
#ifdef MIDI
  MIDI_PORT.begin(MIDI_BPS);
#endif

  // Set up sensors and calibration values
  for (unsigned char i=0; i<KEYS; i++)
  {
    key_touched[i] = false;
    keys[i] = new CapacitiveSensor(send_pins[i], recv_pins[i]);
    keys[i]->set_CS_AutocaL_Millis(0xFFFFFFFF); // Autocalibrate off
    key_cal[i] = 0;
  }

  for (int j=0; j<10; j++)
  {
    for (unsigned char i=0; i<KEYS; i++)
    {
      long value = keys[i]->capacitiveSensor(CAP_SAMPLES);
      if (value > key_cal[i]) key_cal[i] = value;
    }
    delay(50);
  }
  log("Keys calibrated");

  calibrate_air();
}

/**
 * Calibrate air sensor, finding base level
 */
void calibrate_air()
{
  min_air = 0;
  for (int i=0; i<10; i++)
  {
    int value = analogRead(AIR_PIN);
    if (value > min_air) min_air = value;
    delay(50);
  }
  String msg = "Air sensor calibrated with base level value of ";
  msg = msg + min_air;
  log(msg);
}

/**
 * Arduino main loop
 */
void loop()                    
{
  String sensor_msg = "Keys:";
  String finger_msg = "[";
  String other_msg = "";
  global_msg = "";
  
  // Get values of all capacitive keys
  bool keys_changed = false;
  for (unsigned char i=0; i<KEYS; i++)
  {
    long key_value = keys[i]->capacitiveSensor(CAP_SAMPLES) - key_cal[i];
    bool touched = (key_value > KEY_TRESHOLD);
    if (touched != key_touched[i])
    {
      keys_changed = true;
      key_touched[i] = touched;
    }
    sensor_msg += " ";
    sensor_msg += key_value;
    if (touched) finger_msg += "O"; else finger_msg += ".";
  }
  finger_msg += "] ";

  int air_value = analogRead(AIR_PIN) - min_air; // get calibrated air value
#ifdef ALLOW_INHALING
  air_value = abs(air_value); // allow inhaling by turning it positive value
#endif
  air_value -= AIR_TRESHOLD; // eliminate noise by a treshold level
  if (air_value < 0) air_value = 0;
  air_value = air_value * AIR_MULTIPLIER; // amplify the value to midi velocity levels
  if (air_value > 127) air_value = 127; // don't go beyond the level
#ifdef ALWAYS_MAX_VELOCITY
  if (air_value > 0) air_value = 127;
#endif
  
  sensor_msg += " Air: ";
  sensor_msg += air_value;
  
  // Determine if the air value changed significantly enough to generate
  // a new MIDI message
  bool air_changed = false;
  if (abs(air_value - old_air) > 0) air_changed = true;
  old_air = air_value;

  // Determine note based on key combination
  unsigned char keys_value = 0;
  for (unsigned char i=0; i<KEYS; i++)
  {
    if (key_touched[i]) keys_value += 1 << i;
  }
  unsigned char note_value = 0;
  for (unsigned char i=0; i<NOTES; i++)
  {
    if (note_keys[i] == keys_value)
    {
      note_value = note_values[i];
      other_msg += " Keys ";
      other_msg += keys_value;
      other_msg += " matched note ";
      other_msg += note_value;
      break;
    }
  }
#ifdef KEEP_PLAYING
  // Unknown combination (note_value 0) does not register as changed.
  // Instead keep using the old note_value.
  if (note_value == 0) 
  {
    keys_changed = false;
    note_value = old_note_value;
  }
  old_note_value = note_value;
#endif

  // Generate a note update on MIDI if air value changed enough
  // or the keys have changed and are detected to be a note.
  if (air_changed || keys_changed)
  {
    play_note(note_value, air_value);
  }

  String msg = finger_msg;
  msg += sensor_msg;
  msg += other_msg;
  msg += global_msg;
  log(msg);

  // Sleep to not generate debug or MIDI messages too often
  unsigned long ms = 0;
  do
  {
    ms = millis();
  }
  while (ms - last_ms < INTERVAL);
  last_ms = ms;
}

/**
 * Stop note that is currently playing (if any)
 */
void stop_note()
{
  if (last_note <= 0) return;
  
  global_msg += " STOP ";
  global_msg += last_note;
  
#ifdef MIDI
  MIDI_PORT.write(0b10000000 | (CHANNEL - 1));
  MIDI_PORT.write(last_note);
  MIDI_PORT.write(0);
#endif
  last_note = 0;
}

/**
 * Play a note, stopping old note first (if playing)
 */
void play_note(unsigned char note_value, int velocity)
{
  if (velocity <= 0)
  {
    stop_note();
    return;
  }
  if (note_value != last_note) stop_note();
  
  if (note_value == 0) return;

  if ((note_value == last_note) && (velocity != last_velocity))
  {
    global_msg += " CHANGE ";
    global_msg += note_value;
    global_msg += "/";
    global_msg += velocity;
  
#ifdef MIDI
    MIDI_PORT.write(0b10100000 | (CHANNEL - 1));
    MIDI_PORT.write(note_value);
    MIDI_PORT.write(velocity);
#endif

    last_velocity = velocity;
  }
  else
  if ((note_value != last_note) && (velocity > 0))
  {
    global_msg += " START ";
    global_msg += note_value;
    global_msg += "/";
    global_msg += velocity;

#ifdef MIDI
    MIDI_PORT.write(0b10010000 | (CHANNEL - 1));
    MIDI_PORT.write(note_value);
    MIDI_PORT.write(velocity);
#endif
  
    last_note = note_value;
    last_velocity = velocity;
  }
}

/**
 * Logging function
 */
void log(String msg)
{
#ifdef DEBUG
  DEBUG_PORT.println(msg);
#endif
}
