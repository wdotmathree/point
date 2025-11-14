#include "ttable.hpp"

TTEntry *TTable::probe(uint64_t key) {
	if (arr[key % TT_SIZE].key == key)
		return &arr[key % TT_SIZE];
	return nullptr;
}

void TTable::store(uint64_t key, uint16_t depth, Move move, Value score, TTFlag flag) {
	TTEntry &ent = arr[key % TT_SIZE];
	ent.key = key;
	ent.depth = depth;
	ent.move = move;
	ent.score = score;
	ent.flag = flag;
}
