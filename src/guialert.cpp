//
// Modal alerts via SDL message boxes (M6 Phase 1 — replace chargen wait loops).
//

#include "guialert.hpp"

#include "log.hpp"

#include <SDL.h>

extern SDL_Window* win_window;

static void gt_ui_message(Uint32 flags, const char* title, const char* message)
{
    if (!message) message = "(no message)";
    if (flags & SDL_MESSAGEBOX_ERROR)
        LOG_ERROR("{}", message);
    else if (flags & SDL_MESSAGEBOX_WARNING)
        LOG_WARN("{}", message);
    else
        LOG_INFO("{}", message);

    if (win_window)
        SDL_ShowSimpleMessageBox(flags, title, message, win_window);
}

void gt_ui_error(const char* message) { gt_ui_message(SDL_MESSAGEBOX_ERROR, "GTUltra", message); }
void gt_ui_warn(const char* message) { gt_ui_message(SDL_MESSAGEBOX_WARNING, "GTUltra", message); }
void gt_ui_info(const char* message) { gt_ui_message(SDL_MESSAGEBOX_INFORMATION, "GTUltra", message); }

bool gt_ui_confirm(const char* message)
{
    if (!message) message = "Continue?";
    LOG_INFO("confirm: {}", message);

    const SDL_MessageBoxButtonData buttons[] = {
        { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Yes" },
        { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "No" },
    };
    const SDL_MessageBoxData data = {
        SDL_MESSAGEBOX_WARNING,
        win_window,
        "GTUltra",
        message,
        (int)(sizeof buttons / sizeof buttons[0]),
        buttons,
        nullptr,
    };
    int button = 0;
    if (SDL_ShowMessageBox(&data, &button) < 0) {
        LOG_WARN("SDL_ShowMessageBox failed: {}", SDL_GetError());
        return false;
    }
    return button == 1;
}
