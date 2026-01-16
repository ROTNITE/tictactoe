#pragma once
#include "Board.hpp"
#include "MinimaxAI.hpp"
#include "DynamicArray.hpp"
#include <string>
#include <functional>
#include <fstream>
#include <atomic>
#include <algorithm>
#include <mutex>
#include <cmath>
#include <chrono>


enum class MoveStatus {
    Ok,
    InvalidCell,
    ForbiddenFirstCenter,
    GameAlreadyOver
};


class AIVsAIMoveInfo {
public:
    bool isSwap = false;
    Player player;
    Coord move;
    int evalScore;
    AIStatistics stats;
};

class AIVsAIResult {
public:
    GameMode mode;
    int finalLinesX;
    int finalLinesO;
    int finalScore; 
    bool xWins;
    bool oWins;
    bool draw;
    DynamicArray<AIVsAIMoveInfo> moves;
};

class GameController {
public:
    GameController(int rows      = 3, int cols      = 3, int winLength = 3, GameMode mode = GameMode::LinesScore, bool useSwapRule = false)
        : board_(rows, cols, winLength)
        , mode_(mode)
        , currentPlayer_(Player::X)
        , gameOver_(false)
        , moveNumber_(0)
        , creditedLinesX_(0)
        , creditedLinesO_(0)
        , prevLinesX_(0)
        , prevLinesO_(0)
        , swapRuleEnabled_(useSwapRule)
        , swapAvailable_(false)
        , swapUsed_(false)
        , aiX_(Player::X, 9, true, mode)
        , aiO_(Player::O, 9, true, mode)
    {
    }

    void setOnAIMoveCallback(const std::function<void(const Board&, Player, const MoveEvaluation&, const AIStatistics&)>& cb)
    {
        std::lock_guard<std::mutex> lk(cbMutex_);
        onAIMoveCallback_ = cb;
    }

    void newGame(int rows, int cols, int winLength, GameMode mode, bool useSwapRule = false) {
        board_ = Board(rows, cols, winLength);
        mode_ = mode;
        currentPlayer_ = Player::X;
        gameOver_ = false;
        moveNumber_ = 0;
        creditedLinesX_ = 0;
        creditedLinesO_ = 0;
        prevLinesX_ = 0;
        prevLinesO_ = 0;
        swapRuleEnabled_ = useSwapRule;
        swapAvailable_ = false;
        swapUsed_ = false;
        aiX_.setMode(mode_);
        aiO_.setMode(mode_);
        aiX_.clearCache();
        aiO_.clearCache();
    }

    const Board& board() const { return board_; }
    Board& board() { return board_; }

    GameMode mode() const { return mode_; }
    Player currentPlayer() const { return currentPlayer_; }
    bool isGameOver() const { return gameOver_; }
    int moveNumber() const { return moveNumber_; }

    bool isSwapRuleEnabled() const { return swapRuleEnabled_ && mode_ == GameMode::LinesScore; }
    bool isSwapAvailable() const {
        return isSwapRuleEnabled() && swapAvailable_ && !gameOver_ && moveNumber_ == 1;
    }
    bool swapUsed() const { return swapUsed_; }

    
    bool applySwapRule() {
        if (!isSwapAvailable()) return false;
        swapAvailable_ = false;
        swapUsed_ = true;
        
        
        return true;
    }

    
    bool maybeAutoSwapForCurrent(int depthHint, bool memoHint, int margin = 0, std::atomic<bool>* cancelFlag = nullptr) {
        if (!isSwapAvailable()) return false;

        
        SearchParams params;
        params.setMaxDepth(std::max(1, std::min(depthHint, 4)));
        params.setUseMemoization(memoHint);

        if (margin <= 0) {
            int cells = board_.getRows() * board_.getCols();
            margin = std::max(20, board_.getWinLength() * 10) + cells;
        }

        
        AnalysisResult stayEval = analysePosition(board_, currentPlayer_, mode_, params,
                                                  creditedLinesX_, creditedLinesO_,
                                                  cancelFlag);
        int stayScore = stayEval.bestScore;   
        int swapScore = -stayScore;           

        if (swapScore > stayScore + margin) {
            return applySwapRule();
        }
        return false;
    }

    int creditedLinesX() const { return creditedLinesX_; }
    int creditedLinesO() const { return creditedLinesO_; }

    
    int score() const { return creditedLinesO_ - creditedLinesX_; }

    
    MoveStatus applyMove(int row, int col) {
        if (gameOver_) {
            return MoveStatus::GameAlreadyOver;
        }

        int rows = board_.getRows();
        int cols = board_.getCols();
        if (row < 0 || row >= rows || col < 0 || col >= cols) {
            return MoveStatus::InvalidCell;
        }
        if (!board_.isEmpty(row, col)) {
            return MoveStatus::InvalidCell;
        }

        int centerRow = rows / 2;
        int centerCol = cols / 2;

        
        if (mode_ == GameMode::LinesScore &&
            !isSwapRuleEnabled() &&
            (rows % 2 == 1) &&
            (cols % 2 == 1) &&
            currentPlayer_ == Player::X &&
            moveNumber_ == 0 &&
            row == centerRow && col == centerCol)
        {
            return MoveStatus::ForbiddenFirstCenter;
        }

        CellState currentCell =
            (currentPlayer_ == Player::X) ? CellState::X : CellState::O;

        board_.set(row, col, currentCell);
        ++moveNumber_;

        if (isSwapRuleEnabled()) {
            if (moveNumber_ == 1) {
                swapAvailable_ = true;
                swapUsed_ = false;
            } else if (moveNumber_ > 1) {
                swapAvailable_ = false;
            }
        } else {
            swapAvailable_ = false;
        }

        if (mode_ == GameMode::LinesScore) {
            int newLines = board_.countLinesFromMove(Coord(row, col), currentCell);
            int delta = std::min(2, std::max(0, newLines));
            if (currentCell == CellState::X) {
                creditedLinesX_ += delta;
                prevLinesX_ += newLines;
            } else {
                creditedLinesO_ += delta;
                prevLinesO_ += newLines;
            }

            if (board_.isFull()) {
                gameOver_ = true;
            }
        } else { 
            if (board_.checkWin(currentCell)) {
                gameOver_ = true;
            } else if (board_.isFull()) {
                gameOver_ = true;
            }
        }

        if (!gameOver_) {
            currentPlayer_ = (currentPlayer_ == Player::X)
            ? Player::O : Player::X;
        }

        return MoveStatus::Ok;
    }

    
    static int countLinesFor(const Board& b, CellState player) {
        if (player == CellState::Empty) return 0;

        int rows      = b.getRows();
        int cols      = b.getCols();
        int winLength = b.getWinLength();
        int count     = 0;

        
        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col <= cols - winLength; ++col) {
                bool win = true;
                for (int k = 0; k < winLength; ++k) {
                    if (b.get(row, col + k) != player) {
                        win = false;
                        break;
                    }
                }
                if (win) ++count;
            }
        }

        
        for (int col = 0; col < cols; ++col) {
            for (int row = 0; row <= rows - winLength; ++row) {
                bool win = true;
                for (int k = 0; k < winLength; ++k) {
                    if (b.get(row + k, col) != player) {
                        win = false;
                        break;
                    }
                }
                if (win) ++count;
            }
        }

        
        for (int row = 0; row <= rows - winLength; ++row) {
            for (int col = 0; col <= cols - winLength; ++col) {
                bool win = true;
                for (int k = 0; k < winLength; ++k) {
                    if (b.get(row + k, col + k) != player) {
                        win = false;
                        break;
                    }
                }
                if (win) ++count;
            }
        }

        
        for (int row = 0; row <= rows - winLength; ++row) {
            for (int col = winLength - 1; col < cols; ++col) {
                bool win = true;
                for (int k = 0; k < winLength; ++k) {
                    if (b.get(row + k, col - k) != player) {
                        win = false;
                        break;
                    }
                }
                if (win) ++count;
            }
        }

        return count;
    }

    static int computeScoreLines(const Board& b) {
        int linesX = countLinesFor(b, CellState::X);
        int linesO = countLinesFor(b, CellState::O);
        return linesO - linesX;
    }

    
    MoveEvaluation findBestMove(Player aiPlayer, int depth, bool useMemoization, AIStatistics& outStats, MoveEvaluation* bestSoFar = nullptr, std::atomic<bool>* cancelFlag = nullptr, int timeLimitMs = -1)
    {
        MinimaxAI& ai = (aiPlayer == Player::X) ? aiX_ : aiO_;
        ai.setMaxDepth(depth);
        ai.setUseMemoization(useMemoization);
        ai.setCancelFlag(cancelFlag);
        ai.setBestSoFar(bestSoFar);
        ai.setCredits(creditedLinesX_, creditedLinesO_);
        ai.setTimeLimitMs(timeLimitMs);

        if (isSwapRuleEnabled() &&
            mode_ == GameMode::LinesScore &&
            moveNumber_ == 0 &&
            aiPlayer == Player::X)
        {
            return chooseSwapAwareFirstMove(depth, useMemoization, outStats, timeLimitMs, cancelFlag);
        }

        MoveEvaluation eval = ai.findBestMove(board_);
        outStats = ai.getStatistics();
        return eval;
    }

    
    AIVsAIResult runAIVsAIGame(int depthX, bool memoX, int depthO, bool memoO, const std::string& csvFilename, std::atomic<bool>* cancelFlag = nullptr, int timeLimitMsX = -1, int timeLimitMsO = -1)
    {
        int rows   = board_.getRows();
        int cols   = board_.getCols();
        int winLen = board_.getWinLength();
        newGame(rows, cols, winLen, mode_, swapRuleEnabled_);

        AIVsAIResult result{};
        result.mode = mode_;
        result.finalLinesX = 0;
        result.finalLinesO = 0;
        result.finalScore = 0;
        result.xWins = false;
        result.oWins = false;
        result.draw = false;
        result.moves.clear();

        std::ofstream csv(csvFilename);
        if (csv.is_open()) {
            csv << "Move,Player,Row,Col,EvalScore,"
                   "NodesVisited,NodesGenerated,CacheHits,CacheMisses,TimeMs\n";
        }

        int moveIndex = 0;

        while (!gameOver_) {
            if (cancelFlag && cancelFlag->load(std::memory_order_relaxed)) {
                break;
            }
            ++moveIndex;
            Player aiPlayer = currentPlayer_;
            int  depth = (aiPlayer == Player::X) ? depthX : depthO;
            bool memo  = (aiPlayer == Player::X) ? memoX : memoO;

            if (maybeAutoSwapForCurrent(depth, memo, 0, cancelFlag)) {
                AIVsAIMoveInfo info;
                info.isSwap   = true;
                info.player   = aiPlayer;
                info.move     = Coord(-1, -1);
                info.evalScore = 0;
                info.stats    = AIStatistics();
                result.moves.push_back(info);
                if (csv.is_open()) {
                    csv << moveIndex << ",SWAP,,,,,,,,\n";
                }
                continue;
            }

            AIStatistics stats;
            int timeLimit = (aiPlayer == Player::X) ? timeLimitMsX : timeLimitMsO;
            MoveEvaluation bestSoFar(Coord(-1, -1), std::numeric_limits<int>::min());
            MoveEvaluation eval = findBestMove(aiPlayer, depth, memo, stats, &bestSoFar, cancelFlag, timeLimit);

            auto isUsableMove = [&](const MoveEvaluation& mv) {
                if (mv.move.row() < 0 || mv.move.col() < 0) return false;
                int rows = board_.getRows();
                int cols = board_.getCols();
                if (mv.move.row() >= rows || mv.move.col() >= cols) return false;
                if (!board_.isEmpty(mv.move.row(), mv.move.col())) return false;
                return true;
            };
            MoveEvaluation chosen = isUsableMove(bestSoFar) ? bestSoFar : eval;
            if (!isUsableMove(chosen)) {
                break;
            }

            MoveStatus st = applyMove(chosen.move.row(), chosen.move.col());
            if (st != MoveStatus::Ok) {
                break;
            }

            AIVsAIMoveInfo info;
            info.player    = aiPlayer;
            info.move      = chosen.move;
            info.evalScore = chosen.score;
            info.stats     = stats;
            result.moves.push_back(info);

            if (csv.is_open()) {
                csv << moveIndex << ","
                    << (aiPlayer == Player::X ? "X" : "O") << ","
                    << chosen.move.row() << "," << chosen.move.col() << ","
                    << chosen.score << ","
                    << stats.nodesVisited << ","
                    << stats.nodesGenerated << ","
                    << stats.cacheHits << ","
                    << stats.cacheMisses << ","
                    << stats.timeMs << "\n";
            }

            
            std::function<void(const Board&, Player, const MoveEvaluation&, const AIStatistics&)> cb;
            {
                std::lock_guard<std::mutex> lk(cbMutex_);
                cb = onAIMoveCallback_;
            }
            if (cb) {
                cb(board_, aiPlayer, eval, stats);
            }
        }

        
        if (mode_ == GameMode::LinesScore) {
            result.finalLinesX = creditedLinesX_;
            result.finalLinesO = creditedLinesO_;
            result.finalScore  = score();

            if (result.finalScore > 0) {
                result.oWins = true;
            } else if (result.finalScore < 0) {
                result.xWins = true;
            } else {
                result.draw = true;
            }
        } else { 
            bool xWin = board_.checkWin(CellState::X);
            bool oWin = board_.checkWin(CellState::O);
            result.finalLinesX = countLinesFor(board_, CellState::X);
            result.finalLinesO = countLinesFor(board_, CellState::O);
            result.finalScore  = result.finalLinesO - result.finalLinesX;

            result.xWins = xWin && !oWin;
            result.oWins = oWin && !xWin;
            result.draw  = !xWin && !oWin;
        }

        if (csv.is_open()) {
            csv.close();
        }
        return result;
    }

    
    inline bool stepAIVsAIMove(int depthX, bool memoX, int depthO, bool memoO, AIVsAIMoveInfo& outMove, std::atomic<bool>* cancelFlag = nullptr, int timeLimitMsX = -1, int timeLimitMsO = -1)
    {
        if (gameOver_) {
            return false;
        }

        Player aiPlayer = currentPlayer_;
        int  depth = (aiPlayer == Player::X) ? depthX : depthO;
        bool memo  = (aiPlayer == Player::X) ? memoX : memoO;

        
        if (maybeAutoSwapForCurrent(depth, memo, 0, cancelFlag)) {
            outMove.isSwap   = true;
            outMove.player   = currentPlayer_;
            outMove.move     = Coord(-1, -1);
            outMove.evalScore = 0;
            outMove.stats    = AIStatistics();
            return true;
        }

        AIStatistics stats;
        int timeLimit = (aiPlayer == Player::X) ? timeLimitMsX : timeLimitMsO;
        MoveEvaluation bestSoFar(Coord(-1, -1), std::numeric_limits<int>::min());
        MoveEvaluation eval = findBestMove(aiPlayer, depth, memo, stats, &bestSoFar, cancelFlag, timeLimit);

        auto isUsableMove = [&](const MoveEvaluation& mv) {
            if (mv.move.row() < 0 || mv.move.col() < 0) return false;
            int rows = board_.getRows();
            int cols = board_.getCols();
            if (mv.move.row() >= rows || mv.move.col() >= cols) return false;
            if (!board_.isEmpty(mv.move.row(), mv.move.col())) return false;
            return true;
        };
        MoveEvaluation chosen = isUsableMove(bestSoFar) ? bestSoFar : eval;
        if (!isUsableMove(chosen)) {
            return false;
        }

        MoveStatus st = applyMove(chosen.move.row(), chosen.move.col());
        if (st != MoveStatus::Ok) {
            return false;
        }

        outMove.player    = aiPlayer;
        outMove.move      = chosen.move;
        outMove.evalScore = chosen.score;
        outMove.stats     = stats;

        return true;
    }

private:
    Board board_;
    GameMode mode_;
    Player currentPlayer_;
    bool gameOver_;
    int moveNumber_;

    int creditedLinesX_;
    int creditedLinesO_;

    int prevLinesX_;
    int prevLinesO_;

    bool swapRuleEnabled_;
    bool swapAvailable_;
    bool swapUsed_;

    MinimaxAI aiX_;
    MinimaxAI aiO_;

    std::function<void(const Board&, Player, const MoveEvaluation&, const AIStatistics&)> onAIMoveCallback_;
    mutable std::mutex cbMutex_;

    
    MoveEvaluation chooseSwapAwareFirstMove(int depth, bool memo, AIStatistics& outStats, int timeLimitMs = -1, std::atomic<bool>* cancelFlag = nullptr) const {
        DynamicArray<Coord> moves = board_.getCandidateMoves(1);
        if (moves.empty()) {
            outStats.reset();
            return MoveEvaluation(Coord(-1, -1), 0);
        }

        SearchParams params;
        params.setMaxDepth(depth);
        params.setUseMemoization(memo);

        int bestAbs = std::numeric_limits<int>::max();
        MoveEvaluation bestMove(Coord(-1, -1), std::numeric_limits<int>::min());
        AIStatistics aggStats;

        auto startTime = std::chrono::steady_clock::now();
        for (size_t i = 0; i < moves.size(); ++i) {
            if (cancelFlag && cancelFlag->load(std::memory_order_relaxed)) {
                break;
            }
            if (timeLimitMs > 0) {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
                if (elapsed >= timeLimitMs) {
                    break;
                }
            }
            Coord mv = moves[i];
            Board temp = board_;
            temp.set(mv, CellState::X);

            int nextScoreX = creditedLinesX_;
            int nextScoreO = creditedLinesO_;
            if (mode_ == GameMode::LinesScore) {
                int gained = temp.countLinesFromMove(mv, CellState::X);
                int delta = std::min(2, std::max(0, gained));
                nextScoreX += delta;
            }

            AnalysisResult res = analysePosition(temp,
                                                 Player::O, 
                                                 mode_,
                                                 params,
                                                 nextScoreX,
                                                 nextScoreO,
                                                 cancelFlag);
            
            int scoreForX = -res.bestScore;
            int absScore = std::abs(scoreForX);

            aggStats.nodesVisited   += res.stats.nodesVisited;
            aggStats.nodesGenerated += res.stats.nodesGenerated;
            aggStats.cacheHits      += res.stats.cacheHits;
            aggStats.cacheMisses    += res.stats.cacheMisses;
            aggStats.timeMs         += res.stats.timeMs;

            if (absScore < bestAbs || (absScore == bestAbs && scoreForX > bestMove.score)) {
                bestAbs = absScore;
                bestMove = MoveEvaluation(mv, scoreForX);
            }
        }

        auto endTime = std::chrono::steady_clock::now();
        aggStats.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
        aggStats.completedDepth = 1;
        outStats = aggStats;
        return bestMove;
    }
};
