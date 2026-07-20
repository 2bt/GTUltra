#pragma once
//
// Native file dialogs for the ImGui UI (portable-file-dialogs).
//

#include <cstddef>

namespace gtfile {

// Song load (F10). merge=true matches legacy Shift/Ctrl+load (mergesong).
bool open_song(char* out_path, size_t out_size, bool merge);

// Song save (F11 / Save-as when quick-save has no path).
bool save_song(char* out_path, size_t out_size);

// Instrument load/save when the instrument or tables panel has focus (F10/F11).
bool open_instrument(char* out_path, size_t out_size);
bool save_instrument(char* out_path, size_t out_size);

// WAV export path (SaveWav action).
bool export_wav(char* out_path, size_t out_size);

// Pack/relocate export (F9): SID, PRG, or BIN. Sets packedsongname/fileformat globals.
bool save_relocated(char* out_path, size_t out_size);

} // namespace gtfile
