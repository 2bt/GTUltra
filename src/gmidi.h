#ifndef GMIDI_H
#define GMIDI_H

#ifdef __cplusplus
extern "C" {
#endif

	typedef struct {
		unsigned char *message;
		int size;
	}MIDI_MESSAGE;

	// 9999 = MIDI input disabled (legacy gtultra.cfg convention).
	enum { MIDI_PORT_DISABLED = 9999 };

	int initMidi(int midiPort);
	// Close/reopen the input port at runtime (no app restart). Returns the port
	// actually opened, or MIDI_PORT_DISABLED when off or unavailable.
	int setMidiPort(int midiPort);
	int checkForMidiInput(MIDI_MESSAGE *m, int midiPort);
	unsigned int getPortCount();
	char* getPortName(int portNumber);
	// Writes the port name into @p buf; returns 1 on success, 0 on failure.
	int getMidiPortNameInto(int portNumber, char* buf, int bufSize);

#ifdef __cplusplus
}
#endif


#endif

