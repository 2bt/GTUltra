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
    ImGui::SetNextWindowPos(ImVec2(8, 330), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(560, 236), ImGuiCond_FirstUseEver);
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

        ImGui::BeginChild(gtui::table_name(t), ImVec2(132, 320), true);
        ImGui::TextUnformatted(gtui::table_name(t));
        ImGui::Separator();

        const float cellW = ImGui::CalcTextSize("0000").x;
        const ImGuiInputTextFlags hexFlags =
            ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase |
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll;

        // 255 rows * 4 tables per frame - clip to the visible ones. Each value
        // is a hex field; committing (Enter) writes back through gtui::table_set
        // so the edit goes through the legacy undo system.
        ImGuiListClipper clipper;
        clipper.Begin(rows);
        while (clipper.Step())
        {
            for (int r = clipper.DisplayStart; r < clipper.DisplayEnd; r++)
            {
                ImGui::PushID(r);
                const bool atCursor = (t == cursorTable && r == cursorPos);
                if (atCursor)
                    ImGui::PushStyleColor(ImGuiCol_Text, cursorCol);
                ImGui::Text("%02X:", r + 1);
                if (atCursor)
                    ImGui::PopStyleColor();

                unsigned char l = (unsigned char)gtui::table_left(t, r);
                unsigned char rt = (unsigned char)gtui::table_right(t, r);

                ImGui::SameLine();
                ImGui::SetNextItemWidth(cellW);
                if (ImGui::InputScalar("##l", ImGuiDataType_U8, &l, NULL, NULL, "%02X", hexFlags))
                    gtui::table_set(t, r, 0, l);

                ImGui::SameLine();
                ImGui::SetNextItemWidth(cellW);
                if (ImGui::InputScalar("##r", ImGuiDataType_U8, &rt, NULL, NULL, "%02X", hexFlags))
                    gtui::table_set(t, r, 1, rt);

                ImGui::PopID();
            }
        }
        ImGui::EndChild();
    }

    ImGui::End();
}

// Read-only pattern grid (M4 first cut): a custom ImDrawList grid, following
// Furnace's approach - fixed monospace metrics, virtualized to the visible rows,
// per-field coloring. Reads live model state via guimodel; editing comes later.
static void gimgui_draw_pattern(void)
{
    ImGui::SetNextWindowPos(ImVec2(8, 24), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(430, 300), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Pattern"))
    {
        ImGui::End();
        return;
    }

    // Colors (hardcoded for now; the theme system is M7).
    const ImU32 cBeat1    = IM_COL32(255, 255, 255, 16);
    const ImU32 cBeat2    = IM_COL32(255, 255, 255, 32);
    const ImU32 cCursorRow= IM_COL32(64, 110, 190, 90);
    const ImU32 cCursorChn= IM_COL32(255, 232, 0, 40);
    const ImU32 cRowNum   = IM_COL32(120, 140, 160, 255);
    const ImU32 cRowNumHi = IM_COL32(210, 210, 130, 255);
    const ImU32 cNote     = IM_COL32(224, 230, 238, 255);
    const ImU32 cInstr    = IM_COL32(120, 205, 120, 255);
    const ImU32 cCmd      = IM_COL32(235, 180, 90, 255);
    const ImU32 cDots     = IM_COL32(85, 95, 108, 255);
    const ImU32 cHeader   = IM_COL32(180, 200, 220, 255);
    const ImU32 cEnd      = IM_COL32(150, 160, 175, 255);

    const int   chans = gtui::pattern_channels();
    const int   rows  = gtui::pattern_rows();
    const int   step  = gtui::pattern_step() > 0 ? gtui::pattern_step() : 4;
    const int   curRow = gtui::pattern_cursor_row();
    const int   curChn = gtui::pattern_cursor_chn();

    const float charW = ImGui::CalcTextSize("0").x;
    const float lineH = ImGui::GetTextLineHeight();
    const float rowNumW = charW * 4.0f;   // "999 "
    const float chanW   = charW * 9.0f;   // "NOTEIIcDD" + gutter

    // Column headers (fixed, above the scrolling body).
    {
        ImDrawList *hdl = ImGui::GetWindowDrawList();
        ImVec2 hp = ImGui::GetCursorScreenPos();
        char hbuf[32];
        for (int c = 0; c < chans; c++)
        {
            snprintf(hbuf, sizeof hbuf, "CH%X %02X", gtui::pattern_actual_channel(c),
                     gtui::pattern_number(c));
            hdl->AddText(ImVec2(hp.x + rowNumW + c * chanW, hp.y), cHeader, hbuf);
        }
        ImGui::Dummy(ImVec2(rowNumW + chans * chanW, lineH));
        ImGui::Separator();
    }

    ImGui::BeginChild("patgrid", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    {
        ImDrawList *dl = ImGui::GetWindowDrawList();
        const ImVec2 origin = ImGui::GetCursorScreenPos(); // accounts for scroll
        const float totalW = rowNumW + chans * chanW;
        const float totalH = rows * lineH;
        ImGui::Dummy(ImVec2(totalW, totalH)); // reserve scroll region

        // Keep the edit cursor in view (mirrors the legacy always-centered view).
        static int lastCur = -1;
        if (curRow != lastCur)
        {
            lastCur = curRow;
            float target = curRow * lineH - ImGui::GetWindowHeight() * 0.5f;
            if (target < 0) target = 0;
            ImGui::SetScrollY(target);
        }

        const float scrollY = ImGui::GetScrollY();
        const float winH = ImGui::GetWindowHeight();
        int firstRow = (int)(scrollY / lineH);
        int lastRow = (int)((scrollY + winH) / lineH) + 1;
        if (firstRow < 0) firstRow = 0;
        if (lastRow > rows) lastRow = rows;

        char buf[16];
        for (int r = firstRow; r < lastRow; r++)
        {
            const float y = origin.y + r * lineH;

            // Row background: beat highlight + cursor row.
            const bool firstOfBeat = (r % step) == 0;
            const bool secondBeat = (r % (step * 2)) < step;
            ImU32 bg = 0;
            if (secondBeat) bg = firstOfBeat ? cBeat2 : cBeat1;
            else if (firstOfBeat) bg = cBeat1;
            if (bg)
                dl->AddRectFilled(ImVec2(origin.x, y), ImVec2(origin.x + totalW, y + lineH), bg);
            if (r == curRow)
                dl->AddRectFilled(ImVec2(origin.x, y), ImVec2(origin.x + totalW, y + lineH), cCursorRow);

            // Row number.
            snprintf(buf, sizeof buf, "%3d", r);
            dl->AddText(ImVec2(origin.x, y), firstOfBeat ? cRowNumHi : cRowNum, buf);

            // Channels.
            for (int c = 0; c < chans; c++)
            {
                const float cx = origin.x + rowNumW + c * chanW;
                if (c == curChn)
                    dl->AddRectFilled(ImVec2(cx - charW * 0.25f, y),
                                      ImVec2(cx + charW * 8.5f, y + lineH), cCursorChn);

                gtui::PatCell cell = gtui::pattern_cell(c, r);
                if (!cell.valid)
                    continue;
                if (cell.end)
                {
                    dl->AddText(ImVec2(cx, y), cEnd, "===");
                    continue;
                }

                // Note (3 chars); dim an empty (REST) note like the legacy view.
                const bool emptyNote = (cell.note[0] == '.');
                dl->AddText(ImVec2(cx, y), emptyNote ? cDots : cNote, cell.note);
                // Instrument (2 chars) or dots.
                if (cell.instr) { snprintf(buf, sizeof buf, "%02X", cell.instr); dl->AddText(ImVec2(cx + charW * 3, y), cInstr, buf); }
                else            { dl->AddText(ImVec2(cx + charW * 3, y), cDots, ".."); }
                // Command nibble + data byte (3 chars) or dots.
                if (cell.cmd)   { snprintf(buf, sizeof buf, "%01X%02X", cell.cmd, cell.data); dl->AddText(ImVec2(cx + charW * 5, y), cCmd, buf); }
                else            { dl->AddText(ImVec2(cx + charW * 5, y), cDots, "..."); }
            }
        }
    }
    ImGui::EndChild();
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

    gimgui_draw_pattern();
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
