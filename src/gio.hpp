#pragma once

// Linked goatdata blob + filesystem fallback for players / icon.

int      io_open(const char* name);
int      io_lseek(int handle, int bytes, int whence);
int      io_read(int handle, void* buffer, int size);
void     io_close(int handle);
bool     io_openlinkeddatafile(const unsigned char* ptr);
unsigned io_read8(int handle);
