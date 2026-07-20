//
// GTUltra path helpers (native dialogs live in gfiledialog).
//

#define GFILE_C

#include "goattrk2.hpp"

void initpaths(void) {
    memset(loadedsongfilename, 0, sizeof loadedsongfilename);
    memset(songfilename, 0, sizeof songfilename);
    memset(instrfilename, 0, sizeof instrfilename);
    memset(songpath, 0, sizeof songpath);
    memset(instrpath, 0, sizeof instrpath);
    memset(packedpath, 0, sizeof packedpath);
    strcpy(songfilter, "*.sng");
    strcpy(wavfilter, "*.wav");
    strcpy(instrfilter, "*.ins");

    getcwd(songpath, MAX_PATHNAME);
    strcpy(instrpath, songpath);
    strcpy(packedpath, songpath);
}
