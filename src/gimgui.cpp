//
// GTUltra Dear ImGui integration layer (milestone M2).
//
// Strategy (see docs/UI_MIGRATION_PLAN.md): rather than open a second window,
// reuse bme's existing SDL2 window + renderer. bme already renders the whole
// legacy editor into a texture and blits it every frame; we hook in just before
// its present to draw ImGui on top, and forward SDL events to ImGui. This lets
// the old UI keep working while native ImGui panels are built on top of it.
//
#include "gimgui.h"

#include <SDL.h>

#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_sdlrenderer2.h"

#include "guimodel.h" // SDL-free bridge to the legacy model

// bme globals/hooks we bind to. Declared here (with C linkage) instead of
// including bme's headers, so we pull the *system* SDL2 headers that the ImGui
// backends use rather than bme's bundled copy. Both are SDL2, so the opaque
// SDL_Window*/SDL_Renderer* types and the underlying library match.
extern "C" {
    extern SDL_Window   *win_window;
    extern SDL_Renderer *gfx_renderer;
    extern void (*bme_overlay_render_hook)(void);
    extern void (*bme_event_hook)(void *sdl_event);
    extern int (*bme_input_capture_hook)(void);
}

static bool g_imgui_ready = false;
static bool g_show_demo = false; // toggleable ImGui reference/demo window

// First native panel: the four SID tables (wave/pulse/filter/speed), read-only.
// Reads live model state via the guimodel bridge, so it mirrors the legacy
// tables as the user navigates them. A proof-of-concept for the model-binding +
// per-cell rendering that the editable panels will build on.
static void gimgui_draw_tables(void)
{
    if (!ImGui::Begin("SID Tables"))
    {
        ImGui::End();
        return;
    }

    const int cursorTable = gtui::table_cursor_table();
    const int cursorPos = gtui::table_cursor_pos();
    const int rows = gtui::table_len();
    const ImU32 cursorCol = IM_COL32(255, 232, 0, 255);

    for (int t = 0; t < gtui::table_count(); t++)
    {
        if (t)
            ImGui::SameLine();

        ImGui::BeginChild(gtui::table_name(t), ImVec2(92, 320), true);
        ImGui::TextUnformatted(gtui::table_name(t));
        ImGui::Separator();

        // 255 rows * 4 tables per frame - clip to the visible ones.
        ImGuiListClipper clipper;
        clipper.Begin(rows);
        while (clipper.Step())
        {
            for (int r = clipper.DisplayStart; r < clipper.DisplayEnd; r++)
            {
                const bool atCursor = (t == cursorTable && r == cursorPos);
                if (atCursor)
                    ImGui::PushStyleColor(ImGuiCol_Text, cursorCol);
                ImGui::Text("%02X:%02X %02X", r + 1,
                            gtui::table_left(t, r), gtui::table_right(t, r));
                if (atCursor)
                    ImGui::PopStyleColor();
            }
        }
        ImGui::EndChild();
    }

    ImGui::End();
}

// Called by bme (via bme_overlay_render_hook) between its RenderCopy and its
// RenderPresent, i.e. on top of the freshly-drawn legacy frame.
extern "C" void gimgui_overlay_render(void)
{
    if (!g_imgui_ready)
        return;

    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem("ImGui Demo", nullptr, &g_show_demo);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    gimgui_draw_tables();
    if (g_show_demo)
        ImGui::ShowDemoWindow(&g_show_demo);

    ImGui::Render();

    // The legacy frame is letterboxed via an explicit destination rect (no
    // renderer logical size), so ImGui already draws across the full window in
    // output pixels here.
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), gfx_renderer);
}

// Called by bme (via bme_event_hook) for every polled SDL event.
extern "C" void gimgui_event_process(void *sdl_event)
{
    if (!g_imgui_ready)
        return;
    ImGui_ImplSDL2_ProcessEvent(static_cast<const SDL_Event *>(sdl_event));
}

// Called by bme (via bme_input_capture_hook): tells the legacy editor to ignore
// input that ImGui is consuming. bit0 = mouse, bit1 = keyboard.
extern "C" int gimgui_input_capture(void)
{
    if (!g_imgui_ready)
        return 0;
    ImGuiIO &io = ImGui::GetIO();
    int flags = 0;
    if (io.WantCaptureMouse)    flags |= 1;
    if (io.WantCaptureKeyboard) flags |= 2;
    return flags;
}

void gimgui_init()
{
    if (g_imgui_ready)
        return;
    // gfx_renderer can be null under headless/unsupported video drivers.
    if (!win_window || !gfx_renderer)
        return;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForSDLRenderer(win_window, gfx_renderer);
    ImGui_ImplSDLRenderer2_Init(gfx_renderer);

    g_imgui_ready = true;

    bme_overlay_render_hook = gimgui_overlay_render;
    bme_event_hook = gimgui_event_process;
    bme_input_capture_hook = gimgui_input_capture;
}

void gimgui_shutdown()
{
    if (!g_imgui_ready)
        return;

    bme_overlay_render_hook = nullptr;
    bme_event_hook = nullptr;
    bme_input_capture_hook = nullptr;

    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    g_imgui_ready = false;
}
