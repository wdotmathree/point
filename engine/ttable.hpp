#pragma once

#include "includes.hpp"
#include "move.hpp"

#define TT_SIZE (268435456 / sizeof(TTEntry))

enum TTFlag : uint16_t { NONE, EXACT, UPPER_BOUND, LOWER_BOUND };

struct TTEntry {
	uint64_t key;
	uint16_t depth;
	Move move;
	Value score;
	TTFlag flag;
};

class TTable {
private:
	TTEntry *arr;

public:
	TTable() {
		arr = new TTEntry[TT_SIZE];
		memset(arr, 0, sizeof(arr));
	}
	~TTable() {
		delete[] arr;
	}
	TTable &operator=(const TTable &o) {
		if (this != &o) {
			delete[] arr;
			arr = new TTEntry[TT_SIZE];
			memset(arr, 0, sizeof(arr));
		}
		return *this;
	}

	TTEntry *probe(uint64_t key);
	void store(uint64_t key, uint16_t depth, Move move, Value score, TTFlag flag);

	static Value mate_from_tt(Value ttscore, int ply) {
		if (ttscore <= -VALUE_MATE_MAX_PLY)
			return ttscore + ply;
		if (ttscore >= VALUE_MATE_MAX_PLY)
			return ttscore - ply;
		return ttscore;
	}

	static Value mate_to_tt(Value rawscore, int ply) {
		if (rawscore <= -VALUE_MATE_MAX_PLY)
			return rawscore - ply;
		if (rawscore >= VALUE_MATE_MAX_PLY)
			return rawscore + ply;
		return rawscore;
	}
};
