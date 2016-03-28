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
#define CAP_SAMPLES 100        // How many samples to take per key per check (30)
#define INTERVAL 100          // Execute main loop this often (ms)
#define AIR_PIN 14            // Pin for air flow sensor (A0)
#define AIR_MULTIPLIER 0.5f   // Multiply air flow value (to map to midi velocity)
#define AIR_OFFSET -256       // Air flow value offset (to map to midi velocity)
#define AIR_TRESHOLD 5        // How much the midi velocity has to change to send a update
#define KEYS 8                // How many keys the instrument has
#define SEND_PIN 2            // Pin for sending a capacitive pulse (D2)
#define RECV_PIN0 3           // Receive pins (D3 to D10)
#define RECV_PIN1 4
#define RECV_PIN2 5
#define RECV_PIN3 6
#define RECV_PIN4 7
#define RECV_PIN5 8
#define RECV_PIN6 9
#define RECV_PIN7 10
#define KEY_TRESHOLD 10       // Values over this are considered a touching of sensor
#define DEBUG 0               // Define when wanting to debug
//#define MIDI 1                // Define when wanting to use as midi device
#define EMULATE_AIR 1         // Always full velocity (no air sensor, but test the keys)
#define NOTES 12              // How many key combinations (notes) are available
#define CHANNEL 3             // MIDI channel to use (1 to 16)

const unsigned char note_keys[NOTES] =
  {
  0b11111111, // Low C
  0b11111110, // Low D
  0b11111100, // Low E
  0b11111010, // F
  0b11110110, // F#
  0b11110000, // G
  0b11100000, // A
  0b11010000, // Bb (H or A#)
  0b11000000, // B
  0b10100000, // C
  0b00100000, // D
  0b01111100  // E
  };
const unsigned char note_values[NOTES] =
  {60, 62, 64, 77, 78, 79, 81, 82, 83, 72, 74, 76};

// Array of the holes/keys/sensors which simulate holes on a real flute
const unsigned char recv_pins[KEYS] = {RECV_PIN0, RECV_PIN1, RECV_PIN2, RECV_PIN3, RECV_PIN4, RECV_PIN5, RECV_PIN6, RECV_PIN7};
CapacitiveSensor *keys[KEYS];
bool key_touched[KEYS];

// Some global variables
float old_air;

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

  // Set up sensors and initial values
  for (unsigned char i=0; i<KEYS; i++)
  {
    key_touched[i] = false;
    keys[i] = new CapacitiveSensor(SEND_PIN, recv_pins[i]);
    //keys[i]->set_CS_AutocaL_Millis(0xFFFFFFFF); // autocalibrate off
  }
  old_air = analogRead(AIR_PIN);
}

/**
 * Arduino main loop
 */
void loop()                    
{
  // Get values of all capacitive keys
  String msg = "Keys:";
  bool keys_changed = false;
  for (unsigned char i=0; i<KEYS; i++)
  {
    long key_value = 0;
    key_value = keys[i]->capacitiveSensor(CAP_SAMPLES);
    bool touched = (key_value > KEY_TRESHOLD);
    if (touched != key_touched[i])
    {
      keys_changed = true;
      key_touched[i] = touched;
    }
    msg = msg + " ";
    msg = msg + key_value;
  }
  log(msg);

  // Calculate value of air flowing, mapping it to MIDI velocity range
  // (0-127)
  float air_value = (float)analogRead(AIR_PIN);
  air_value = air_value * AIR_MULTIPLIER + AIR_OFFSET;
  if (air_value < 0) air_value = 0;
  if (air_value > 127) air_value = 127;
  
  msg = "Air: ";
  msg = msg + air_value;
  log(msg);

  // Determine if the air value changed significantly enough to generate
  // a new MIDI message
  bool air_changed = false;
  if (abs(air_value-old_air)>AIR_TRESHOLD)
  {
    air_changed = true;
  }
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
      String msg = "Key combination";
      msg = msg + keys_value;
      msg = msg + " matched to note ";
      msg = msg + note_value;
      log(msg);
      break;
    }
  }

  // Generate a note update on MIDI channel 1 if air value changed enough
  // or the keys have changed and are detected to be a note.
#ifdef MIDI
  if ((air_changed || keys_changed))
  {
    log("Sending note");
    
    unsigned char velocity = (unsigned char)air_value;
    
    // If we have no air sensor, emulate full velocity
#ifdef EMULATE_AIR
      velocity = 127;
#endif

    // No velocity if no note
    if (note_value == 0) velocity = 0;

    if (velocity == 0)
    {
      MIDI_PORT.write(0b10000000 + (CHANNEL-1));
      MIDI_PORT.write(note_value);
      MIDI_PORT.write(velocity);
    }
    else
    {
      MIDI_PORT.write(0b10010000 + (CHANNEL-1));
      MIDI_PORT.write(note_value);
      MIDI_PORT.write(velocity);
    }
  }
#endif

  // Sleep to not generate debug or MIDI messages too often
  delay(INTERVAL);
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
