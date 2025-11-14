#include "search.hpp"

Move g_best;
uint64_t nodes;
clock_t end_time;
bool early_exit;
bool can_exit;

Move pvtable[MAX_PLY][MAX_PLY];

Value history[2][64][64];

int MVV_LVA[6][6];
int lmr[256][MAX_PLY];

SSEntry ss[MAX_PLY];

__attribute__((constructor)) void init_consts() {
	// MVV-LVA
	for (int i = 0; i < 6; i++) {
		for (int j = 0; j < 6; j++) {
			MVV_LVA[i][j] = PieceValue[i] * (QueenValue / PawnValue) - PieceValue[j];
		}
	}

	// LMR
	for (int i = 1; i <= 255; i++) {
		for (int d = 1; d < MAX_PLY; d++) {
			lmr[i][d] = 0.77 + log(i) * log(d) / 2.36;
		}
	}
}

Value quiesce(Board &b, Value alpha, Value beta) {
	nodes++;

	if ((nodes & 0xfff) == 0) {
		clock_t now = clock();
		if (now >= end_time && can_exit) {
			early_exit = true;
			return 0;
		}
	}

	Value stand_pat = eval(b);

	if (stand_pat >= beta)
		return stand_pat;
	if (stand_pat > alpha)
		alpha = stand_pat;

	pzstd::vector<Move> moves;
	b.captures(moves);

	pzstd::vector<std::pair<int, Move>> order;
	for (auto &m : moves) {
		int score = 0;
		if (b.is_capture(m))
			score += MVV_LVA[b.mailbox[m.dst()] & 0b111][b.mailbox[m.src()] & 0b111];

		order.push_back({-score, m});
	}
	std::stable_sort(order.begin(), order.end());

	Value best = stand_pat;
	for (auto &[_, m] : order) {
		b.make_move(m);
		Value v = -quiesce(b, -beta, -alpha);
		b.unmake_move();

		if (v > best) {
			best = v;
			if (v > alpha)
				alpha = v;
		}
		if (v >= beta)
			return best;
	}

	return best;
}

Value negamax(Board &b, int d, Value alpha, Value beta, int ply, bool root, bool pv) {
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
	if (!root && b.threefold(ply))
		return 0;

	TTEntry *ent = b.ttable.probe(b.zobrist);
	if (ent && ent->depth >= d && !pv) {
		Value ttscore = TTable::mate_from_tt(ent->score, ply);
		if (ent->flag == EXACT)
			return ttscore;
		if (ent->flag == LOWER_BOUND && ttscore >= beta)
			return ttscore;
		if (ent->flag == UPPER_BOUND && ttscore <= alpha)
			return ttscore;
	}

	if (d <= 0) {
		return quiesce(b, alpha, beta);
	}

	// RFP
	Value static_eval = eval(b);
	if (!pv && !in_check && d <= 8) {
		if (static_eval - 100 * d >= beta)
			return static_eval;
	}

	// Razoring
	if (!pv && !in_check && d <= 8 && static_eval + 200 * d < alpha) {
		Value v = quiesce(b, alpha, alpha + 1);
		if (v < alpha)
			return v;
	}

	pzstd::vector<Move> moves;
	b.legal_moves(moves);

	pzstd::vector<std::pair<int, Move>> order;
	for (auto &m : moves) {
		int score = 0;
		if (ent && m == ent->move) {
			score += 10000000;
		} else if (b.is_capture(m)) {
			score += MVV_LVA[b.mailbox[m.dst()] & 0b111][b.mailbox[m.src()] & 0b111] + 1000000;
		} else {
			if (m == ss[ply].killer0 || m == ss[ply].killer1)
				score += 100000;
			score += history[b.side][m.src()][m.dst()];
		}

		order.push_back({-score, m});
	}
	std::stable_sort(order.begin(), order.end());

	TTFlag flag = UPPER_BOUND;
	Value best = -VALUE_INFINITE;
	Move bestmove = NullMove;
	int i = 1;
	for (auto &[_, m] : order) {
		b.make_move(m);

		Value v;
		// LMR
		if (d >= 2 && i >= 2 + 2 * pv) {
			int r = 0;
			r += lmr[i][d];

			v = -negamax(b, d - 1 - r, -beta, -alpha, ply + 1, false, false);
			if (v > alpha)
				v = -negamax(b, d - 1, -alpha - 1, -alpha, ply + 1, false, false);
		} else if (!pv || i > 1) {
			v = -negamax(b, d - 1, -alpha - 1, -alpha, ply + 1, false, false);
		}
		if (pv && (i == 1 || v > alpha)) {
			v = -negamax(b, d - 1, -beta, -alpha, ply + 1, false, true);
		}

		b.unmake_move();

		if (early_exit)
			return 0;

		if (v > best) {
			best = v;
			if (v > alpha) {
				alpha = v;
				bestmove = m;
				flag = EXACT;
			}
		}
		if (v >= beta) {
			if (root)
				g_best = bestmove;

			if (!b.is_capture(m)) {
				if (m != ss[ply].killer0 && m != ss[ply].killer1) {
					ss[ply].killer1 = ss[ply].killer0;
					ss[ply].killer0 = m;
				}

				history[b.side][m.src()][m.dst()] += d * d;
			}

			b.ttable.store(b.zobrist, d, bestmove, best, LOWER_BOUND);
			return best;
		}

		i++;
	}

	// Mate/stalemate detection
	if (best == -VALUE_MATE) {
		if (in_check)
			return -VALUE_MATE + ply;
		else
			return 0;
	}

	if (root)
		g_best = bestmove;

	Value ttscore = TTable::mate_to_tt(best, ply);
	b.ttable.store(b.zobrist, d, bestmove, ttscore, flag);
	return best;
}

void search(Board &b, uint64_t mx_time, int depth) {
	nodes = 0;
	end_time = clock() + mx_time * (CLOCKS_PER_SEC / 1000.0);
	early_exit = false;
	can_exit = false;

	memset(history, 0, sizeof(history));

	for (int i = 1; i <= depth; i++) {
		Value v = negamax(b, i, -VALUE_INFINITE, VALUE_INFINITE, 0, true, true);
		if (early_exit)
			break;
		std::cout << "info depth " << i << " score cp " << v << " pv " << g_best.to_string() << std::endl;
		can_exit = true;
	}

	std::cout << "bestmove " << g_best.to_string() << std::endl;
}
