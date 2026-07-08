//
// portable-file-dialogs wrappers for GTUltra (Linux: zenity or kdialog).
//

#include "../extern/portable-file-dialogs/portable-file-dialogs.h"

#include "gfiledialog.h"
#include "gimgui.h"
#include "goattrk2.h"
#include "gpattern.h"
#include "gsong.h"

#include <cstring>
#include <string>
#include <utility>
#include <vector>

#ifndef _WIN32
#include <unistd.h>
#endif

extern "C" {
void win_native_modal_begin(void);
void win_native_modal_end(void);
}

namespace {

static const std::vector<std::string> kSngFilters = {
    "GTUltra Songs", "*.sng",
    "All Files",     "*",
};

static const std::vector<std::string> kInsFilters = {
    "GTUltra Instruments", "*.ins",
    "All Files",           "*",
};

static const std::vector<std::string> kWavFilters = {
    "WAV Audio", "*.wav",
    "All Files", "*",
};

template <typename Fn>
static auto run_modal(Fn&& fn) -> decltype(fn())
{
    win_native_modal_begin();
    auto result = std::forward<Fn>(fn)();
    win_native_modal_end();
    gimgui_reset_input_after_modal();
    return result;
}

static void copy_path(const std::string& src, char* out, size_t out_size)
{
    if (!out || out_size == 0) return;
    snprintf(out, out_size, "%s", src.c_str());
}

static char* path_slash(char* path)
{
    char* slash = strrchr(path, '/');
#ifdef _WIN32
    if (!slash) slash = strrchr(path, '\\');
#endif
    return slash;
}

static void sync_song_paths_from_full_path(const char* full_path)
{
    if (!full_path || !full_path[0]) return;

    snprintf(songfilename, MAX_PATHNAME, "%s", full_path);

    char dirbuf[MAX_PATHNAME];
    snprintf(dirbuf, sizeof dirbuf, "%s", full_path);
    char* slash = path_slash(dirbuf);
    if (slash) {
        *slash = '\0';
        snprintf(songpath, MAX_PATHNAME, "%s", dirbuf);
        chdir(songpath);
    }
}

static void sync_instr_paths_from_full_path(const char* full_path)
{
    if (!full_path || !full_path[0]) return;

    char dirbuf[MAX_PATHNAME];
    snprintf(dirbuf, sizeof dirbuf, "%s", full_path);
    char* slash = path_slash(dirbuf);
    if (slash) {
        snprintf(instrfilename, MAX_FILENAME, "%s", slash + 1);
        *slash = '\0';
        snprintf(instrpath, MAX_PATHNAME, "%s", dirbuf);
        chdir(instrpath);
    } else {
        snprintf(instrfilename, MAX_FILENAME, "%s", full_path);
    }
}

static std::string initial_open_dir()
{
    if (songpath[0]) return songpath;
    return ".";
}

static std::string initial_instr_open_dir()
{
    if (instrpath[0]) return instrpath;
    if (songpath[0]) return songpath;
    return ".";
}

static std::string initial_save_path()
{
    if (loadedsongfilename[0]) return loadedsongfilename;
    if (songfilename[0]) return songfilename;
    if (songpath[0]) return std::string(songpath) + "/untitled.sng";
    return "untitled.sng";
}

static std::string initial_instr_save_path()
{
    if (instrfilename[0]) {
        if (instrpath[0]) return std::string(instrpath) + "/" + instrfilename;
        return instrfilename;
    }
    if (editorInfo.einum && instr[editorInfo.einum].name[0]) {
        std::string name = instr[editorInfo.einum].name;
        if (instrpath[0]) return instrpath + ("/" + name + ".ins");
        return name + ".ins";
    }
    if (instrpath[0]) return std::string(instrpath) + "/instrument.ins";
    return "instrument.ins";
}

static std::string initial_wav_path()
{
    if (wavfilename[0]) return wavfilename;
    if (loadedsongfilename[0]) {
        std::string p = loadedsongfilename;
        const size_t dot = p.rfind('.');
        if (dot != std::string::npos) p.resize(dot);
        return p + ".wav";
    }
    if (songpath[0]) return std::string(songpath) + "/export.wav";
    return "export.wav";
}

} // namespace

namespace gtfile {

bool open_song(char* out_path, size_t out_size, bool merge)
{
    const char* title = merge ? "Merge Song" : "Load Song";
    auto          picked = run_modal([&] {
        return pfd::open_file(title, initial_open_dir(), kSngFilters).result();
    });
    if (picked.empty()) return false;

    sync_song_paths_from_full_path(picked.front().c_str());
    copy_path(picked.front(), out_path, out_size);
    return true;
}

bool save_song(char* out_path, size_t out_size)
{
    auto picked = run_modal([&] {
        return pfd::save_file("Save Song", initial_save_path(), kSngFilters).result();
    });
    if (picked.empty()) return false;

    sync_song_paths_from_full_path(picked.c_str());
    copy_path(picked, out_path, out_size);
    return true;
}

bool open_instrument(char* out_path, size_t out_size)
{
    auto picked = run_modal([&] {
        return pfd::open_file("Load Instrument", initial_instr_open_dir(), kInsFilters).result();
    });
    if (picked.empty()) return false;

    sync_instr_paths_from_full_path(picked.front().c_str());
    copy_path(picked.front(), out_path, out_size);
    return true;
}

bool save_instrument(char* out_path, size_t out_size)
{
    auto picked = run_modal([&] {
        return pfd::save_file("Save Instrument", initial_instr_save_path(), kInsFilters).result();
    });
    if (picked.empty()) return false;

    sync_instr_paths_from_full_path(picked.c_str());
    copy_path(picked, out_path, out_size);
    return true;
}

bool export_wav(char* out_path, size_t out_size)
{
    auto picked = run_modal([&] {
        return pfd::save_file("Export as WAV", initial_wav_path(), kWavFilters).result();
    });
    if (picked.empty()) return false;

    snprintf(wavfilename, MAX_PATHNAME, "%s", picked.c_str());
    copy_path(picked, out_path, out_size);
    return true;
}

} // namespace gtfile
