#pragma once

typedef struct {
    unsigned char* message;
    int            size;
} MIDI_MESSAGE;

// 9999 = MIDI input disabled (legacy gtultra.cfg convention).
enum { MIDI_PORT_DISABLED = 9999 };

int          initMidi(int midiPort);
int          setMidiPort(int midiPort);
int          checkForMidiInput(MIDI_MESSAGE* m, int midiPort);
unsigned int getPortCount();
int          getMidiPortNameInto(int portNumber, char* buf, int bufSize);
