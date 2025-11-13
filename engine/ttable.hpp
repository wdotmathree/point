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
	}
	~TTable() {
		delete[] arr;
	}
	TTable(TTable &x) = delete;
	TTable(TTable *x) = delete;

	TTEntry *probe(uint64_t key);
	void store(uint64_t key, uint16_t depth, Move move, Value score, TTFlag flag);

	static Value mate_from_tt(Value ttscore, int ply) {
		if (ttscore < 0)
			return ttscore + ply;
		else
			return ttscore - ply;
	}

	static Value mate_to_tt(Value rawscore, int ply) {
		if (rawscore < 0)
			return rawscore - ply;
		else
			return rawscore + ply;
	}
};
