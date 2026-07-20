#include "gplatform.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

struct DataHeader {
    Uint32 offset;
    Sint32 length;
    char   name[13];
};

struct DataHandle {
    DataHeader* currentheader;
    int         filepos;
    int         open;
};

bool           io_usedatafile = false;
DataHeader*    fileheaders    = nullptr;
unsigned       files          = 0;
DataHandle     handle[MAX_HANDLES];
FILE*          fileptr[MAX_HANDLES] = { nullptr };
unsigned char* datafileptr          = nullptr;
unsigned char* datafilestart        = nullptr;

void     linkedseek(unsigned pos);
void     linkedread(void* buffer, int length);
unsigned linkedreadle32();

void linkedseek(unsigned pos) { datafileptr = &datafilestart[pos]; }

void linkedread(void* buffer, int length) {
    unsigned char* dest = (unsigned char*)buffer;
    while (length--) *dest++ = *datafileptr++;
}

unsigned linkedreadle32() {
    unsigned char bytes[4];
    linkedread(&bytes, 4);
    return ((unsigned)bytes[3] << 24) | ((unsigned)bytes[2] << 16) | ((unsigned)bytes[1] << 8) | bytes[0];
}

} // namespace

bool io_openlinkeddatafile(const unsigned char* ptr) {
    datafilestart = const_cast<unsigned char*>(ptr);
    linkedseek(0);

    char ident[4];
    linkedread(ident, 4);
    if (memcmp(ident, "DAT!", 4)) return false;

    files       = linkedreadle32();
    fileheaders = (DataHeader*)malloc(files * sizeof(DataHeader));
    if (!fileheaders) return false;

    for (unsigned index = 0; index < files; index++) {
        fileheaders[index].offset = linkedreadle32();
        fileheaders[index].length = (Sint32)linkedreadle32();
        linkedread(&fileheaders[index].name, 13);
    }

    for (int index = 0; index < MAX_HANDLES; index++) handle[index].open = 0;
    io_usedatafile = true;
    return true;
}

int io_open(const char* name) {
    if (!name) return -1;

    if (!io_usedatafile) {
        int index;
        for (index = 0; index < MAX_HANDLES; index++) {
            if (!fileptr[index]) break;
        }
        if (index == MAX_HANDLES) return -1;

        FILE* file = fopen(name, "rb");
        if (!file) return -1;
        fileptr[index] = file;
        return index;
    }

    char namecopy[13];
    int  namelength = (int)strlen(name);
    if (namelength > 12) namelength = 12;
    memcpy(namecopy, name, (size_t)namelength + 1);
    for (int i = 0; namecopy[i]; i++) namecopy[i] = (char)toupper((unsigned char)namecopy[i]);

    for (int index = 0; index < MAX_HANDLES; index++) {
        if (!handle[index].open) {
            int count                   = (int)files;
            handle[index].currentheader = fileheaders;

            while (count) {
                if (!strcmp(namecopy, handle[index].currentheader->name)) {
                    handle[index].open    = 1;
                    handle[index].filepos = 0;
                    return index;
                }
                count--;
                handle[index].currentheader++;
            }
            return -1;
        }
    }
    return -1;
}

int io_lseek(int index, int offset, int whence) {
    if (!io_usedatafile) {
        fseek(fileptr[index], offset, whence);
        return (int)ftell(fileptr[index]);
    }

    if ((index < 0) || (index >= MAX_HANDLES) || !handle[index].open) return -1;

    int newpos;
    switch (whence) {
    default:
    case SEEK_SET: newpos = offset; break;
    case SEEK_CUR: newpos = offset + handle[index].filepos; break;
    case SEEK_END: newpos = offset + handle[index].currentheader->length; break;
    }
    if (newpos < 0) newpos = 0;
    if (newpos > handle[index].currentheader->length) newpos = handle[index].currentheader->length;
    handle[index].filepos = newpos;
    return newpos;
}

int io_read(int index, void* buffer, int length) {
    if (!io_usedatafile) return (int)fread(buffer, 1, (size_t)length, fileptr[index]);

    if ((index < 0) || (index >= MAX_HANDLES) || !handle[index].open) return -1;
    if (length + handle[index].filepos > handle[index].currentheader->length)
        length = handle[index].currentheader->length - handle[index].filepos;

    linkedseek(handle[index].currentheader->offset + (unsigned)handle[index].filepos);
    linkedread(buffer, length);
    handle[index].filepos += length;
    return length;
}

void io_close(int index) {
    if (!io_usedatafile) {
        fclose(fileptr[index]);
        fileptr[index] = nullptr;
        return;
    }

    if ((index < 0) || (index >= MAX_HANDLES)) return;
    handle[index].open = 0;
}

unsigned io_read8(int index) {
    unsigned char byte;
    io_read(index, &byte, 1);
    return byte;
}
