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
static bool g_show_demo = false;    // toggleable ImGui reference/demo window
static bool g_reset_layout = false; // one-shot: snap panels back to defaults

// Reusable scaffold for a scrolling, virtualized monospace grid body (used by
// the pattern editor and each SID table). Handles the child window, content
// reservation, row virtualization, no-lag cursor-follow, and click hit-testing.
// The caller supplies the cell/row content and click handling; all colours and
// per-cell layout live there.
//   rows      : total row count
//   rowW      : content width (horizontal scroll reservation)
//   lineH     : row height
//   followRow : row to keep centred when it changes (<0 = don't auto-follow)
//   drawRow(ImDrawList* dl, int row, float x, float y)   x,y = row's top-left
//   onClick(int row, float localX)                       localX = px from row start
template <class DrawRow, class OnClick>
static void gimgui_grid_body(const char *id, int rows, float rowW, float lineH,
                             int followRow, DrawRow drawRow, OnClick onClick)
{
    ImGui::BeginChild(id, ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    ImDrawList *dl = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float totalH = rows * lineH;
    ImGui::Dummy(ImVec2(rowW, totalH)); // reserve scroll region

    const float winH = ImGui::GetWindowHeight();
    const float curScroll = ImGui::GetScrollY();
    const float contentTop = origin.y + curScroll; // scroll-independent anchor

    // Follow the cursor row when it moves. SetScrollY only takes effect next
    // frame, so we also draw at the target scroll *this* frame (no one-frame
    // jump). Follow state is per-grid (child storage), so grids don't yank each
    // other and manual scrolling sticks until the cursor moves again.
    float drawScroll = curScroll;
    if (followRow >= 0)
    {
        ImGuiStorage *st = ImGui::GetStateStorage();
        const ImGuiID key = ImGui::GetID("##gridfollow");
        if (st->GetInt(key, -1) != followRow)
        {
            st->SetInt(key, followRow);
            float maxScroll = totalH - winH;
            if (maxScroll < 0) maxScroll = 0;
            drawScroll = followRow * lineH - winH * 0.5f;
            if (drawScroll < 0) drawScroll = 0;
            if (drawScroll > maxScroll) drawScroll = maxScroll;
            ImGui::SetScrollY(drawScroll);
        }
    }
    const float drawTop = contentTop - drawScroll;

    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        const ImVec2 m = ImGui::GetIO().MousePos;
        int r = (int)((m.y - drawTop) / lineH);
        if (r >= 0 && r < rows)
            onClick(r, m.x - origin.x);
    }

    int firstRow = (int)(drawScroll / lineH);
    int lastRow = (int)((drawScroll + winH) / lineH) + 1;
    if (firstRow < 0) firstRow = 0;
    if (lastRow > rows) lastRow = rows;
    for (int r = firstRow; r < lastRow; r++)
        drawRow(dl, r, origin.x, drawTop + r * lineH);

    ImGui::EndChild();
}

// Draw one SID table as an independently-scrolling column: a fixed header over
// a virtualized scrolling body (fills the available height). Only the active
// table auto-follows its cursor; the others keep their own scroll, matching the
// legacy's per-table independent scrolling.
static void gimgui_draw_one_table(int t, float colW, float charW, float lineH)
{
    const ImU32 cCursorRow = IM_COL32(255, 255, 255, 20);
    const ImU32 cSelect    = IM_COL32(48, 96, 200, 110);
    const ImU32 cCursorFill= IM_COL32(235, 225, 120, 70);
    const ImU32 cCursorEdge= IM_COL32(235, 225, 120, 230);
    const ImU32 cIdx       = IM_COL32(120, 140, 160, 255);
    const ImU32 cVal       = IM_COL32(224, 230, 238, 255);
    static const int colOff[4] = { 3, 4, 6, 7 }; // cursor char within "II:LL RR"

    const int tlen   = gtui::table_len();
    const int curTab = gtui::table_cursor_table();
    const int curPos = gtui::table_cursor_pos();
    const int curCol = gtui::table_cursor_col();
    const int markTab = gtui::table_mark_table();
    int markLo = gtui::table_mark_start(), markHi = gtui::table_mark_end();
    if (markLo > markHi) { int tmp = markLo; markLo = markHi; markHi = tmp; }
    const bool active = (curTab == t);

    ImGui::BeginChild("col", ImVec2(colW, 0), true);

    // TODO: clicking a table header should toggle an alternative "detailed"
    // interpreted view (a GTUltra feature; not for the speed table). For now the
    // header is plain text.
    ImGui::TextUnformatted(gtui::table_name(t));
    ImGui::Separator();

    gimgui_grid_body(
        "body", tlen, charW * 8, lineH, active ? curPos : -1,
        [&](ImDrawList *dl, int r, float x, float y) {
            char buf[16];
            if (markTab == t && r >= markLo && r <= markHi)
                dl->AddRectFilled(ImVec2(x + charW * 3, y), ImVec2(x + charW * 8, y + lineH), cSelect);
            if (active && curPos == r)
            {
                dl->AddRectFilled(ImVec2(x, y), ImVec2(x + charW * 8, y + lineH), cCursorRow);
                float cs = x + colOff[curCol < 0 ? 0 : (curCol > 3 ? 3 : curCol)] * charW;
                dl->AddRectFilled(ImVec2(cs, y), ImVec2(cs + charW, y + lineH), cCursorFill);
                dl->AddRect(ImVec2(cs, y), ImVec2(cs + charW, y + lineH), cCursorEdge);
            }
            snprintf(buf, sizeof buf, "%02X:", r + 1);
            dl->AddText(ImVec2(x, y), cIdx, buf);
            snprintf(buf, sizeof buf, "%02X %02X", gtui::table_left(t, r), gtui::table_right(t, r));
            dl->AddText(ImVec2(x + charW * 3, y), cVal, buf);
        },
        [&](int r, float localX) {
            int off = (int)(localX / charW);
            int col = (off <= 3) ? 0 : (off == 4) ? 1 : (off <= 6) ? 2 : 3;
            gtui::table_set_cursor(t, r, col);
        });

    ImGui::EndChild();
}

// The four SID tables (wave/pulse/filter/speed): four independently-scrolling
// columns (matching the legacy per-table scroll), each a custom grid like the
// pattern editor. Keyboard editing flows through the legacy table editor;
// clicking a cell places the cursor.
static void gimgui_draw_tables(void)
{
    const ImGuiCond posCond = g_reset_layout ? ImGuiCond_Always : ImGuiCond_FirstUseEver;
    ImGui::SetNextWindowPos(ImVec2(8, 330), posCond);
    ImGui::SetNextWindowSize(ImVec2(380, 236), posCond);
    if (!ImGui::Begin("SID Tables"))
    {
        ImGui::End();
        return;
    }

    const float charW = ImGui::CalcTextSize("0").x;
    const float lineH = ImGui::GetTextLineHeight();
    const float colW = charW * 8.0f + ImGui::GetStyle().ScrollbarSize +
                       ImGui::GetStyle().WindowPadding.x * 2.0f + 2.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 2));
    for (int t = 0; t < gtui::table_count(); t++)
    {
        if (t)
            ImGui::SameLine();
        ImGui::PushID(t);
        gimgui_draw_one_table(t, colW, charW, lineH);
        ImGui::PopID();
    }
    ImGui::PopStyleVar();

    ImGui::End();
}

// Read-only pattern grid (M4 first cut): a custom ImDrawList grid, following
// Furnace's approach - fixed monospace metrics, virtualized to the visible rows,
// per-field coloring. Reads live model state via guimodel; editing comes later.
static void gimgui_draw_pattern(void)
{
    const ImGuiCond posCond = g_reset_layout ? ImGuiCond_Always : ImGuiCond_FirstUseEver;
    ImGui::SetNextWindowPos(ImVec2(8, 24), posCond);
    ImGui::SetNextWindowSize(ImVec2(430, 300), posCond);
    if (!ImGui::Begin("Pattern"))
    {
        ImGui::End();
        return;
    }

    // Colors (hardcoded for now; the theme system is M7).
    const ImU32 cBeat1     = IM_COL32(255, 255, 255, 16);
    const ImU32 cBeat2     = IM_COL32(255, 255, 255, 32);
    const ImU32 cCursorRow = IM_COL32(255, 255, 255, 20);
    const ImU32 cSelect    = IM_COL32(48, 96, 200, 110);   // Shift+Up/Down mark
    const ImU32 cCursorFill= IM_COL32(235, 225, 120, 70);  // cursor cell
    const ImU32 cCursorEdge= IM_COL32(235, 225, 120, 230);
    const ImU32 cRowNum    = IM_COL32(120, 140, 160, 255);
    const ImU32 cRowNumHi  = IM_COL32(210, 210, 130, 255);
    const ImU32 cNote      = IM_COL32(224, 230, 238, 255);
    const ImU32 cInstr     = IM_COL32(120, 205, 120, 255);
    const ImU32 cCmd       = IM_COL32(235, 180, 90, 255);
    const ImU32 cDots      = IM_COL32(85, 95, 108, 255);
    const ImU32 cHeader    = IM_COL32(180, 200, 220, 255);
    const ImU32 cEnd       = IM_COL32(150, 160, 175, 255);

    const int   chans = gtui::pattern_channels();
    const int   rows  = gtui::pattern_rows();
    const int   step  = gtui::pattern_step() > 0 ? gtui::pattern_step() : 4;
    const int   curRow = gtui::pattern_cursor_row();
    const int   curChn = gtui::pattern_cursor_chn();
    const int   curCol = gtui::pattern_cursor_col();

    // Active selection (Shift+Up/Down): actual channel + inclusive row range.
    const int   markChn = gtui::pattern_mark_channel();
    int markLo = gtui::pattern_mark_start();
    int markHi = gtui::pattern_mark_end();
    if (markLo > markHi) { int t = markLo; markLo = markHi; markHi = t; }

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

    const float totalW = rowNumW + chans * chanW;

    gimgui_grid_body(
        "patgrid", rows, totalW, lineH, curRow,
        [&](ImDrawList *dl, int r, float x, float y) {
            char buf[16];

            // Row background: beat highlight + cursor row.
            const bool firstOfBeat = (r % step) == 0;
            const bool secondBeat = (r % (step * 2)) < step;
            ImU32 bg = 0;
            if (secondBeat) bg = firstOfBeat ? cBeat2 : cBeat1;
            else if (firstOfBeat) bg = cBeat1;
            if (bg)
                dl->AddRectFilled(ImVec2(x, y), ImVec2(x + totalW, y + lineH), bg);
            if (r == curRow)
                dl->AddRectFilled(ImVec2(x, y), ImVec2(x + totalW, y + lineH), cCursorRow);

            // Row number.
            snprintf(buf, sizeof buf, "%3d", r);
            dl->AddText(ImVec2(x, y), firstOfBeat ? cRowNumHi : cRowNum, buf);

            for (int c = 0; c < chans; c++)
            {
                const float cx = x + rowNumW + c * chanW;

                // Selection background for the marked channel + row range.
                if (markChn >= 0 && gtui::pattern_actual_channel(c) == markChn &&
                    r >= markLo && r <= markHi)
                    dl->AddRectFilled(ImVec2(cx, y), ImVec2(cx + charW * 8.0f, y + lineH), cSelect);

                // Cursor cell: the exact sub-field the cursor is on. epcolumn
                // 0 = note (3 chars); 1..5 = one nibble at cell offset 2+col.
                if (r == curRow && c == curChn)
                {
                    float cs = (curCol == 0) ? cx : cx + (2 + curCol) * charW;
                    float cw = (curCol == 0) ? charW * 3.0f : charW;
                    dl->AddRectFilled(ImVec2(cs, y), ImVec2(cs + cw, y + lineH), cCursorFill);
                    dl->AddRect(ImVec2(cs, y), ImVec2(cs + cw, y + lineH), cCursorEdge);
                }

                gtui::PatCell cell = gtui::pattern_cell(c, r);
                if (!cell.valid)
                    continue;
                if (cell.end)
                {
                    dl->AddText(ImVec2(cx, y), cEnd, "===");
                    continue;
                }

                // Note (dim an empty/REST note), instrument, command+data.
                const bool emptyNote = (cell.note[0] == '.');
                dl->AddText(ImVec2(cx, y), emptyNote ? cDots : cNote, cell.note);
                if (cell.instr) { snprintf(buf, sizeof buf, "%02X", cell.instr); dl->AddText(ImVec2(cx + charW * 3, y), cInstr, buf); }
                else            { dl->AddText(ImVec2(cx + charW * 3, y), cDots, ".."); }
                if (cell.cmd)   { snprintf(buf, sizeof buf, "%01X%02X", cell.cmd, cell.data); dl->AddText(ImVec2(cx + charW * 5, y), cCmd, buf); }
                else            { dl->AddText(ImVec2(cx + charW * 5, y), cDots, "..."); }
            }
        },
        [&](int r, float localX) {
            float rx = localX - rowNumW;
            if (rx < 0) return;
            int c = (int)(rx / chanW);
            if (c < 0 || c >= chans) return;
            int off = (int)((rx - c * chanW) / charW);
            if (off > 7) off = 7;
            int col = (off < 3) ? 0 : (off - 2);
            gtui::pattern_set_cursor(c, r, col);
        });

    ImGui::End();
}

// Order list, vertical layout (positions = rows, channels = columns) via the
// shared grid scaffold. Cells are pattern numbers or commands (+/-/R/RST).
// Keyboard editing flows through the legacy order editor; clicking places the
// cursor (gtui::order_set_cursor).
static void gimgui_draw_orderlist(void)
{
    const ImGuiCond posCond = g_reset_layout ? ImGuiCond_Always : ImGuiCond_FirstUseEver;
    ImGui::SetNextWindowPos(ImVec2(446, 24), posCond);
    ImGui::SetNextWindowSize(ImVec2(300, 260), posCond);
    if (!ImGui::Begin("Order List"))
    {
        ImGui::End();
        return;
    }

    const ImU32 cCursorRow = IM_COL32(255, 255, 255, 20);
    const ImU32 cSelect    = IM_COL32(48, 96, 200, 110);
    const ImU32 cCursorFill= IM_COL32(235, 225, 120, 70);
    const ImU32 cCursorEdge= IM_COL32(235, 225, 120, 230);
    const ImU32 cRowNum    = IM_COL32(120, 140, 160, 255);
    const ImU32 cPat       = IM_COL32(224, 230, 238, 255);
    const ImU32 cCmd       = IM_COL32(235, 180, 90, 255);
    const ImU32 cLoop      = IM_COL32(150, 160, 175, 255);
    const ImU32 cHeader    = IM_COL32(180, 200, 220, 255);

    const int chans = gtui::order_channels();
    const int rows  = gtui::order_rows();
    const int curRow = gtui::order_cursor_row();
    const int curChn = gtui::order_cursor_chn();
    const int curCol = gtui::order_cursor_col();
    const int markChn = gtui::order_mark_chn();
    int markLo = gtui::order_mark_start(), markHi = gtui::order_mark_end();
    if (markLo > markHi) { int tmp = markLo; markLo = markHi; markHi = tmp; }

    const float charW = ImGui::CalcTextSize("0").x;
    const float lineH = ImGui::GetTextLineHeight();
    const float rowNumW = charW * 4.0f; // "PP "
    const float colW = charW * 4.0f;    // "PP " cell + gutter
    const float totalW = rowNumW + chans * colW;

    // Fixed header: POS + channel numbers.
    {
        ImDrawList *hdl = ImGui::GetWindowDrawList();
        ImVec2 hp = ImGui::GetCursorScreenPos();
        char hbuf[16];
        hdl->AddText(ImVec2(hp.x, hp.y), cHeader, "POS");
        for (int c = 0; c < chans; c++)
        {
            snprintf(hbuf, sizeof hbuf, "CH%X", gtui::order_actual_channel(c));
            hdl->AddText(ImVec2(hp.x + rowNumW + c * colW, hp.y), cHeader, hbuf);
        }
        ImGui::Dummy(ImVec2(totalW, lineH));
        ImGui::Separator();
    }

    gimgui_grid_body(
        "olgrid", rows, totalW, lineH, curRow,
        [&](ImDrawList *dl, int r, float x, float y) {
            char buf[8];
            if (r == curRow)
                dl->AddRectFilled(ImVec2(x, y), ImVec2(x + totalW, y + lineH), cCursorRow);
            snprintf(buf, sizeof buf, "%02X", r);
            dl->AddText(ImVec2(x, y), cRowNum, buf);

            for (int c = 0; c < chans; c++)
            {
                const float cx = x + rowNumW + c * colW;
                if (markChn == c && r >= markLo && r <= markHi)
                    dl->AddRectFilled(ImVec2(cx, y), ImVec2(cx + charW * 3, y + lineH), cSelect);
                if (r == curRow && c == curChn)
                {
                    float cs = cx + (curCol < 0 ? 0 : (curCol > 2 ? 2 : curCol)) * charW;
                    dl->AddRectFilled(ImVec2(cs, y), ImVec2(cs + charW, y + lineH), cCursorFill);
                    dl->AddRect(ImVec2(cs, y), ImVec2(cs + charW, y + lineH), cCursorEdge);
                }

                gtui::OrderCell cell = gtui::order_cell(c, r);
                if (!cell.valid)
                    continue;
                ImU32 col = cell.kind == 2 ? cCmd : cell.kind == 3 ? cLoop : cPat;
                dl->AddText(ImVec2(cx, y), col, cell.text);
            }
        },
        [&](int r, float localX) {
            float rx = localX - rowNumW;
            if (rx < 0) return;
            int c = (int)(rx / colW);
            if (c < 0 || c >= chans) return;
            int off = (int)((rx - c * colW) / charW);
            gtui::order_set_cursor(c, r, off > 2 ? 2 : off);
        });

    ImGui::End();
}

// Instrument table (deviates from the legacy single-instrument view): one
// instrument per row, columns for name + the instrument fields. Read-only for
// now; clicking a row selects that instrument (gtui::instr_select). Reuses the
// shared grid scaffold.
static void gimgui_draw_instruments(void)
{
    const ImGuiCond posCond = g_reset_layout ? ImGuiCond_Always : ImGuiCond_FirstUseEver;
    ImGui::SetNextWindowPos(ImVec2(446, 292), posCond);
    ImGui::SetNextWindowSize(ImVec2(356, 262), posCond);
    if (!ImGui::Begin("Instruments"))
    {
        ImGui::End();
        return;
    }

    const ImU32 cCursorRow = IM_COL32(64, 110, 190, 90);
    const ImU32 cIdx       = IM_COL32(120, 140, 160, 255);
    const ImU32 cName      = IM_COL32(224, 230, 238, 255);
    const ImU32 cVal       = IM_COL32(200, 210, 225, 255);
    const ImU32 cHeader    = IM_COL32(180, 200, 220, 255);

    const int rows = gtui::instr_count();
    const int cur  = gtui::instr_current();

    const float charW = ImGui::CalcTextSize("0").x;
    const float lineH = ImGui::GetTextLineHeight();

    // Column layout, in character units.
    const int   NAMEW = 16;
    const float xIdx  = 0.0f;
    const float xName = 3.0f * charW;
    const float xFields = (3 + NAMEW + 1) * charW;
    const float fieldW = 3.0f * charW; // "XX "
    const char *fieldLabel[10] = { "AD","SR","WA","PU","FI","VB","VD","GT","FW","PN" };
    const float totalW = xFields + 10 * fieldW;

    // Field value for column f of instrument i.
    auto fieldVal = [](int i, int f) -> int {
        switch (f)
        {
        case 0: return gtui::instr_ad(i);
        case 1: return gtui::instr_sr(i);
        case 2: return gtui::instr_ptr(i, 0); // WTBL (wave)
        case 3: return gtui::instr_ptr(i, 1); // PTBL (pulse)
        case 4: return gtui::instr_ptr(i, 2); // FTBL (filter)
        case 5: return gtui::instr_ptr(i, 3); // STBL (speed/vibrato)
        case 6: return gtui::instr_vibdelay(i);
        case 7: return gtui::instr_gatetimer(i);
        case 8: return gtui::instr_firstwave(i);
        default: return gtui::instr_pan(i);
        }
    };

    // Fixed header row.
    {
        ImDrawList *hdl = ImGui::GetWindowDrawList();
        ImVec2 hp = ImGui::GetCursorScreenPos();
        hdl->AddText(ImVec2(hp.x + xIdx, hp.y), cHeader, "##");
        hdl->AddText(ImVec2(hp.x + xName, hp.y), cHeader, "NAME");
        for (int f = 0; f < 10; f++)
            hdl->AddText(ImVec2(hp.x + xFields + f * fieldW, hp.y), cHeader, fieldLabel[f]);
        ImGui::Dummy(ImVec2(totalW, lineH));
        ImGui::Separator();
    }

    gimgui_grid_body(
        "insgrid", rows, totalW, lineH, cur,
        [&](ImDrawList *dl, int r, float x, float y) {
            char buf[24];
            if (r == cur)
                dl->AddRectFilled(ImVec2(x, y), ImVec2(x + totalW, y + lineH), cCursorRow);

            snprintf(buf, sizeof buf, "%02X", r);
            dl->AddText(ImVec2(x + xIdx, y), cIdx, buf);
            snprintf(buf, sizeof buf, "%.16s", gtui::instr_name(r));
            dl->AddText(ImVec2(x + xName, y), cName, buf);
            for (int f = 0; f < 10; f++)
            {
                snprintf(buf, sizeof buf, "%02X", fieldVal(r, f));
                dl->AddText(ImVec2(x + xFields + f * fieldW, y), cVal, buf);
            }
        },
        [&](int r, float) { gtui::instr_select(r); });

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
            if (ImGui::MenuItem("Reset window layout"))
                g_reset_layout = true;
            ImGui::MenuItem("ImGui Demo", nullptr, &g_show_demo);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    gimgui_draw_pattern();
    gimgui_draw_orderlist();
    gimgui_draw_tables();
    gimgui_draw_instruments();
    if (g_show_demo)
        ImGui::ShowDemoWindow(&g_show_demo);

    g_reset_layout = false; // one-shot, consumed by this frame's windows

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
