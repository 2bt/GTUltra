//
// portable-file-dialogs wrappers for GTUltra (Linux: zenity or kdialog).
//

#include "../extern/portable-file-dialogs/portable-file-dialogs.h"

#include "gfiledialog.hpp"
#include "gimgui.hpp"
#include "goattrk2.hpp"
#include "gpattern.hpp"
#include "greloc.hpp"
#include "gsong.hpp"
#include "log.hpp"

#include <cstring>
#include <strings.h>
#include <string>
#include <utility>
#include <vector>

#ifndef _WIN32
#include <stdlib.h>
#include <unistd.h>
#endif

namespace {

const std::vector<std::string> kSngFilters = {
    "GTUltra Songs",
    "*.sng",
    "All Files",
    "*"
};

const std::vector<std::string> kInsFilters = {
    "GTUltra Instruments",
    "*.ins",
    "All Files",
    "*"
};

const std::vector<std::string> kWavFilters = {
    "WAV Audio",
    "*.wav",
    "All Files",
    "*"
};

const std::vector<std::string> kRelocFilters = {
    "SID Music", "*.sid", "C64 Program", "*.prg", "Raw Binary", "*.bin", "All Files", "*"
};

template <typename Fn> auto run_modal(Fn&& fn) -> decltype(fn()) {
    win_native_modal_begin();
    auto result = std::forward<Fn>(fn)();
    win_native_modal_end();
    gimgui_reset_input_after_modal();
    return result;
}

void copy_path(const std::string& src, char* out, size_t out_size) {
    if (!out || out_size == 0) return;
    snprintf(out, out_size, "%s", src.c_str());
}

char* path_slash(char* path) {
    char* slash = strrchr(path, '/');
#ifdef _WIN32
    if (!slash) slash = strrchr(path, '\\');
#endif
    return slash;
}

std::string path_basename(const std::string& path) {
    const size_t p = path.find_last_of("/\\");
    return p == std::string::npos ? path : path.substr(p + 1);
}

std::string path_stem(const std::string& path) {
    std::string  base = path_basename(path);
    const size_t dot  = base.rfind('.');
    if (dot != std::string::npos) base.resize(dot);
    return base;
}

std::string path_parent(const std::string& path) {
    const size_t p = path.find_last_of("/\\");
    if (p == std::string::npos) return "";
    return path.substr(0, p);
}

std::string join_dir_file(const std::string& dir, const std::string& file) {
    if (dir.empty() || dir == ".") return file;
    if (dir.back() == '/' || dir.back() == '\\') return dir + file;
    return dir + "/" + file;
}

void log_save_context(const char* kind, const std::string& default_path) {
    LOG_DEBUG("{} default_path={}", kind, default_path);
    LOG_DEBUG("{} loadedsongfilename={}", kind, loadedsongfilename);
    LOG_DEBUG("{} songfilename={} songpath={}", kind, songfilename, songpath);
    LOG_DEBUG("{} packedpath={} packedsongname={} fileformat={}", kind, packedpath, packedsongname, fileformat);
}

std::string run_save_dialog(const char*                     kind,
                            const char*                     title,
                            const std::string&              default_path,
                            const std::vector<std::string>& filters) {
    log_save_context(kind, default_path);

    if (log_debug_enabled()) pfd::settings::verbose(true);

    const std::string picked = run_modal([&] { return pfd::save_file(title, default_path, filters).result(); });

    if (!picked.empty()) LOG_DEBUG("{} picked {}", kind, picked);
    else LOG_DEBUG("{} cancelled", kind);

    return picked;
}

// zenity/kdialog pre-fill works best with a full path to a (possibly new) file.
std::string absolute_path(std::string path) {
    if (path.empty()) return path;

    const std::string file = path_basename(path);
    std::string       dir  = path_parent(path);
    if (dir.empty()) {
        char cwd[MAX_PATHNAME];
        if (getcwd(cwd, sizeof cwd)) dir = cwd;
        else return path;
    }

#ifndef _WIN32
    char* resolved = realpath(dir.c_str(), nullptr);
    if (resolved) {
        path = join_dir_file(resolved, file);
        free(resolved);
        return path;
    }
#endif
    return join_dir_file(dir, file);
}

std::string proposed_save_path(const char* source_path,
                               const char* fallback_dir,
                               const char* default_stem,
                               const char* extension) {
    std::string stem = default_stem;
    std::string dir;
    if (source_path && source_path[0]) {
        stem = path_stem(source_path);
        dir  = path_parent(source_path);
    }
    if (dir.empty() && fallback_dir && fallback_dir[0]) dir = fallback_dir;
    if (dir.empty()) dir = ".";

    return absolute_path(join_dir_file(dir, stem + extension));
}

void sync_song_paths_from_full_path(const char* full_path) {
    if (!full_path || !full_path[0]) return;

    snprintf(songfilename, MAX_PATHNAME, "%s", full_path);

    char dirbuf[MAX_PATHNAME];
    snprintf(dirbuf, sizeof dirbuf, "%s", full_path);
    char* slash = path_slash(dirbuf);
    if (slash) {
        *slash = '\0';
        snprintf(songpath, MAX_PATHNAME, "%s", dirbuf);
        if (chdir(songpath) != 0) {
            /* keep songpath; cwd unchanged on failure */
        }
    }
}

void sync_instr_paths_from_full_path(const char* full_path) {
    if (!full_path || !full_path[0]) return;

    char dirbuf[MAX_PATHNAME];
    snprintf(dirbuf, sizeof dirbuf, "%s", full_path);
    char* slash = path_slash(dirbuf);
    if (slash) {
        snprintf(instrfilename, MAX_FILENAME, "%s", slash + 1);
        *slash = '\0';
        snprintf(instrpath, MAX_PATHNAME, "%s", dirbuf);
        if (chdir(instrpath) != 0) {
            /* keep instrpath; cwd unchanged on failure */
        }
    }
    else {
        snprintf(instrfilename, MAX_FILENAME, "%s", full_path);
    }
}

const char* extension_for_format(int fmt) {
    switch (fmt) {
    case FORMAT_PRG: return ".prg";
    case FORMAT_BIN: return ".bin";
    default: return ".sid";
    }
}

void set_fileformat_from_path(const char* path) {
    if (!path) return;
    if (const char* dot = strrchr(path, '.')) {
        if (strcasecmp(dot, ".prg") == 0) fileformat = FORMAT_PRG;
        else if (strcasecmp(dot, ".bin") == 0) fileformat = FORMAT_BIN;
        else if (strcasecmp(dot, ".sid") == 0) fileformat = FORMAT_SID;
    }
}

void sync_packed_paths_from_full_path(const char* full_path) {
    if (!full_path || !full_path[0]) return;

    char dirbuf[MAX_PATHNAME];
    snprintf(dirbuf, sizeof dirbuf, "%s", full_path);
    char* slash = path_slash(dirbuf);
    if (slash) {
        snprintf(packedsongname, MAX_FILENAME, "%s", slash + 1);
        *slash = '\0';
        snprintf(packedpath, MAX_PATHNAME, "%s", dirbuf);
        if (chdir(packedpath) != 0) {
            /* keep packedpath; cwd unchanged on failure */
        }
    }
    else {
        snprintf(packedsongname, MAX_FILENAME, "%s", full_path);
    }

    set_fileformat_from_path(packedsongname);
}

std::string initial_open_dir() {
    if (songpath[0]) return songpath;
    return ".";
}

std::string initial_instr_open_dir() {
    if (instrpath[0]) return instrpath;
    if (songpath[0]) return songpath;
    return ".";
}

std::string initial_save_path() {
    if (loadedsongfilename[0]) return absolute_path(loadedsongfilename);
    if (songfilename[0]) return absolute_path(songfilename);
    return absolute_path(join_dir_file(songpath[0] ? songpath : ".", "untitled.sng"));
}

std::string initial_instr_save_path() {
    if (instrfilename[0]) {
        if (instrpath[0]) return absolute_path(join_dir_file(instrpath, instrfilename));
        return absolute_path(instrfilename);
    }
    if (editorInfo.einum && instr[editorInfo.einum].name[0]) {
        const std::string name = std::string(instr[editorInfo.einum].name) + ".ins";
        if (instrpath[0]) return absolute_path(join_dir_file(instrpath, name));
        return absolute_path(name);
    }
    return absolute_path(join_dir_file(instrpath[0] ? instrpath : ".", "instrument.ins"));
}

std::string initial_wav_path() {
    if (wavfilename[0]) return absolute_path(wavfilename);

    const char* src = loadedsongfilename[0] ? loadedsongfilename : songfilename;
    if (src && src[0]) {
        std::string  p   = src;
        const size_t dot = p.rfind('.');
        if (dot != std::string::npos) p.resize(dot);
        return absolute_path(p + ".wav");
    }
    return absolute_path(join_dir_file(songpath[0] ? songpath : ".", "export.wav"));
}

std::string initial_packed_save_path() {
    if (packedsongname[0]) {
        const char* dir = packedpath[0] ? packedpath : songpath;
        return absolute_path(join_dir_file(dir ? dir : ".", packedsongname));
    }

    const char* src = loadedsongfilename[0] ? loadedsongfilename : songfilename;
    const char* dir = packedpath[0] ? packedpath : songpath;
    return proposed_save_path(src, dir, "export", extension_for_format(fileformat));
}

} // namespace

namespace gtfile {

bool open_song(char* out_path, size_t out_size, bool merge) {
    const char* title = merge ? "Merge Song" : "Load Song";
    LOG_DEBUG("open_song merge={}", merge);
    auto picked = run_modal([&] { return pfd::open_file(title, initial_open_dir(), kSngFilters).result(); });
    if (picked.empty()) {
        LOG_DEBUG("open_song cancelled");
        return false;
    }

    LOG_DEBUG("open_song picked {}", picked.front());

    sync_song_paths_from_full_path(picked.front().c_str());
    copy_path(picked.front(), out_path, out_size);
    return true;
}

bool save_song(char* out_path, size_t out_size) {
    const std::string picked = run_save_dialog("save_song", "Save Song", initial_save_path(), kSngFilters);
    if (picked.empty()) return false;

    sync_song_paths_from_full_path(picked.c_str());
    copy_path(picked, out_path, out_size);
    return true;
}

bool open_instrument(char* out_path, size_t out_size) {
    auto picked = run_modal(
        [&] { return pfd::open_file("Load Instrument", initial_instr_open_dir(), kInsFilters).result(); });
    if (picked.empty()) return false;

    sync_instr_paths_from_full_path(picked.front().c_str());
    copy_path(picked.front(), out_path, out_size);
    return true;
}

bool save_instrument(char* out_path, size_t out_size) {
    const std::string picked =
        run_save_dialog("save_instrument", "Save Instrument", initial_instr_save_path(), kInsFilters);
    if (picked.empty()) return false;

    sync_instr_paths_from_full_path(picked.c_str());
    copy_path(picked, out_path, out_size);
    return true;
}

bool export_wav(char* out_path, size_t out_size) {
    const std::string picked = run_save_dialog("export_wav", "Export as WAV", initial_wav_path(), kWavFilters);
    if (picked.empty()) return false;

    snprintf(wavfilename, MAX_PATHNAME, "%s", picked.c_str());
    copy_path(picked, out_path, out_size);
    return true;
}

bool save_relocated(char* out_path, size_t out_size) {
    const std::string picked =
        run_save_dialog("save_relocated", "Save Music+Playroutine", initial_packed_save_path(), kRelocFilters);
    if (picked.empty()) return false;

    sync_packed_paths_from_full_path(picked.c_str());
    copy_path(picked, out_path, out_size);
    return true;
}

} // namespace gtfile
