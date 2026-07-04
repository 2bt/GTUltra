//
// GTUltra Dear ImGui integration layer (milestone M2).
//
// Composites an ImGui layer on top of the legacy editor by hooking into bme's
// existing SDL2 window / renderer. Only built when GTULTRA_IMGUI is defined.
//
#ifndef GIMGUI_H
#define GIMGUI_H

// Initialise ImGui on bme's existing window + renderer and install the bme
// overlay/event hooks. Call once, after the graphics subsystem is up.
void gimgui_init();

// Tear down ImGui. Call once on shutdown.
void gimgui_shutdown();

#endif
