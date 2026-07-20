//
// GOATTRACKER ULTRA MIDI
//

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <signal.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "RtMidi.hpp"

#include "gmidi.hpp"

namespace {

bool      done     = false;
RtMidiIn* midiin   = nullptr;
bool      portOpen = false;

unsigned char msg[4096];
int           test = 0;

int apply_midi_port(int midiPort) {
    if (!midiin) midiin = new RtMidiIn();

    if (midiPort == MIDI_PORT_DISABLED) {
        if (portOpen) {
            midiin->closePort();
            portOpen = false;
        }
        return MIDI_PORT_DISABLED;
    }

    unsigned int nPorts = midiin->getPortCount();
    if (nPorts == 0) {
        if (portOpen) {
            midiin->closePort();
            portOpen = false;
        }
        return MIDI_PORT_DISABLED;
    }

    if (midiPort < 0 || (unsigned int)midiPort >= nPorts) midiPort = 0;

    if (portOpen) midiin->closePort();

    midiin->openPort((unsigned int)midiPort);
    midiin->ignoreTypes(false, false, false);
    portOpen = true;

    return midiPort;
}

} // namespace

int initMidi(int midiPort) { return apply_midi_port(midiPort); }

int setMidiPort(int midiPort) { return apply_midi_port(midiPort); }

int checkForMidiInput(MIDI_MESSAGE* m, int midiPort) {
    m->size = 0;
    if (portOpen) {
        int nDevices = midiin->getPortCount();
        if (nDevices) {
            std::vector<unsigned char> message;
            midiin->getMessage(&message);
            m->message = msg;
            m->size    = (int)message.size();

            for (int i = 0; i < m->size; i++) msg[i] = message[i];
        }
        else {
            portOpen = false;
            midiin->closePort();
        }
        return nDevices;
    }

    if (!midiin) midiin = new RtMidiIn();

    unsigned int nPorts = midiin->getPortCount();
    if (nPorts > 0) {
        midiin->openPort(midiPort);
        midiin->ignoreTypes(false, false, false);
        portOpen = true;
        return 1;
    }
    return test;
}

unsigned int getPortCount() {
    if (!midiin) midiin = new RtMidiIn();
    return midiin->getPortCount();
}

int getMidiPortNameInto(int portNumber, char* buf, int bufSize) {
    if (!buf || bufSize <= 0) return 0;

    buf[0] = '\0';
    if (!midiin) midiin = new RtMidiIn();

    try {
        std::string str = midiin->getPortName(portNumber);
        snprintf(buf, (size_t)bufSize, "%s", str.c_str());
        return 1;
    }
    catch (RtMidiError& error) {
        return 0;
    }
}
