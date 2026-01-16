
#pragma once
#include "Board.hpp"
#include "HashMap.hpp"
#include "DynamicArray.hpp"
#include <limits>
#include <chrono>
#include <iostream>
#include <algorithm>
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
    static constexpr int MAX_KILLER_DEPTH = 64;
    Coord killerMoves_[MAX_KILLER_DEPTH][2]{};
    DynamicArray<uint64_t> zobristTable_;
    uint64_t zobristPlayerX_ = 0;
    uint64_t zobristPlayerO_ = 0;
    int timeLimitMs_ = -1;
    std::chrono::steady_clock::time_point startTime_;

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

    
    static int countLinesFor(const Board& b, CellState who) {
        if (who == CellState::Empty) return 0;

        int rows     = b.getRows();
        int cols     = b.getCols();
        int winLen   = b.getWinLength();
        int count    = 0;

        
        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col <= cols - winLen; ++col) {
                bool ok = true;
                for (int k = 0; k < winLen; ++k) {
                    if (b.get(row, col + k) != who) {
                        ok = false;
                        break;
                    }
                }
                if (ok) ++count;
            }
        }

        
        for (int col = 0; col < cols; ++col) {
            for (int row = 0; row <= rows - winLen; ++row) {
                bool ok = true;
                for (int k = 0; k < winLen; ++k) {
                    if (b.get(row + k, col) != who) {
                        ok = false;
                        break;
                    }
                }
                if (ok) ++count;
            }
        }

        
        for (int row = 0; row <= rows - winLen; ++row) {
            for (int col = 0; col <= cols - winLen; ++col) {
                bool ok = true;
                for (int k = 0; k < winLen; ++k) {
                    if (b.get(row + k, col + k) != who) {
                        ok = false;
                        break;
                    }
                }
                if (ok) ++count;
            }
        }

        
        for (int row = 0; row <= rows - winLen; ++row) {
            for (int col = winLen - 1; col < cols; ++col) {
                bool ok = true;
                for (int k = 0; k < winLen; ++k) {
                    if (b.get(row + k, col - k) != who) {
                        ok = false;
                        break;
                    }
                }
                if (ok) ++count;
            }
        }

        return count;
    }

    
    static int potentialLinesFor(const Board& b, CellState who) {
        if (who == CellState::Empty) return 0;
        CellState opp = (who == CellState::X) ? CellState::O : CellState::X;

        int rows   = b.getRows();
        int cols   = b.getCols();
        int winLen = b.getWinLength();
        int score  = 0;

        auto addWindow = [&](int r, int c, int dr, int dc) {
            int own = 0;
            for (int k = 0; k < winLen; ++k) {
                CellState cell = b.get(r + k * dr, c + k * dc);
                if (cell == opp) return;
                if (cell == who) ++own;
            }
            score += own * own;
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
        return score;
    }

    
    static int openLineScore(const Board& b, CellState who) {
        if (who == CellState::Empty) return 0;
        CellState opp = (who == CellState::X) ? CellState::O : CellState::X;
        int rows   = b.getRows();
        int cols   = b.getCols();
        int winLen = b.getWinLength();
        int score  = 0;

        auto addWindow = [&](int r, int c, int dr, int dc) {
            int own = 0;
            for (int k = 0; k < winLen; ++k) {
                CellState cell = b.get(r + k * dr, c + k * dc);
                if (cell == opp) return; 
                if (cell == who) ++own;
            }

            int openEnds = 0;
            int br = r - dr, bc = c - dc;
            int ar = r + winLen * dr, ac = c + winLen * dc;
            if (br >= 0 && br < rows && bc >= 0 && bc < cols && b.isEmpty(br, bc)) ++openEnds;
            if (ar >= 0 && ar < rows && ac >= 0 && ac < cols && b.isEmpty(ar, ac)) ++openEnds;

            int base = own * own;
            if (own == winLen - 1 && openEnds > 0) {
                base += (openEnds == 2 ? 200 : 80); 
            }
            int mult = (openEnds == 2) ? 8 : (openEnds == 1 ? 4 : 1);
            score += base * mult;
        };

        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col <= cols - winLen; ++col) addWindow(row, col, 0, 1);
        }
        for (int col = 0; col < cols; ++col) {
            for (int row = 0; row <= rows - winLen; ++row) addWindow(row, col, 1, 0);
        }
        for (int row = 0; row <= rows - winLen; ++row) {
            for (int col = 0; col <= cols - winLen; ++col) addWindow(row, col, 1, 1);
        }
        for (int row = 0; row <= rows - winLen; ++row) {
            for (int col = winLen - 1; col < cols; ++col) addWindow(row, col, 1, -1);
        }
        return score;
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

    int baseScorePerspective(int scoreX, int scoreO) const {
        return (player_ == Player::X) ? (scoreX - scoreO) : (scoreO - scoreX);
    }

    
    int evaluate(const Board& board, int scoreX = 0, int scoreO = 0) const {
        CellState playerCell   = playerToCell(player_);
        CellState opponentCell = playerToCell(opponent_);

        int rows    = board.getRows();
        int cols    = board.getCols();
        int minSide = std::min(rows, cols);
        int winLen  = board.getWinLength();
        (void)winLen;

        int centerRow = rows / 2;
        int centerCol = cols / 2;

        
        if (mode_ == GameMode::Classic) {
            if (board.checkWin(playerCell)) {
                return 10000;
            }
            if (board.checkWin(opponentCell)) {
                return -10000;
            }

            int score = 0;
            int lineWeight = 80 * winLen;
            int potWeight  = 6 * winLen;
            int openWeight = 5 * winLen;

            
            int linesPlayer = countLinesFor(board, playerCell);
            int linesOpp    = countLinesFor(board, opponentCell);
            score += (linesPlayer - linesOpp) * lineWeight;
            int potPlayer = potentialLinesFor(board, playerCell);
            int potOpp    = potentialLinesFor(board, opponentCell);
            score += (potPlayer - potOpp) * potWeight;
            int openP = openLineScore(board, playerCell);
            int openO = openLineScore(board, opponentCell);
            score += (openP - openO) * openWeight;

            
            for (int row = 0; row < rows; ++row) {
                for (int col = 0; col < cols; ++col) {
                    CellState cell = board.get(row, col);
                    if (cell == CellState::Empty) continue;

                    int centerDist =
                        std::abs(row - centerRow) + std::abs(col - centerCol);
                    int posValue = (minSide - centerDist);

                    if (cell == playerCell) {
                        score += posValue;
                    } else if (cell == opponentCell) {
                        score -= posValue;
                    }
                }
            }

            return score;
        }

        
        int score = baseScorePerspective(scoreX, scoreO) * 1000;
        int lineWeight = 40 * winLen;
        int potWeight  = 4 * winLen;
        int openWeight = 3 * winLen;

        int linesPlayer = countLinesFor(board, playerCell);
        int linesOpp    = countLinesFor(board, opponentCell);

        
        int lineDiff = linesPlayer - linesOpp;
        score += lineDiff * lineWeight;   
        int potPlayer = potentialLinesFor(board, playerCell);
        int potOpp    = potentialLinesFor(board, opponentCell);
        score += (potPlayer - potOpp) * potWeight;
        int openP = openLineScore(board, playerCell);
        int openO = openLineScore(board, opponentCell);
        score += (openP - openO) * openWeight;

        
        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col < cols; ++col) {
                CellState cell = board.get(row, col);
                if (cell == CellState::Empty) continue;

                int centerDist =
                    std::abs(row - centerRow) + std::abs(col - centerCol);
                int posValue = (minSide - centerDist);

                if (cell == playerCell) {
                    score += posValue;
                } else if (cell == opponentCell) {
                    score -= posValue;
                }
            }
        }

        return score;
    }

    int minimax(Board& board, int depth, int alpha, int beta, Player currentPlayer, bool isMaximizing, uint64_t hashKey, const Coord& lastMove, int scoreX, int scoreO) {

        stats_.nodesVisited++;

        if (isCancelled()) {
            return evaluate(board, scoreX, scoreO);
        }

        int alphaOriginal = alpha;
        int betaOriginal  = beta;
        int rows = board.getRows();
        int cols = board.getCols();
        int winLen = board.getWinLength();
        CellState playerCell   = playerToCell(player_);
        CellState opponentCell = playerToCell(opponent_);

        
        if (mode_ == GameMode::Classic) {
            
            if (lastMove.row() >= 0) {
                CellState lastCell = playerToCell(getOpponent(currentPlayer));
                if (board.checkWinFromMove(lastMove, lastCell)) {
                    if (lastCell == playerCell) return 10000 + depth;
                    else return -10000 - depth;
                }
            }
            if (board.isFull() || depth <= 0) {
                
                return evaluate(board);
            }
        } else { 
            if (board.isFull()) {
                
                return baseScorePerspective(scoreX, scoreO) * 1000;
            }
            
            if (depth <= 0) {
                return evaluate(board, scoreX, scoreO);
            }
        }

        
        if (useMemoization_) {
            size_t key = makeHashKey(hashKey, rows, cols, winLen, scoreX, scoreO);
            if (transpositionTable_.contains(key)) {
                const TTEntry& e = transpositionTable_.get(key);
                if (e.generation == ttGeneration_ && e.depth >= depth) {
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

        DynamicArray<Coord> moves = board.getCandidateMoves(1);
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

        auto moveUrgency = [&](const Coord& mv) -> int {
            int bonus = 0;
            Board& b = board;
            CellState curCell = playerToCell(currentPlayer);
            b.set(mv, curCell);
            if (b.checkWinFromMove(mv, curCell)) bonus += 5000;
            b.set(mv, CellState::Empty);

            CellState oppCell = playerToCell(getOpponent(currentPlayer));
            b.set(mv, oppCell);
            if (b.checkWinFromMove(mv, oppCell)) bonus += 3000;
            b.set(mv, CellState::Empty);

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
        for (size_t i = 0; i < moves.size(); ++i) {
            moveList.push_back(std::pair<Coord,int>(moves[i], moveUrgency(moves[i])));
        }
        std::sort(moveList.begin(), moveList.end(), [](const std::pair<Coord,int>& a, const std::pair<Coord,int>& b) {return a.second > b.second; });
        moves.clear();
        DynamicArray<int> urgencies;
        urgencies.reserve(moveList.size());
        for (const auto& p : moveList) {
            moves.push_back(p.first);
            urgencies.push_back(p.second);
        }

        int       bestScore;
        CellState currentCell = playerToCell(currentPlayer);
        Coord bestMoveCoord(-1, -1);

        if (isMaximizing) {
            bestScore = std::numeric_limits<int>::min();

            for (size_t i = 0; i < moves.size(); ++i) {
                board.set(moves[i], currentCell);
                int nextDepth = depth - 1;
                int urgency = (i < urgencies.size()) ? urgencies[i] : moveUrgency(moves[i]);
                
                if (depth > 3 && i > 4 && urgency < 4000) {
                    nextDepth = depth - 2;
                    if (depth > 6 && i > 8 && urgency < 2000) {
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
                    int gained = board.countLinesFromMove(moves[i], currentCell);
                    int delta = std::min(2, std::max(0, gained));
                    if (currentCell == CellState::X) nextScoreX += delta;
                    else nextScoreO += delta;
                }

                int score = minimax(board, nextDepth, alpha, beta,
                                    getOpponent(currentPlayer), false, childHash, moves[i], nextScoreX, nextScoreO);
                board.set(moves[i], CellState::Empty);

                bestScore = std::max(bestScore, score);
                alpha     = std::max(alpha, bestScore);
                if (score == bestScore) {
                    bestMoveCoord = moves[i];
                }

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

            for (size_t i = 0; i < moves.size(); ++i) {
                board.set(moves[i], currentCell);
                int nextDepth = depth - 1;
                int urgency = (i < urgencies.size()) ? urgencies[i] : moveUrgency(moves[i]);
                if (depth > 3 && i > 4 && urgency < 4000) {
                    nextDepth = depth - 2;
                    if (depth > 6 && i > 8 && urgency < 2000) {
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
                    int gained = board.countLinesFromMove(moves[i], currentCell);
                    int delta = std::min(2, std::max(0, gained));
                    if (currentCell == CellState::X) nextScoreX += delta;
                    else nextScoreO += delta;
                }

                int score = minimax(board, nextDepth, alpha, beta,
                                    getOpponent(currentPlayer), true, childHash, moves[i], nextScoreX, nextScoreO);
                board.set(moves[i], CellState::Empty);

                bestScore = std::min(bestScore, score);
                beta      = std::min(beta, bestScore);
                if (score == bestScore) {
                    bestMoveCoord = moves[i];
                }

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

            if (!transpositionTable_.contains(key) ||
                transpositionTable_.get(key).depth <= entry.depth ||
                transpositionTable_.get(key).generation != ttGeneration_)
            {
                transpositionTable_.insert(key, entry);
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

    friend AnalysisResult analysePosition(const Board& b, Player toMove, GameMode mode, const SearchParams& params, int scoreX, int scoreO);

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
        if (transpositionTable_.size() > 500000) {
            transpositionTable_.clear();
            ttGeneration_ = 1;
        }

        int rows = board.getRows();
        int cols = board.getCols();
        int totalCells = rows * cols;

        DynamicArray<Coord> moves = board.getCandidateMoves(2);
        if (moves.size() == static_cast<size_t>(totalCells)) {
            int centerRow = rows / 2;
            int centerCol = cols / 2;
            bool hasCenter = (rows % 2 == 1) && (cols % 2 == 1);

            
            if (mode_ == GameMode::LinesScore &&
                player_ == Player::X &&
                hasCenter)
            {
                for (size_t i = 0; i < moves.size(); ++i) {
                    if (!(moves[i].row() == centerRow &&
                          moves[i].col() == centerCol))
                    {
                        stats_.timeMs = 0;
                        return MoveEvaluation(moves[i], 0);
                    }
                }
            }

            
            stats_.timeMs = 0;
            return MoveEvaluation(Coord(centerRow, centerCol), 0);
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
                        if (!board.isEmpty(rr, cc)) {
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
        bool enableParallel = (moves.size() > 2 && maxDepth_ >= 4 && hw > 1);
        unsigned int threadBudget = hw == 0 ? 2 : std::min<unsigned int>(hw, 4);

        for (int depth = 1; depth <= maxDepth_; ++depth) {
            orderMoves(moves, principal);

            bool redoFullWindow = false;
            int baseWindow = 120;
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

                if (enableParallel && depth == maxDepth_ && moves.size() > 1) {
                    class TaskResult {
                    public:
                        Coord mv;
                        int score;
                        AIStatistics stats;
                    };
                    size_t idxMove = 0;
                    while (idxMove < moves.size() && !isCancelled()) {
                        DynamicArray<std::future<TaskResult>> futures;
                        futures.reserve(threadBudget);
                        for (unsigned int t = 0; t < threadBudget && idxMove < moves.size(); ++t, ++idxMove) {
                            Coord mv = moves[idxMove];
                            futures.push_back(std::async(std::launch::async, [=, &board]() {Board localBoard = board; localBoard.set(mv, playerCell); MinimaxAI worker(player_, maxDepth_, useMemoization_, mode_, cancelFlag_, nullptr, nullptr); for (int d = 0; d < MAX_KILLER_DEPTH; ++d) {worker.killerMoves_[d][0] = Coord(-1, -1); worker.killerMoves_[d][1] = Coord(-1, -1); } worker.initZobrist(rows, cols); DynamicArray<int> localHistory; localHistory.reserve(static_cast<size_t>(rows * cols)); for (int j = 0; j < rows * cols; ++j) localHistory.push_back(0); worker.historyTable_ = &localHistory; uint64_t childHash = baseHash; childHash = worker.toggleToMove(childHash, player_); childHash = worker.togglePiece(childHash, mv.row(), mv.col(), cols, playerCell); childHash = worker.toggleToMove(childHash, opponent_); int nextScoreX = baseScoreX; int nextScoreO = baseScoreO; if (mode_ == GameMode::LinesScore) {int gained = localBoard.countLinesFromMove(mv, playerCell); int delta = std::min(2, std::max(0, gained)); if (playerCell == CellState::X) nextScoreX += delta; else nextScoreO += delta; } int sc = worker.minimax(localBoard, depth - 1, alpha, beta, opponent_, false, childHash, mv, nextScoreX, nextScoreO); TaskResult r{ mv, sc, worker.stats_ }; return r; }));
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
                    board.set(moves[i], playerCell);

                    uint64_t childHash = baseHash;
                    childHash = toggleToMove(childHash, player_);
                    childHash = togglePiece(childHash, moves[i].row(), moves[i].col(), cols, playerCell);
                    childHash = toggleToMove(childHash, opponent_);

                        int nextScoreX = creditedX_;
                        int nextScoreO = creditedO_;
                        if (mode_ == GameMode::LinesScore) {
                            int gained = board.countLinesFromMove(moves[i], playerCell);
                            int delta = std::min(2, std::max(0, gained));
                            if (playerCell == CellState::X) nextScoreX += delta;
                            else nextScoreO += delta;
                        }

                        int score = minimax(board, depth - 1, alpha, beta,
                                            opponent_, false, childHash, moves[i], nextScoreX, nextScoreO);

                        board.set(moves[i], CellState::Empty);

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
    void setMode(GameMode m) { mode_ = m; }
    void setCredits(int cx, int co) { creditedX_ = cx; creditedO_ = co; }
    void setTimeLimitMs(int ms) { timeLimitMs_ = ms; }
};



class SearchParams {
public:
    SearchParams(int maxDepth = 9, bool useMemoization = true)
        : maxDepth_(maxDepth), useMemoization_(useMemoization) {}

    int maxDepth() const { return maxDepth_; }
    void setMaxDepth(int d) { maxDepth_ = d; }

    bool useMemoization() const { return useMemoization_; }
    void setUseMemoization(bool v) { useMemoization_ = v; }

private:
    int maxDepth_;
    bool useMemoization_;
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
    ai.initZobrist(boardCopy.getRows(), boardCopy.getCols());
    uint64_t baseHash = ai.computeZobrist(boardCopy, toMove);

    AnalysisResult result;
    result.bestMove  = Move(-1, -1);
    result.bestScore = std::numeric_limits<int>::min();
    result.topMoves.clear();

    DynamicArray<Coord> moves = boardCopy.getCandidateMoves(1);
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

    for (size_t i = 0; i < moves.size(); ++i) {
        Coord mv = moves[i];

        boardCopy.set(mv, playerCell);
        uint64_t childHash = baseHash;
        childHash = ai.toggleToMove(childHash, toMove);
        childHash = ai.togglePiece(childHash, mv.row(), mv.col(), boardCopy.getCols(), playerCell);
        childHash = ai.toggleToMove(childHash, ai.opponent_);
        int localAlpha = std::numeric_limits<int>::min();
        int localBeta  = std::numeric_limits<int>::max();
        int nextScoreX = scoreX;
        int nextScoreO = scoreO;
        if (mode == GameMode::LinesScore) {
            int gained = boardCopy.countLinesFromMove(mv, playerCell);
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
                               nextScoreO);
        boardCopy.set(mv, CellState::Empty);

        result.topMoves.push_back(std::pair<Move,int>(mv, score));

        if (score > result.bestScore) {
            result.bestScore = score;
            result.bestMove  = mv;
        }

    }

    auto endTime = std::chrono::high_resolution_clock::now();
    ai.stats_.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                           endTime - startTime).count();
    ai.stats_.completedDepth = params.maxDepth();

    
    std::sort(result.topMoves.begin(), result.topMoves.end(), [](const std::pair<Move,int>& a, const std::pair<Move,int>& b) {return a.second > b.second; });

    result.stats = ai.stats_;

    return result;
}
