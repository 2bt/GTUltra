//
// GTUltra path helpers (native dialogs live in gfiledialog).
//

#include "goattrk2.hpp"

#include <cstring>
#include <unistd.h>

void initpaths() {
    loadedsongfilename[0] = '\0';
    songfilename[0]       = '\0';
    instrfilename[0]      = '\0';

    if (!getcwd(songpath, MAX_PATHNAME)) songpath[0] = '\0';
    std::strcpy(instrpath, songpath);
    std::strcpy(packedpath, songpath);
}
