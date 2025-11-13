#include "search.hpp"

Move g_best;
uint64_t nodes;
clock_t end_time;
bool early_exit;
bool can_exit;

Move pvtable[MAX_PLY][MAX_PLY];

int MVV_LVA[6][6];

__attribute__((constructor)) void init_mvvlva() {
	for (int i = 0; i < 6; i++) {
		for (int j = 0; j < 6; j++) {
			MVV_LVA[i][j] = PieceValue[i] * (QueenValue / PawnValue) - PieceValue[j];
		}
	}
}

Value negamax(Board &b, int d, Value alpha, Value beta, int ply, bool root) {
	nodes++;

	if ((nodes & 0xfff) == 0) {
		clock_t now = clock();
		if (now >= end_time && can_exit) {
			early_exit = true;
			return 0;
		}
	}

	bool in_check = false;
	if (b.control(_tzcnt_u64(b.piece_boards[KING] & b.piece_boards[OCC(b.side)]), !b.side))
		in_check = true;
	if (b.control(_tzcnt_u64(b.piece_boards[KING] & b.piece_boards[OPPOCC(b.side)]), b.side)) {
		// We win
		return VALUE_MATE;
	}

	if (d == 0) {
		// quiesce
		return eval(b);
	}

	pzstd::vector<Move> moves;
	b.legal_moves(moves);

	pzstd::vector<std::pair<int, Move>> order;
	for (auto &m : moves) {
		int score = 0;
		if (b.is_capture(m))
			score += MVV_LVA[b.mailbox[m.dst()] & 0b111][b.mailbox[m.src()] & 0b111];

		order.push_back({-score, m});
	}
	std::stable_sort(order.begin(), order.end());

	Value best = -VALUE_INFINITE;
	Move bestmove = NullMove;
	for (auto &[_, m] : order) {
		b.make_move(m);
		Value v = -negamax(b, d - 1, -beta, -alpha, ply + 1, false);
		b.unmake_move();

		if (early_exit)
			return 0;

		if (v > best) {
			best = v;
			if (v > alpha) {
				alpha = v;
				bestmove = m;
			}
		}
		if (v >= beta) {
			if (root)
				g_best = bestmove;

			return best;
		}
	}

	// Stalemate
	if (best == -VALUE_MATE) {
		if (in_check)
			return -VALUE_MATE + ply;
		else
			return 0;
	}

	if (root)
		g_best = bestmove;
	return best;
}

void search(Board &b, uint64_t mx_time, int depth) {
	nodes = 0;
	end_time = clock() + mx_time * (CLOCKS_PER_SEC / 1000.0);
	early_exit = false;
	can_exit = false;

	for (int i = 1; i <= depth; i++) {
		Value v = negamax(b, i, -VALUE_INFINITE, VALUE_INFINITE, 0, true);
		if (early_exit)
			break;
		std::cout << "info depth " << i << " score cp " << v << " pv " << g_best.to_string() << std::endl;
		can_exit = true;
	}

	std::cout << "bestmove " << g_best.to_string() << std::endl;
}
