#pragma once

#include "includes.hpp"

#include "bitboard.hpp"
#include "eval.hpp"
// #include "history.hpp"
#include "movegen.hpp"
// #include "movepicker.hpp"
// #include "ttable.hpp"

#include <algorithm>

extern uint64_t nodes;

void search(Board &b, uint64_t mx_time, int depth);

struct SSEntry {
	Move killer0, killer1;
};
