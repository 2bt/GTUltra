#pragma once

// Blocking modal alerts for fatal/user-visible errors (M6 Phase 1).

void gt_ui_error(const char* message);
void gt_ui_warn(const char* message);
void gt_ui_info(const char* message);

// Yes/No confirm. Returns true if the user chose Yes.
bool gt_ui_confirm(const char* message);
