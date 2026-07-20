//
// GTUltra path helpers (native dialogs live in gfiledialog).
//

#include "goattrk2.hpp"

#include <cstdio>
#include <cstring>
#include <unistd.h>

void initpaths() {
    memset(loadedsongfilename, 0, sizeof loadedsongfilename);
    memset(songfilename, 0, sizeof songfilename);
    memset(instrfilename, 0, sizeof instrfilename);
    memset(songpath, 0, sizeof songpath);
    memset(instrpath, 0, sizeof instrpath);
    memset(packedpath, 0, sizeof packedpath);
    snprintf(songfilter, MAX_FILENAME, "%s", "*.sng");
    snprintf(wavfilter, MAX_FILENAME, "%s", "*.wav");
    snprintf(instrfilter, MAX_FILENAME, "%s", "*.ins");

    if (!getcwd(songpath, MAX_PATHNAME)) songpath[0] = '\0';
    snprintf(instrpath, MAX_PATHNAME, "%s", songpath);
    snprintf(packedpath, MAX_PATHNAME, "%s", songpath);
}
