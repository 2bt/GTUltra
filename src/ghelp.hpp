#pragma once

// Online help topic index. Keybind tables are filled from gtaction at draw time;
// reference tabs keep authored prose here.

#include "gactions.hpp"

#include <cstddef>
#include <iostream>
#include <optional>
#include <span>
#include <string_view>

namespace gthelp {

enum class Kind { Keybinds, Reference };

struct Topic {
    std::string_view tab;
    std::string_view title;
    Kind             kind;
    std::optional<gtaction::Ctx> binds; // set when kind == Keybinds
    std::span<const std::string_view> notes; // optional preamble
    std::span<const std::string_view> body;  // reference prose
};

std::span<const Topic> topics();

// Index into topics() for the panel currently being edited, or 0 (General).
std::size_t topic_index_for_edit_panel(int edit_panel);

void print_reference(std::ostream& out = std::cout);

} // namespace gthelp
