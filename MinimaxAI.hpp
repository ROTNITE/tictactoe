
#pragma once
#include "Board.hpp"
#include "HashMap.hpp"
#include "DynamicArray.hpp"
#include <limits>
#include <chrono>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <utility>
#include <atomic>
#include <cstdint>
#include <future>
#include <thread>


class SearchParams;
class AnalysisResult;
enum class Player;
enum class GameMode;

enum class Player {
    X,
    O
};

enum class MoveGenMode {
    Full,
    Frontier,
    Hybrid
};

AnalysisResult analysePosition(const Board& b, Player toMove, GameMode mode, const SearchParams& params, int scoreX = 0, int scoreO = 0, std::atomic<bool>* cancelFlag = nullptr);

class MoveEvaluation {
public:
    Coord move;
    int score;

    MoveEvaluation() : move(-1, -1), score(std::numeric_limits<int>::min()) {}
    MoveEvaluation(const Coord& m, int s) : move(m), score(s) {}
};

class AIStatistics {
public:
    size_t nodesVisited;   
    size_t nodesGenerated; 
    size_t cacheHits;      
    size_t cacheMisses;    
    long long timeMs;      
    int completedDepth;    

    AIStatistics()
        : nodesVisited(0),
        nodesGenerated(0),
        cacheHits(0),
        cacheMisses(0),
        timeMs(0),
        completedDepth(0) {}

    void reset() {
        nodesVisited = 0;
        nodesGenerated = 0;
        cacheHits = 0;
        cacheMisses = 0;
        timeMs = 0;
        completedDepth = 0;
    }

    void print() const {
        std::cout << "Статистика работы ИИ:\n";
        std::cout << "  Посещено узлов: " << nodesVisited << "\n";
        std::cout << "  Сгенерировано узлов: " << nodesGenerated << "\n";
        std::cout << "  Попаданий в кеш: " << cacheHits << "\n";
        std::cout << "  Промахов мимо кеша: " << cacheMisses << "\n";
        std::cout << "  Время работы: " << timeMs << " мс\n";
        if (cacheHits + cacheMisses > 0) {
            double hitRate =
                100.0 * static_cast<double>(cacheHits)
                / static_cast<double>(cacheHits + cacheMisses);
            std::cout << "  Доля попаданий в кеш: " << hitRate << "%\n";
        }
    }
};

using Move        = Coord;
using SearchStats = AIStatistics;

class MinimaxAI {
private:
    friend AnalysisResult analysePosition(const Board& b, Player toMove, GameMode mode, const SearchParams& params, int scoreX, int scoreO, std::atomic<bool>* cancelFlag);
    Player   player_;
    Player   opponent_;
    int      maxDepth_;        
    bool     useMemoization_;  
    GameMode mode_;            
    std::atomic<bool>* cancelFlag_ = nullptr;
    MoveEvaluation* bestSoFarPtr_ = nullptr;
    DynamicArray<int>* historyTable_ = nullptr;
    int creditedX_ = 0;
    int creditedO_ = 0;
    static constexpr int WIN_SCORE = 1000000000;
    static constexpr int MAX_KILLER_DEPTH = 64;
    Coord killerMoves_[MAX_KILLER_DEPTH][2]{};
    DynamicArray<uint64_t> zobristTable_;
    uint64_t zobristPlayerX_ = 0;
    uint64_t zobristPlayerO_ = 0;
    int timeLimitMs_ = -1;
    std::chrono::steady_clock::time_point startTime_;
    struct WindowInfo {
        int row = 0;
        int col = 0;
        int dr = 0;
        int dc = 0;
        int xCount = 0;
        int oCount = 0;
    };
    DynamicArray<WindowInfo> windows_;
    DynamicArray<DynamicArray<int>> cellWindows_;
    DynamicArray<int> windowWeight_;
    DynamicArray<int> posValues_;
    int64_t windowScoreSum_ = 0;
    int64_t centerBias_ = 0;
    bool evalReady_ = false;
    int evalRows_ = 0;
    int evalCols_ = 0;
    int evalWinLen_ = 0;
    GameMode evalMode_ = GameMode::Classic;
    MoveGenMode moveGenMode_ = MoveGenMode::Hybrid;
    bool useLMR_ = true;
    bool useExtensions_ = true;
    bool perfectClassic3_ = false;
    bool allowOpeningShortcut_ = true;

    class TTEntry {
    public:
        int score = 0;
        int depth = -1;
        enum class Flag { Exact, Lower, Upper } flag = Flag::Exact;
        MoveEvaluation bestMove;
        uint64_t generation = 0;
    };

    
    HashMap<size_t, TTEntry> transpositionTable_;
    uint64_t ttGeneration_ = 0;

    AIStatistics stats_;

    

    CellState playerToCell(Player p) const {
        return p == Player::X ? CellState::X : CellState::O;
    }

    Player getOpponent(Player p) const {
        return p == Player::X ? Player::O : Player::X;
    }

    void initZobrist(int rows, int cols) {
        size_t sz = static_cast<size_t>(rows * cols * 2);
        zobristTable_.clear();
        zobristTable_.reserve(sz);
        uint64_t seed = 0x9e3779b97f4a7c15ull ^ static_cast<uint64_t>(rows * 131 + cols);
        auto nextRand = [&seed]() {
            seed ^= seed >> 12;
            seed ^= seed << 25;
            seed ^= seed >> 27;
            return seed * 0x2545F4914F6CDD1Dull;
        };
        for (size_t i = 0; i < sz; ++i) {
            zobristTable_.push_back(nextRand());
        }
        zobristPlayerX_ = nextRand();
        zobristPlayerO_ = nextRand();
    }

    uint64_t computeZobrist(const Board& board, Player toMove) const {
        int rows = board.getRows();
        int cols = board.getCols();
        int winLen = board.getWinLength();
        uint64_t h = 0;
        int total = rows * cols;
        for (int i = 0; i < total; ++i) {
            CellState cell = board.getNoCheck(i / cols, i % cols);
            if (cell == CellState::Empty) continue;
            size_t idx = static_cast<size_t>(i * 2 + (cell == CellState::X ? 0 : 1));
            if (idx < zobristTable_.size()) {
                h ^= zobristTable_[idx];
            }
        }
        h ^= (toMove == Player::X ? zobristPlayerX_ : zobristPlayerO_);
        return h;
    }

    
    size_t makeHashKey(uint64_t hashKey, int rows, int cols, int winLen, int scoreX, int scoreO) const
    {
        size_t h = hashKey;

        auto mix = [](size_t& h, size_t v) {
            h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
        };

        mix(h, static_cast<size_t>(rows));
        mix(h, static_cast<size_t>(cols));
        mix(h, static_cast<size_t>(winLen));
        mix(h, static_cast<size_t>(mode_ == GameMode::Classic ? 0u : 1u));
        if (mode_ == GameMode::LinesScore) {
            mix(h, static_cast<size_t>(scoreX));
            mix(h, static_cast<size_t>(scoreO));
        }

        return h;
    }

    inline uint64_t toggleToMove(uint64_t h, Player p) const {
        return h ^ (p == Player::X ? zobristPlayerX_ : zobristPlayerO_);
    }

    inline uint64_t togglePiece(uint64_t h, int row, int col, int cols, CellState cell) const {
        size_t idx = static_cast<size_t>((row * cols + col) * 2 + (cell == CellState::X ? 0 : 1));
        if (idx < zobristTable_.size()) {
            h ^= zobristTable_[idx];
        }
        return h;
    }

    void buildEvalTables(int rows, int cols, int winLen) {
        windows_.clear();
        cellWindows_.clear();
        posValues_.clear();
        windowWeight_.clear();

        int totalCells = rows * cols;
        cellWindows_.reserve(static_cast<size_t>(totalCells));
        for (int i = 0; i < totalCells; ++i) {
            cellWindows_.push_back(DynamicArray<int>());
        }

        auto addWindow = [&](int r, int c, int dr, int dc) {
            WindowInfo w;
            w.row = r;
            w.col = c;
            w.dr = dr;
            w.dc = dc;
            w.xCount = 0;
            w.oCount = 0;
            int idx = static_cast<int>(windows_.size());
            windows_.push_back(w);
            for (int k = 0; k < winLen; ++k) {
                int rr = r + k * dr;
                int cc = c + k * dc;
                int cellIndex = rr * cols + cc;
                cellWindows_[cellIndex].push_back(idx);
            }
        };

        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col <= cols - winLen; ++col) {
                addWindow(row, col, 0, 1);
            }
        }
        for (int col = 0; col < cols; ++col) {
            for (int row = 0; row <= rows - winLen; ++row) {
                addWindow(row, col, 1, 0);
            }
        }
        for (int row = 0; row <= rows - winLen; ++row) {
            for (int col = 0; col <= cols - winLen; ++col) {
                addWindow(row, col, 1, 1);
            }
        }
        for (int row = 0; row <= rows - winLen; ++row) {
            for (int col = winLen - 1; col < cols; ++col) {
                addWindow(row, col, 1, -1);
            }
        }

        windowWeight_.reserve(static_cast<size_t>(winLen + 1));
        windowWeight_.push_back(0);
        if (mode_ == GameMode::Classic) {
            for (int i = 1; i <= winLen; ++i) {
                int w = i * i * winLen;
                if (i == winLen - 1) {
                    w += winLen * winLen * 4;
                }
                if (i == winLen) {
                    w = WIN_SCORE / 16;
                }
                windowWeight_.push_back(w);
            }
        } else {
            int winScore = 2000 + winLen * 80;
            for (int i = 1; i <= winLen; ++i) {
                int w = i * i * winLen * 3;
                if (i == winLen - 1) {
                    w += winLen * winLen * 2;
                }
                if (i == winLen) {
                    w = winScore;
                }
                windowWeight_.push_back(w);
            }
        }

        int centerRow = rows / 2;
        int centerCol = cols / 2;
        int minSide = std::min(rows, cols);
        posValues_.reserve(static_cast<size_t>(totalCells));
        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col < cols; ++col) {
                int centerDist = std::abs(row - centerRow) + std::abs(col - centerCol);
                posValues_.push_back(minSide - centerDist);
            }
        }
    }

    int64_t windowScoreForPlayer(int xCount, int oCount) const {
        int own = (player_ == Player::X) ? xCount : oCount;
        int opp = (player_ == Player::X) ? oCount : xCount;
        if (own > 0 && opp > 0) return 0;
        if (own > 0) return static_cast<int64_t>(windowWeight_[own]);
        if (opp > 0) return -static_cast<int64_t>(windowWeight_[opp]);
        return 0;
    }

    void initEvalCache(const Board& board) {
        int rows = board.getRows();
        int cols = board.getCols();
        int winLen = board.getWinLength();
        if (!evalReady_ || rows != evalRows_ || cols != evalCols_ || winLen != evalWinLen_ || evalMode_ != mode_) {
            buildEvalTables(rows, cols, winLen);
            evalRows_ = rows;
            evalCols_ = cols;
            evalWinLen_ = winLen;
            evalMode_ = mode_;
            evalReady_ = true;
        }

        windowScoreSum_ = 0;
        centerBias_ = 0;
        for (size_t i = 0; i < windows_.size(); ++i) {
            WindowInfo& w = windows_[i];
            int xCount = 0;
            int oCount = 0;
            int r = w.row;
            int c = w.col;
            for (int k = 0; k < winLen; ++k) {
                CellState cell = board.getNoCheck(r + k * w.dr, c + k * w.dc);
                if (cell == CellState::X) ++xCount;
                else if (cell == CellState::O) ++oCount;
            }
            w.xCount = xCount;
            w.oCount = oCount;
            windowScoreSum_ += windowScoreForPlayer(xCount, oCount);
        }

        CellState playerCell = playerToCell(player_);
        CellState opponentCell = playerToCell(opponent_);
        int totalCells = rows * cols;
        for (int idx = 0; idx < totalCells; ++idx) {
            int row = idx / cols;
            int col = idx % cols;
            CellState cell = board.getNoCheck(row, col);
            if (cell == CellState::Empty) continue;
            int pos = posValues_[idx];
                if (cell == playerCell) centerBias_ += pos;
                else if (cell == opponentCell) centerBias_ -= pos;
        }
    }

    int applyMoveEval(Board& board, const Coord& mv, CellState cell) {
        board.set(mv, cell);
        if (!evalReady_) return 0;
        int idx = mv.row() * evalCols_ + mv.col();
        if (idx < 0 || idx >= static_cast<int>(cellWindows_.size())) {
#ifndef NDEBUG
            std::cerr << "applyMoveEval: invalid idx " << idx
                      << " for move (" << mv.row() << "," << mv.col()
                      << "), evalCols=" << evalCols_
                      << ", size=" << cellWindows_.size() << "\n";
#endif
            return 0;
        }
        DynamicArray<int>& wins = cellWindows_[idx];
        int gained = 0;
        for (size_t i = 0; i < wins.size(); ++i) {
            int widx = wins[i];
            WindowInfo& w = windows_[widx];
            int64_t oldScore = windowScoreForPlayer(w.xCount, w.oCount);
            int ownCount = (cell == CellState::X) ? w.xCount : w.oCount;
            int oppCount = (cell == CellState::X) ? w.oCount : w.xCount;
            if (cell == CellState::X) ++w.xCount;
            else if (cell == CellState::O) ++w.oCount;
            int64_t newScore = windowScoreForPlayer(w.xCount, w.oCount);
            windowScoreSum_ += (newScore - oldScore);
            if (oppCount == 0 && ownCount + 1 == evalWinLen_) {
                ++gained;
            }
        }
        int pos = posValues_[idx];
        if (cell == playerToCell(player_)) centerBias_ += pos;
        else if (cell == playerToCell(opponent_)) centerBias_ -= pos;
        return gained;
    }

    void undoMoveEval(Board& board, const Coord& mv, CellState cell) {
        board.set(mv, CellState::Empty);
        if (!evalReady_) return;
        int idx = mv.row() * evalCols_ + mv.col();
        if (idx < 0 || idx >= static_cast<int>(cellWindows_.size())) {
#ifndef NDEBUG
            std::cerr << "undoMoveEval: invalid idx " << idx
                      << " for move (" << mv.row() << "," << mv.col()
                      << "), evalCols=" << evalCols_
                      << ", size=" << cellWindows_.size() << "\n";
#endif
            return;
        }
        DynamicArray<int>& wins = cellWindows_[idx];
        for (size_t i = 0; i < wins.size(); ++i) {
            int widx = wins[i];
            WindowInfo& w = windows_[widx];
            int64_t oldScore = windowScoreForPlayer(w.xCount, w.oCount);
            if (cell == CellState::X) --w.xCount;
            else if (cell == CellState::O) --w.oCount;
            int64_t newScore = windowScoreForPlayer(w.xCount, w.oCount);
            windowScoreSum_ += (newScore - oldScore);
        }
        int pos = posValues_[idx];
        if (cell == playerToCell(player_)) centerBias_ -= pos;
        else if (cell == playerToCell(opponent_)) centerBias_ += pos;
    }

    int clampScore(int64_t v) const {
        if (v > std::numeric_limits<int>::max()) return std::numeric_limits<int>::max();
        if (v < std::numeric_limits<int>::min()) return std::numeric_limits<int>::min();
        return static_cast<int>(v);
    }

    int baseScorePerspective(int scoreX, int scoreO) const {
        return (player_ == Player::X) ? (scoreX - scoreO) : (scoreO - scoreX);
    }

    
    int evaluateHeuristicClassic(const Board& board) const {
        (void)board;
        return clampScore(windowScoreSum_ + centerBias_);
    }

    int evaluateHeuristicLines(const Board& board, int scoreX, int scoreO) const {
        (void)board;
        int64_t score = static_cast<int64_t>(baseScorePerspective(scoreX, scoreO)) * 1000;
        score += windowScoreSum_ / 2;
        score += centerBias_ / 2;
        return clampScore(score);
    }

    int evaluateHeuristic(const Board& board, int scoreX, int scoreO) const {
        if (mode_ == GameMode::Classic) {
            return evaluateHeuristicClassic(board);
        }
        return evaluateHeuristicLines(board, scoreX, scoreO);
    }

    DynamicArray<Coord> getSearchMoves(const Board& board, int depth) const {
        int rows = board.getRows();
        int cols = board.getCols();
        if (rows <= 4 && cols <= 4) {
            return board.getEmptyCells();
        }
        if (moveGenMode_ != MoveGenMode::Full && (rows >= 6 || cols >= 6)) {
            bool anyStone = false;
            for (int row = 0; row < rows && !anyStone; ++row) {
                for (int col = 0; col < cols; ++col) {
                    if (board.getNoCheck(row, col) != CellState::Empty) {
                        anyStone = true;
                        break;
                    }
                }
            }
            if (!anyStone) {
                int centerRowLow = (rows - 1) / 2;
                int centerRowHigh = rows / 2;
                int centerColLow = (cols - 1) / 2;
                int centerColHigh = cols / 2;
                int radius = (std::min(rows, cols) >= 9) ? 2 : 1;
                int r0 = std::max(0, centerRowLow - radius);
                int r1 = std::min(rows - 1, centerRowHigh + radius);
                int c0 = std::max(0, centerColLow - radius);
                int c1 = std::min(cols - 1, centerColHigh + radius);
                DynamicArray<Coord> moves;
                moves.reserve(static_cast<size_t>((r1 - r0 + 1) * (c1 - c0 + 1)));
                bool banCenter = (mode_ == GameMode::LinesScore && player_ == Player::X &&
                                  (rows % 2 == 1) && (cols % 2 == 1));
                int cr = rows / 2;
                int cc = cols / 2;
                for (int row = r0; row <= r1; ++row) {
                    for (int col = c0; col <= c1; ++col) {
                        if (banCenter && row == cr && col == cc) continue;
                        moves.push_back(Coord(row, col));
                    }
                }
                return moves;
            }
        }
        if (moveGenMode_ == MoveGenMode::Full) {
            return board.getEmptyCells();
        }
        if (moveGenMode_ == MoveGenMode::Frontier) {
            return board.getCandidateMoves(1);
        }
        DynamicArray<Coord> moves = board.getCandidateMoves(1);
        int area = rows * cols;
        if (depth >= 3 && area >= 64) {
            int winLen = board.getWinLength();
            int minDesired = std::max(12, winLen * (depth >= 6 ? 6 : 4));
            if (moves.size() < static_cast<size_t>(minDesired)) {
                DynamicArray<Coord> wider = board.getCandidateMoves(2);
                if (wider.size() > moves.size()) {
                    moves = std::move(wider);
                }
            }
            if (depth >= 6 && area >= 100 && moves.size() < static_cast<size_t>(minDesired)) {
                DynamicArray<Coord> wider = board.getCandidateMoves(3);
                if (wider.size() > moves.size()) {
                    moves = std::move(wider);
                }
            }
        }
        return moves;
    }

    int minimax(Board& board, int depth, int alpha, int beta, Player currentPlayer, bool isMaximizing, uint64_t hashKey, const Coord& lastMove, int scoreX, int scoreO, int extensionBudget) {

        stats_.nodesVisited++;

        if (isCancelled()) {
            return evaluateHeuristic(board, scoreX, scoreO);
        }

        int alphaOriginal = alpha;
        int betaOriginal  = beta;
        int rows = board.getRows();
        int cols = board.getCols();
        int winLen = board.getWinLength();
        CellState playerCell   = playerToCell(player_);

        
        if (mode_ == GameMode::Classic) {
            
            if (lastMove.row() < 0) {
                bool xWin = board.checkWin(CellState::X);
                bool oWin = board.checkWin(CellState::O);
                if (xWin || oWin) {
                    if (xWin && !oWin) {
                        return (playerCell == CellState::X) ? (WIN_SCORE + depth) : (-WIN_SCORE - depth);
                    }
                    if (oWin && !xWin) {
                        return (playerCell == CellState::O) ? (WIN_SCORE + depth) : (-WIN_SCORE - depth);
                    }
                    return 0;
                }
            } else {
                CellState lastCell = playerToCell(getOpponent(currentPlayer));
                if (board.checkWinFromMove(lastMove, lastCell)) {
                    if (lastCell == playerCell) return WIN_SCORE + depth;
                    else return -WIN_SCORE - depth;
                }
            }
            if (board.isFull()) {
                return 0;
            }
            if (depth <= 0) {
                return evaluateHeuristic(board, scoreX, scoreO);
            }
        } else { 
            if (board.isFull()) {
                
                return baseScorePerspective(scoreX, scoreO) * 1000;
            }
            
            if (depth <= 0) {
                return evaluateHeuristic(board, scoreX, scoreO);
            }
        }

        
        if (useMemoization_) {
            size_t key = makeHashKey(hashKey, rows, cols, winLen, scoreX, scoreO);
            if (transpositionTable_.contains(key)) {
                const TTEntry& e = transpositionTable_.get(key);
                if (e.generation != ttGeneration_) {
                    stats_.cacheMisses++;
                } else if (e.depth >= depth) {
                    stats_.cacheHits++;
                    if (e.flag == TTEntry::Flag::Exact) {
                        return e.score;
                    } else if (e.flag == TTEntry::Flag::Lower && e.score > alpha) {
                        alpha = e.score;
                    } else if (e.flag == TTEntry::Flag::Upper && e.score < beta) {
                        beta = e.score;
                    }
                    if (alpha >= beta) {
                        return e.score;
                    }
                    if (e.bestMove.move.row() >= 0 && e.bestMove.move.col() >= 0) {
                        
                    }
                } else {
                    stats_.cacheMisses++;
                }
            } else {
                stats_.cacheMisses++;
            }
        }

        DynamicArray<Coord> moves = getSearchMoves(board, depth);
        stats_.nodesGenerated += moves.size();

        Coord ttHint(-1, -1);
        
        if (useMemoization_) {
            size_t key = makeHashKey(hashKey, rows, cols, winLen, scoreX, scoreO);
            if (transpositionTable_.contains(key)) {
                const TTEntry& e = transpositionTable_.get(key);
                if (e.generation == ttGeneration_ &&
                    e.depth >= depth &&
                    e.bestMove.move.row() >= 0 && e.bestMove.move.col() >= 0) {
                    ttHint = e.bestMove.move;
                }
            }
        }

        auto moveUrgency = [&](const Coord& mv, bool heavy) -> int {
            int bonus = 0;
            Board& b = board;
            CellState curCell = playerToCell(currentPlayer);
            CellState oppCell = playerToCell(getOpponent(currentPlayer));
            if (heavy) {
                if (mode_ == GameMode::LinesScore) {
                    int gainedSelf = applyMoveEval(b, mv, curCell);
                    bonus += gainedSelf * 2500;
                    undoMoveEval(b, mv, curCell);

                    int gainedOpp = applyMoveEval(b, mv, oppCell);
                    bonus += gainedOpp * 1800;
                    undoMoveEval(b, mv, oppCell);
                } else {
                    applyMoveEval(b, mv, curCell);
                    if (b.checkWinFromMove(mv, curCell)) bonus += 5000;
                    undoMoveEval(b, mv, curCell);

                    applyMoveEval(b, mv, oppCell);
                    if (b.checkWinFromMove(mv, oppCell)) bonus += 3000;
                    undoMoveEval(b, mv, oppCell);
                }
            } else {
                int idx = mv.row() * b.getCols() + mv.col();
                if (idx >= 0 && idx < static_cast<int>(posValues_.size())) {
                    bonus += posValues_[idx];
                }
            }

            int idx = mv.row() * b.getCols() + mv.col();
            if (historyTable_ && idx >= 0 && idx < static_cast<int>(historyTable_->size())) {
                bonus += (*historyTable_)[idx];
            }
            if (depth < MAX_KILLER_DEPTH) {
                if (mv == killerMoves_[depth][0]) bonus += 4000;
                else if (mv == killerMoves_[depth][1]) bonus += 3000;
            }
            if (mv == ttHint) bonus += 8000;
            return bonus;
        };

        DynamicArray<std::pair<Coord,int>> moveList;
        moveList.reserve(moves.size());
        size_t heavyBudget = std::min(moves.size(), static_cast<size_t>(depth >= 6 ? 24 : 16));
        for (size_t i = 0; i < moves.size(); ++i) {
            bool heavy = (i < heavyBudget);
            moveList.push_back(std::pair<Coord,int>(moves[i], moveUrgency(moves[i], heavy)));
        }

        auto cmpUrgency = [](const std::pair<Coord,int>& a, const std::pair<Coord,int>& b) {
            return a.second > b.second;
        };

        DynamicArray<char> used;
        used.reserve(moveList.size());
        for (size_t i = 0; i < moveList.size(); ++i) used.push_back(0);

        DynamicArray<std::pair<Coord,int>> ordered;
        ordered.reserve(moveList.size());

        auto takeIfPresent = [&](const Coord& target) {
            if (target.row() < 0 || target.col() < 0) return;
            for (size_t i = 0; i < moveList.size(); ++i) {
                if (used[i]) continue;
                if (moveList[i].first == target) {
                    used[i] = 1;
                    ordered.push_back(moveList[i]);
                    return;
                }
            }
        };

        if (ttHint.row() >= 0 && ttHint.col() >= 0) {
            takeIfPresent(ttHint);
        }
        if (depth < MAX_KILLER_DEPTH) {
            takeIfPresent(killerMoves_[depth][0]);
            takeIfPresent(killerMoves_[depth][1]);
        }

        DynamicArray<std::pair<Coord,int>> rest;
        rest.reserve(moveList.size());
        for (size_t i = 0; i < moveList.size(); ++i) {
            if (!used[i]) rest.push_back(moveList[i]);
        }

        size_t topKBase = static_cast<size_t>(std::max(12, winLen * 3));
        size_t topK = std::min(rest.size(), topKBase);
        if (topK > 0) {
            if (rest.size() > topK) {
                std::nth_element(rest.begin(), rest.begin() + topK, rest.end(), cmpUrgency);
                std::sort(rest.begin(), rest.begin() + topK, cmpUrgency);
            } else {
                std::sort(rest.begin(), rest.end(), cmpUrgency);
            }
            for (size_t i = 0; i < topK; ++i) ordered.push_back(rest[i]);
            for (size_t i = topK; i < rest.size(); ++i) ordered.push_back(rest[i]);
        } else {
            for (size_t i = 0; i < rest.size(); ++i) ordered.push_back(rest[i]);
        }

        moves.clear();
        DynamicArray<int> urgencies;
        urgencies.reserve(ordered.size());
        for (const auto& p : ordered) {
            moves.push_back(p.first);
            urgencies.push_back(p.second);
        }

        int       bestScore;
        CellState currentCell = playerToCell(currentPlayer);
        Coord bestMoveCoord(-1, -1);
        size_t maxMoves = moves.size();
        int area = rows * cols;
        if (area >= 64) {
            size_t cap = depth >= 6 ? static_cast<size_t>(24) : static_cast<size_t>(32);
            if (maxMoves > cap) maxMoves = cap;
        }
        if (useLMR_ && timeLimitMs_ > 0 && depth >= 8 && moves.size() > 1) {
            size_t cap = static_cast<size_t>(std::max(24, winLen * 6));
            if (maxMoves > cap) maxMoves = cap;
        }

        if (isMaximizing) {
            bestScore = std::numeric_limits<int>::min();

            for (size_t i = 0; i < maxMoves; ++i) {
                int gained = applyMoveEval(board, moves[i], currentCell);
                int nextDepth = depth - 1;
                int nextExtensionBudget = extensionBudget;
                int urgency = (i < urgencies.size()) ? urgencies[i] : moveUrgency(moves[i], false);
                bool tactical = (mode_ == GameMode::LinesScore && gained > 0) || urgency >= 3000;
                if (useExtensions_ && tactical && mode_ == GameMode::LinesScore && gained > 0 && depth <= 6 && extensionBudget > 0) {
                    nextDepth = depth;
                    nextExtensionBudget = extensionBudget - 1;
                } else if (useLMR_ && !tactical && timeLimitMs_ > 0 && depth >= 8 && moves.size() > 12 && i > 8 && urgency < 4000) {
                    nextDepth = depth - 2;
                    if (depth >= 10 && moves.size() > 18 && i > 14 && urgency < 2000) {
                        nextDepth = depth - 3;
                    }
                    if (nextDepth < 1) nextDepth = 1;
                }
                uint64_t childHash = hashKey;
                childHash = toggleToMove(childHash, currentPlayer);
                childHash = togglePiece(childHash, moves[i].row(), moves[i].col(), cols, currentCell);
                childHash = toggleToMove(childHash, getOpponent(currentPlayer));

                int nextScoreX = scoreX;
                int nextScoreO = scoreO;
                if (mode_ == GameMode::LinesScore) {
                    int delta = std::min(2, std::max(0, gained));
                    if (currentCell == CellState::X) nextScoreX += delta;
                    else nextScoreO += delta;
                }

                int score;
                if (i == 0 || static_cast<long long>(alpha) >= static_cast<long long>(beta) - 1) {
                    score = minimax(board, nextDepth, alpha, beta,
                                    getOpponent(currentPlayer), false, childHash, moves[i], nextScoreX, nextScoreO, nextExtensionBudget);
                } else {
                    int scout = minimax(board, nextDepth, alpha, alpha + 1,
                                        getOpponent(currentPlayer), false, childHash, moves[i], nextScoreX, nextScoreO, nextExtensionBudget);
                    if (scout > alpha && scout < beta) {
                        score = minimax(board, nextDepth, alpha, beta,
                                        getOpponent(currentPlayer), false, childHash, moves[i], nextScoreX, nextScoreO, nextExtensionBudget);
                    } else {
                        score = scout;
                    }
                }
                undoMoveEval(board, moves[i], currentCell);

                if (score > bestScore) {
                    bestScore = score;
                    bestMoveCoord = moves[i];
                }
                alpha = std::max(alpha, bestScore);

                if (isCancelled()) {
                    break;
                }
                if (beta <= alpha) {
                    if (depth < MAX_KILLER_DEPTH) {
                        if (killerMoves_[depth][0] != moves[i]) {
                            killerMoves_[depth][1] = killerMoves_[depth][0];
                            killerMoves_[depth][0] = moves[i];
                        }
                    }
                    break;
                }
            }
        } else {
            bestScore = std::numeric_limits<int>::max();

            for (size_t i = 0; i < maxMoves; ++i) {
                int gained = applyMoveEval(board, moves[i], currentCell);
                int nextDepth = depth - 1;
                int nextExtensionBudget = extensionBudget;
                int urgency = (i < urgencies.size()) ? urgencies[i] : moveUrgency(moves[i], false);
                bool tactical = (mode_ == GameMode::LinesScore && gained > 0) || urgency >= 3000;
                if (useExtensions_ && tactical && mode_ == GameMode::LinesScore && gained > 0 && depth <= 6 && extensionBudget > 0) {
                    nextDepth = depth;
                    nextExtensionBudget = extensionBudget - 1;
                } else if (useLMR_ && !tactical && timeLimitMs_ > 0 && depth >= 8 && moves.size() > 12 && i > 8 && urgency < 4000) {
                    nextDepth = depth - 2;
                    if (depth >= 10 && moves.size() > 18 && i > 14 && urgency < 2000) {
                        nextDepth = depth - 3;
                    }
                    if (nextDepth < 1) nextDepth = 1;
                }
                uint64_t childHash = hashKey;
                childHash = toggleToMove(childHash, currentPlayer);
                childHash = togglePiece(childHash, moves[i].row(), moves[i].col(), cols, currentCell);
                childHash = toggleToMove(childHash, getOpponent(currentPlayer));

                int nextScoreX = scoreX;
                int nextScoreO = scoreO;
                if (mode_ == GameMode::LinesScore) {
                    int delta = std::min(2, std::max(0, gained));
                    if (currentCell == CellState::X) nextScoreX += delta;
                    else nextScoreO += delta;
                }

                int score;
                if (i == 0 || static_cast<long long>(alpha) >= static_cast<long long>(beta) - 1) {
                    score = minimax(board, nextDepth, alpha, beta,
                                    getOpponent(currentPlayer), true, childHash, moves[i], nextScoreX, nextScoreO, nextExtensionBudget);
                } else {
                    int scout = minimax(board, nextDepth, beta - 1, beta,
                                        getOpponent(currentPlayer), true, childHash, moves[i], nextScoreX, nextScoreO, nextExtensionBudget);
                    if (scout > alpha && scout < beta) {
                        score = minimax(board, nextDepth, alpha, beta,
                                        getOpponent(currentPlayer), true, childHash, moves[i], nextScoreX, nextScoreO, nextExtensionBudget);
                    } else {
                        score = scout;
                    }
                }
                undoMoveEval(board, moves[i], currentCell);

                if (score < bestScore) {
                    bestScore = score;
                    bestMoveCoord = moves[i];
                }
                beta = std::min(beta, bestScore);

                if (isCancelled()) {
                    break;
                }
                if (beta <= alpha) {
                    if (depth < MAX_KILLER_DEPTH) {
                        if (killerMoves_[depth][0] != moves[i]) {
                            killerMoves_[depth][1] = killerMoves_[depth][0];
                            killerMoves_[depth][0] = moves[i];
                        }
                    }
                    break;
                }
            }
        }

        if (useMemoization_) {
            size_t key = makeHashKey(hashKey, rows, cols, winLen, scoreX, scoreO);
            TTEntry entry;
            entry.score = bestScore;
            entry.depth = depth;
            entry.flag  = TTEntry::Flag::Exact;
            
            if (bestScore <= alphaOriginal) entry.flag = TTEntry::Flag::Upper;
            else if (bestScore >= betaOriginal) entry.flag = TTEntry::Flag::Lower;
            
            if (bestMoveCoord.row() >= 0 && bestMoveCoord.col() >= 0) {
                entry.bestMove = MoveEvaluation(bestMoveCoord, bestScore);
            }
            entry.generation = ttGeneration_;

            if (!transpositionTable_.contains(key)) {
                transpositionTable_.insert(key, entry);
            } else {
                const TTEntry& oldEntry = transpositionTable_.get(key);
                if (entry.depth >= oldEntry.depth) {
                    transpositionTable_.insert(key, entry);
                }
            }
        }
        return bestScore;
    }

    bool timeExceeded() const {
        if (timeLimitMs_ <= 0) return false;
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime_).count();
        return elapsed >= timeLimitMs_;
    }

    bool isCancelled() const {
        bool external = cancelFlag_ && cancelFlag_->load(std::memory_order_relaxed);
        return external || timeExceeded();
    }

    void updateBestSoFar(const MoveEvaluation& candidate) {
        if (!bestSoFarPtr_) return;
        if (candidate.move.row() < 0 || candidate.move.col() < 0) return;
        if (candidate.score > bestSoFarPtr_->score) {
            *bestSoFarPtr_ = candidate;
        }
    }

public:
    MinimaxAI(Player player, int maxDepth = 9, bool useMemoization = true, GameMode mode = GameMode::Classic, std::atomic<bool>* cancelFlag = nullptr, MoveEvaluation* bestSoFar = nullptr, DynamicArray<int>* historyTable = nullptr)
        : player_(player),
        opponent_(getOpponent(player)),
        maxDepth_(maxDepth),
        useMemoization_(useMemoization),
        mode_(mode),
        cancelFlag_(cancelFlag),
        bestSoFarPtr_(bestSoFar),
        historyTable_(historyTable)
    {}

    MoveEvaluation findBestMove(Board& board) {
        stats_.reset();
        startTime_ = std::chrono::steady_clock::now();
        ++ttGeneration_;
        int rows = board.getRows();
        int cols = board.getCols();
        int winLen = board.getWinLength();
        size_t maxEntries = static_cast<size_t>(rows * cols) * 5000;
        if (maxEntries < 200000) maxEntries = 200000;
        if (maxEntries > 2000000) maxEntries = 2000000;
        if (transpositionTable_.size() > maxEntries) {
            transpositionTable_.clear();
            ttGeneration_ = 1;
        }
        int totalCells = rows * cols;

        int searchDepth = maxDepth_;
        if (mode_ == GameMode::Classic && rows == 3 && cols == 3 && perfectClassic3_) {
            int remaining = static_cast<int>(board.getEmptyCells().size());
            if (remaining > searchDepth) searchDepth = remaining;
        }

        DynamicArray<Coord> moves = getSearchMoves(board, searchDepth);
        if (moves.size() == static_cast<size_t>(totalCells)) {
            int centerRow = rows / 2;
            int centerCol = cols / 2;
            bool hasCenter = (rows % 2 == 1) && (cols % 2 == 1);

            if (mode_ == GameMode::LinesScore &&
                player_ == Player::X &&
                hasCenter)
            {
                DynamicArray<Coord> filtered;
                filtered.reserve(moves.size());
                Coord bestMove(-1, -1);
                int bestScore = std::numeric_limits<int>::min();
                for (size_t i = 0; i < moves.size(); ++i) {
                    if (moves[i].row() == centerRow &&
                        moves[i].col() == centerCol) {
                        continue;
                    }
                    filtered.push_back(moves[i]);
                    int dist = std::abs(moves[i].row() - centerRow)
                               + std::abs(moves[i].col() - centerCol);
                    int nearScore = 0;
                    for (int dr = -1; dr <= 1; ++dr) {
                        for (int dc = -1; dc <= 1; ++dc) {
                            if (dr == 0 && dc == 0) continue;
                            int rr = moves[i].row() + dr;
                            int cc = moves[i].col() + dc;
                            if (rr >= 0 && rr < rows && cc >= 0 && cc < cols) {
                                if (board.getNoCheck(rr, cc) != CellState::Empty) {
                                    nearScore += 2;
                                }
                            }
                        }
                    }
                    int score = -dist * 3 + nearScore;
                    if (score > bestScore) {
                        bestScore = score;
                        bestMove = moves[i];
                    }
                }
                if (allowOpeningShortcut_ && bestMove.row() >= 0) {
                    stats_.timeMs = 0;
                    return MoveEvaluation(bestMove, 0);
                }
                if (!filtered.empty()) {
                    moves = std::move(filtered);
                }
            }

            if (mode_ == GameMode::Classic && rows == 3 && cols == 3) {
                stats_.timeMs = 0;
                return MoveEvaluation(Coord(centerRow, centerCol), 0);
            }
        }
        if (moves.empty()) {
            return MoveEvaluation();
        }

        DynamicArray<int> history;
        history.reserve(static_cast<size_t>(rows * cols));
        for (int i = 0; i < rows * cols; ++i) history.push_back(0);
        historyTable_ = &history;
        for (int d = 0; d < MAX_KILLER_DEPTH; ++d) {
            killerMoves_[d][0] = Coord(-1, -1);
            killerMoves_[d][1] = Coord(-1, -1);
        }
        initZobrist(rows, cols);
        uint64_t baseHash = computeZobrist(board, player_);
        initEvalCache(board);
        int baseScoreX = creditedX_;
        int baseScoreO = creditedO_;

        MoveEvaluation bestMove;
        bestMove.score = std::numeric_limits<int>::min();
        CellState playerCell = playerToCell(player_);
        if (bestSoFarPtr_) {
            bestSoFarPtr_->score = bestMove.score;
            bestSoFarPtr_->move = Coord(-1, -1);
        }

        auto moveScore = [&](const Coord& mv) {
            int centerRow = rows / 2;
            int centerCol = cols / 2;
            int dist = std::abs(mv.row() - centerRow) + std::abs(mv.col() - centerCol);
            int nearScore = 0;
            for (int dr = -1; dr <= 1; ++dr) {
                for (int dc = -1; dc <= 1; ++dc) {
                    if (dr == 0 && dc == 0) continue;
                    int rr = mv.row() + dr;
                    int cc = mv.col() + dc;
                    if (rr >= 0 && rr < rows && cc >= 0 && cc < cols) {
                        if (board.getNoCheck(rr, cc) != CellState::Empty) {
                            nearScore += 2;
                        }
                    }
                }
            }
            int hist = 0;
            int idx = mv.row() * cols + mv.col();
            if (idx >= 0 && idx < static_cast<int>(history.size())) hist = history[idx];
            return -dist * 3 + nearScore + hist;
        };

        auto orderMoves = [&](DynamicArray<Coord>& list, const MoveEvaluation& pv) {
            std::sort(list.begin(), list.end(), [&](const Coord& a, const Coord& b) {int scoreA = moveScore(a); int scoreB = moveScore(b); if (pv.move == a) scoreA += 10000; if (pv.move == b) scoreB += 10000; return scoreA > scoreB; });
        };

        MoveEvaluation principal;
        principal.score = std::numeric_limits<int>::min();
        unsigned int hw = std::thread::hardware_concurrency();
        // HOTFIX: keep LinesScore single-threaded to avoid rare crashes in AI vs AI.
        bool enableParallel = (mode_ == GameMode::Classic) &&
                              (moves.size() > 2 && searchDepth >= 4 && hw > 1);
        unsigned int threadBudget = hw == 0 ? 2 : std::min<unsigned int>(hw, 4);

        for (int depth = 1; depth <= searchDepth; ++depth) {
            orderMoves(moves, principal);
            int extensionBudget = (useExtensions_ && mode_ == GameMode::LinesScore)
                ? std::min(1, depth / 2)
                : 0;

            bool redoFullWindow = false;
            int baseWindow = 200 + winLen * 40;
            if (principal.score != std::numeric_limits<int>::min()) {
                int64_t absScore = principal.score < 0 ? -(int64_t)principal.score : (int64_t)principal.score;
                int scaled = static_cast<int>(std::min<int64_t>(absScore / 15, std::numeric_limits<int>::max()));
                if (scaled > baseWindow) baseWindow = scaled;
            }
            int fullAlpha = std::numeric_limits<int>::min();
            int fullBeta  = std::numeric_limits<int>::max();
            int aspAlpha = fullAlpha;
            int aspBeta  = fullBeta;
            if (principal.score != std::numeric_limits<int>::min()) {
                aspAlpha = principal.score - baseWindow;
                aspBeta  = principal.score + baseWindow;
            }

            do {
                redoFullWindow = false;
                int alpha = aspAlpha;
                int beta  = aspBeta;

                MoveEvaluation bestAtDepth;
                bestAtDepth.score = std::numeric_limits<int>::min();

                if (enableParallel && depth == searchDepth && moves.size() > 1) {
                    class TaskResult {
                    public:
                        Coord mv;
                        int score;
                        AIStatistics stats;
                    };
                    Board rootSnapshot = board;
                    size_t idxMove = 0;
                    while (idxMove < moves.size() && !isCancelled()) {
                        DynamicArray<std::future<TaskResult>> futures;
                        futures.reserve(threadBudget);
                        for (unsigned int t = 0; t < threadBudget && idxMove < moves.size(); ++t, ++idxMove) {
                            Coord mv = moves[idxMove];
                            futures.push_back(std::async(std::launch::async, [=]() mutable {
                                Board localBoard = rootSnapshot;
                                MinimaxAI worker(player_, searchDepth, useMemoization_, mode_, cancelFlag_, nullptr, nullptr);
                                worker.timeLimitMs_ = timeLimitMs_;
                                worker.startTime_ = startTime_;
                                worker.moveGenMode_ = moveGenMode_;
                                worker.useLMR_ = useLMR_;
                                worker.useExtensions_ = useExtensions_;
                                worker.perfectClassic3_ = perfectClassic3_;
                                for (int d = 0; d < MAX_KILLER_DEPTH; ++d) {
                                    worker.killerMoves_[d][0] = Coord(-1, -1);
                                    worker.killerMoves_[d][1] = Coord(-1, -1);
                                }
                                worker.initZobrist(rows, cols);
                                worker.initEvalCache(localBoard);
                                DynamicArray<int> localHistory;
                                localHistory.reserve(static_cast<size_t>(rows * cols));
                                for (int j = 0; j < rows * cols; ++j) localHistory.push_back(0);
                                worker.historyTable_ = &localHistory;

                                int gained = worker.applyMoveEval(localBoard, mv, playerCell);
                                uint64_t childHash = baseHash;
                                childHash = worker.toggleToMove(childHash, player_);
                                childHash = worker.togglePiece(childHash, mv.row(), mv.col(), cols, playerCell);
                                childHash = worker.toggleToMove(childHash, worker.opponent_);

                                int nextScoreX = baseScoreX;
                                int nextScoreO = baseScoreO;
                                if (mode_ == GameMode::LinesScore) {
                                    int delta = std::min(2, std::max(0, gained));
                                    if (playerCell == CellState::X) nextScoreX += delta;
                                    else nextScoreO += delta;
                                }
                                int sc = worker.minimax(localBoard, depth - 1, alpha, beta,
                                                        worker.opponent_, false, childHash, mv, nextScoreX, nextScoreO, extensionBudget);
                                TaskResult r{ mv, sc, worker.stats_ };
                                return r;
                            }));
                        }
                        bool cutoff = false;
                        for (auto& f : futures) {
                            TaskResult r = f.get();
                            if (cutoff || isCancelled()) {
                                continue;
                            }
                            stats_.nodesVisited  += r.stats.nodesVisited;
                            stats_.nodesGenerated += r.stats.nodesGenerated;
                            stats_.cacheHits     += r.stats.cacheHits;
                            stats_.cacheMisses   += r.stats.cacheMisses;
                            if (r.score > bestAtDepth.score) {
                                bestAtDepth.score = r.score;
                                bestAtDepth.move  = r.mv;
                                principal = bestAtDepth;
                                updateBestSoFar(bestAtDepth);
                            }
                            int hidx = r.mv.row() * cols + r.mv.col();
                            if (hidx >= 0 && hidx < static_cast<int>(history.size())) {
                                history[hidx] += depth * depth;
                            }
                            alpha = std::max(alpha, r.score);
                            if (alpha >= beta) {
                                cutoff = true;
                            }
                        }
                        if (alpha >= beta || isCancelled()) break;
                    }
                } else {
                for (size_t i = 0; i < moves.size(); ++i) {
                    int gained = applyMoveEval(board, moves[i], playerCell);

                    uint64_t childHash = baseHash;
                    childHash = toggleToMove(childHash, player_);
                    childHash = togglePiece(childHash, moves[i].row(), moves[i].col(), cols, playerCell);
                    childHash = toggleToMove(childHash, opponent_);

                        int nextScoreX = creditedX_;
                        int nextScoreO = creditedO_;
                        if (mode_ == GameMode::LinesScore) {
                            int delta = std::min(2, std::max(0, gained));
                            if (playerCell == CellState::X) nextScoreX += delta;
                            else nextScoreO += delta;
                        }

                        int score = minimax(board, depth - 1, alpha, beta,
                                            opponent_, false, childHash, moves[i], nextScoreX, nextScoreO, extensionBudget);

                        undoMoveEval(board, moves[i], playerCell);

                        if (score > bestAtDepth.score) {
                            bestAtDepth.score = score;
                            bestAtDepth.move  = moves[i];
                            principal = bestAtDepth;
                            updateBestSoFar(bestAtDepth);
                        }

                        int idx = moves[i].row() * cols + moves[i].col();
                        if (idx >= 0 && idx < static_cast<int>(history.size())) {
                            history[idx] += depth * depth;
                        }

                        alpha = std::max(alpha, score);

                        if (isCancelled()) {
                            break;
                        }
                    }
                }

                if ((bestAtDepth.score <= aspAlpha || bestAtDepth.score >= aspBeta) &&
                    (aspAlpha != fullAlpha || aspBeta != fullBeta)) {
                    
                    aspAlpha = fullAlpha;
                    aspBeta  = fullBeta;
                    redoFullWindow = true;
                }

                if (!isCancelled()) {
                    bestMove = bestAtDepth;
                }

                if (isCancelled()) {
                    break;
                }
            } while (redoFullWindow);

            if (isCancelled()) {
                break;
            }

            
            stats_.completedDepth = depth;
        }

        auto endTime = std::chrono::steady_clock::now();
        stats_.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                            endTime - startTime_).count();
        historyTable_ = nullptr;
        return bestMove;
    }

    const AIStatistics& getStatistics() const {
        return stats_;
    }

    void clearCache() {
        transpositionTable_.clear();
    }

    void setUseMemoization(bool use) {
        useMemoization_ = use;
    }

    void setMaxDepth(int d) { maxDepth_ = d; }
    void setCancelFlag(std::atomic<bool>* f) { cancelFlag_ = f; }
    void setBestSoFar(MoveEvaluation* p) { bestSoFarPtr_ = p; }
    void setPlayer(Player p) {
        if (player_ == p) return;
        player_ = p;
        opponent_ = getOpponent(p);
        clearCache();
    }
    void setMode(GameMode m) { mode_ = m; }
    void setCredits(int cx, int co) { creditedX_ = cx; creditedO_ = co; }
    void setTimeLimitMs(int ms) { timeLimitMs_ = ms; }
    void setMoveGenMode(MoveGenMode m) { moveGenMode_ = m; }
    void setUseLMR(bool v) { useLMR_ = v; }
    void setUseExtensions(bool v) { useExtensions_ = v; }
    void setPerfectClassic3(bool v) { perfectClassic3_ = v; }
    void setAllowOpeningShortcut(bool v) { allowOpeningShortcut_ = v; }
};



class SearchParams {
public:
    SearchParams(int maxDepth = 9, bool useMemoization = true, int timeLimitMs = -1)
        : maxDepth_(maxDepth), useMemoization_(useMemoization), timeLimitMs_(timeLimitMs) {}

    int maxDepth() const { return maxDepth_; }
    void setMaxDepth(int d) { maxDepth_ = d; }

    bool useMemoization() const { return useMemoization_; }
    void setUseMemoization(bool v) { useMemoization_ = v; }
    MoveGenMode moveGenMode() const { return moveGenMode_; }
    void setMoveGenMode(MoveGenMode m) { moveGenMode_ = m; }
    bool useLMR() const { return useLMR_; }
    void setUseLMR(bool v) { useLMR_ = v; }
    bool useExtensions() const { return useExtensions_; }
    void setUseExtensions(bool v) { useExtensions_ = v; }
    bool perfectClassic3() const { return perfectClassic3_; }
    void setPerfectClassic3(bool v) { perfectClassic3_ = v; }
    int timeLimitMs() const { return timeLimitMs_; }
    void setTimeLimitMs(int ms) { timeLimitMs_ = ms; }

private:
    int maxDepth_;
    bool useMemoization_;
    MoveGenMode moveGenMode_ = MoveGenMode::Hybrid;
    bool useLMR_ = true;
    bool useExtensions_ = true;
    bool perfectClassic3_ = false;
    int timeLimitMs_ = -1;
};

class AnalysisResult {
public:
    Move        bestMove;
    int         bestScore;
    SearchStats stats;
    DynamicArray<std::pair<Move,int>> topMoves;
};

inline AnalysisResult analysePosition(const Board& b, Player toMove, GameMode mode, const SearchParams& params, int scoreX, int scoreO, std::atomic<bool>* cancelFlag)
{
    Board boardCopy = b; 
    MinimaxAI ai(toMove, params.maxDepth(), params.useMemoization(), mode, cancelFlag, nullptr, nullptr);
    ai.setMoveGenMode(params.moveGenMode());
    ai.setUseLMR(params.useLMR());
    ai.setUseExtensions(params.useExtensions());
    ai.setPerfectClassic3(params.perfectClassic3());
    ai.setTimeLimitMs(params.timeLimitMs());
    ai.initZobrist(boardCopy.getRows(), boardCopy.getCols());
    uint64_t baseHash = ai.computeZobrist(boardCopy, toMove);
    ai.initEvalCache(boardCopy);
    ai.startTime_ = std::chrono::steady_clock::now();

    AnalysisResult result;
    result.bestMove  = Move(-1, -1);
    result.bestScore = std::numeric_limits<int>::min();
    result.topMoves.clear();

    DynamicArray<Coord> moves = ai.getSearchMoves(boardCopy, params.maxDepth());
    int rows = boardCopy.getRows();
    int cols = boardCopy.getCols();
    bool emptyBoard = true;
    for (int row = 0; row < rows && emptyBoard; ++row) {
        for (int col = 0; col < cols; ++col) {
            if (boardCopy.getNoCheck(row, col) != CellState::Empty) {
                emptyBoard = false;
                break;
            }
        }
    }
    bool hasCenter = (rows % 2 == 1) && (cols % 2 == 1);
    if (mode == GameMode::LinesScore && toMove == Player::X && emptyBoard && hasCenter) {
        int cr = rows / 2;
        int cc = cols / 2;
        DynamicArray<Coord> filtered;
        filtered.reserve(moves.size());
        for (const auto& m : moves) {
            if (m.row() == cr && m.col() == cc) continue;
            filtered.push_back(m);
        }
        moves = std::move(filtered);
    }
    if (moves.empty()) {
        result.bestScore = 0;
        result.stats     = ai.getStatistics();
        return result;
    }

    CellState playerCell = (toMove == Player::X)
                               ? CellState::X
                               : CellState::O;

    ai.stats_.reset();
    auto startTime = std::chrono::high_resolution_clock::now();

    int extensionBudget = (params.useExtensions() && mode == GameMode::LinesScore)
        ? std::min(1, params.maxDepth() / 2)
        : 0;

    for (size_t i = 0; i < moves.size(); ++i) {
        Coord mv = moves[i];

        int gained = ai.applyMoveEval(boardCopy, mv, playerCell);
        uint64_t childHash = baseHash;
        childHash = ai.toggleToMove(childHash, toMove);
        childHash = ai.togglePiece(childHash, mv.row(), mv.col(), boardCopy.getCols(), playerCell);
        childHash = ai.toggleToMove(childHash, ai.opponent_);
        int localAlpha = std::numeric_limits<int>::min();
        int localBeta  = std::numeric_limits<int>::max();
        int nextScoreX = scoreX;
        int nextScoreO = scoreO;
        if (mode == GameMode::LinesScore) {
            int delta  = std::min(2, std::max(0, gained));
            if (playerCell == CellState::X) nextScoreX += delta;
            else                            nextScoreO += delta;
        }
        int score = ai.minimax(boardCopy,
                               params.maxDepth() - 1,
                               localAlpha,
                               localBeta,
                               ai.opponent_,
                               false,
                               childHash,
                               mv,
                               nextScoreX,
                               nextScoreO,
                               extensionBudget);
        ai.undoMoveEval(boardCopy, mv, playerCell);

        result.topMoves.push_back(std::pair<Move,int>(mv, score));

        if (score > result.bestScore) {
            result.bestScore = score;
            result.bestMove  = mv;
        }

        if (ai.isCancelled()) {
            break;
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    ai.stats_.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                           endTime - startTime).count();
    bool cancelled = ai.isCancelled();
    ai.stats_.completedDepth = cancelled ? 0 : params.maxDepth();

    
    std::sort(result.topMoves.begin(), result.topMoves.end(), [](const std::pair<Move,int>& a, const std::pair<Move,int>& b) {return a.second > b.second; });

    result.stats = ai.stats_;

    return result;
}
