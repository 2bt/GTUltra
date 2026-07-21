#pragma once

#include "gplay.hpp"

#include <string>

namespace ginfo {

// Decode the editor cell under the cursor into a one-line description.
// Pure: reads editor state (editorInfo + song tables), returns the text.
// Handles the data edit modes (Pattern / Instrument / Tables / OrderList);
// returns "" for modes it does not describe.
std::string describe(const GTOBJECT& gt);

} // namespace ginfo
