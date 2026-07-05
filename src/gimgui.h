#pragma once

// Initialise ImGui on bme's existing window + renderer and install the bme
// overlay/event hooks. Call once, after the graphics subsystem is up.
void gimgui_init();

// Tear down ImGui. Call once on shutdown.
void gimgui_shutdown();
