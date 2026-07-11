//
// GOATTRACKER ULTRA MIDI
//

#define GMIDI_C

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <signal.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "RtMidi.hpp"

#include "gmidi.hpp"
#include "gmidiselect.hpp"

bool done;
RtMidiIn *midiin = nullptr;

bool portOpen = false;

static int applyMidiPort(int midiPort)
{
	if (!midiin)
		midiin = new RtMidiIn();

	if (midiPort == MIDI_PORT_DISABLED)
	{
		if (portOpen)
		{
			midiin->closePort();
			portOpen = false;
		}
		return MIDI_PORT_DISABLED;
	}

	unsigned int nPorts = midiin->getPortCount();
	if (nPorts == 0)
	{
		if (portOpen)
		{
			midiin->closePort();
			portOpen = false;
		}
		return MIDI_PORT_DISABLED;
	}

	if (midiPort < 0 || (unsigned int)midiPort >= nPorts)
		midiPort = 0;

	if (portOpen)
		midiin->closePort();

	midiin->openPort((unsigned int)midiPort);
	midiin->ignoreTypes(false, false, false);
	portOpen = true;

	return midiPort;
}

int initMidi(int midiPort)
{
	return applyMidiPort(midiPort);
}

int setMidiPort(int midiPort)
{
	return applyMidiPort(midiPort);
}

char unsigned msg[4096];

int test = 0;
int checkForMidiInput(MIDI_MESSAGE *m,int midiPort)
{
	m->size = 0;
	if (portOpen)
	{
		int nDevices = midiin->getPortCount();
		if (nDevices)
		{

			std::vector<unsigned char> message;
			//int nBytes, i;
			//double stamp;

			midiin->getMessage(&message);
			m->message = (unsigned char*)&msg;
			m->size = message.size();

			for (int i = 0;i < m->size;i++)
			{
				msg[i] = (unsigned char)message[i];
			}
		}
		else
		{
			portOpen = false;
			midiin->closePort();
		}
		return nDevices;	// nDevices;
	}
	else
	{
		if (!midiin)
			midiin = new RtMidiIn();

		unsigned int nPorts = midiin->getPortCount();
		if (nPorts > 0)
		{
			midiin->openPort(midiPort);
			midiin->ignoreTypes(false, false, false);
			portOpen = true;
			return 1;
		}
		return test;
	}
	return 999;

}

unsigned int getPortCount()
{
	if (!midiin)
		midiin = new RtMidiIn();
	return midiin->getPortCount();
}

char* getPortName(int portNumber)
{
	std::string str;
	try {
		if (!midiin)
			midiin = new RtMidiIn();
		str = midiin->getPortName(portNumber);
	}
	catch (RtMidiError &error) {
		return NULL;
	}

	char * writable = new char[str.size() + 1];
	std::copy(str.begin(), str.end(), writable);
	writable[str.size()] = '\0'; // don't forget the terminating 0
	return writable;
}

int getMidiPortNameInto(int portNumber, char* buf, int bufSize)
{
	if (!buf || bufSize <= 0)
		return 0;

	buf[0] = '\0';
	if (!midiin)
		midiin = new RtMidiIn();

	try {
		std::string str = midiin->getPortName(portNumber);
		snprintf(buf, (size_t)bufSize, "%s", str.c_str());
		return 1;
	}
	catch (RtMidiError &error) {
		return 0;
	}
}

