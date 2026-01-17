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

enum class EnginePreset {
    Fast,
    Strict
};

enum class OpeningRule {
    None,
    PieSwap,
    Swap2,
    Swap2Plus
};

enum class Seat {
    A = 0,
    B = 1
};

enum class OpeningPhase {
    Normal,

    Pie_OfferSwap,

    Swap2_A_Place1_X,
    Swap2_A_Place2_O,
    Swap2_A_Place3_X,
    Swap2_B_ChooseOption,
    Swap2_B_PlaceExtraO,
    Swap2_B_Place4_O,
    Swap2_B_Place5_X,
    Swap2_A_FinalChooseSide,

    Swap2P_A_Place1_X,
    Swap2P_A_Place2_O,
    Swap2P_B_ChooseSide,
    Swap2P_B_Place3_SelectedSide,
    Swap2P_A_FinalChooseSide
};

enum class Swap2Option {
    TakeX,
    TakeO_AndPlaceExtraO,
    PlaceTwoAndGiveChoice
};

class OpeningDecision {
public:
    OpeningPhase phase = OpeningPhase::Normal;
    bool valid = false;

    bool pieSwap = false;

    Swap2Option swap2Option = Swap2Option::TakeX;
    Player swap2FinalSideA = Player::X;

    Player swap2PlusSideB = Player::X;
    Player swap2PlusFinalSideA = Player::X;

    std::string actionText;
};


class AIVsAIMoveInfo {
public:
    bool isSwap = false;
    Seat seat = Seat::A;
    OpeningPhase phase = OpeningPhase::Normal;
    std::string action;
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
    GameController(int rows      = 3, int cols      = 3, int winLength = 3, GameMode mode = GameMode::LinesScore, OpeningRule openingRule = OpeningRule::None)
        : board_(rows, cols, winLength)
        , mode_(mode)
        , currentPlayer_(Player::X)
        , gameOver_(false)
        , moveNumber_(0)
        , creditedLinesX_(0)
        , creditedLinesO_(0)
        , openingRule_(openingRule)
        , aiA_(Player::X, 9, true, mode)
        , aiB_(Player::O, 9, true, mode)
    {
        resetOpeningState();
        aiA_.setMoveGenMode(moveGenMode_);
        aiB_.setMoveGenMode(moveGenMode_);
        aiA_.setUseLMR(useLMR_);
        aiB_.setUseLMR(useLMR_);
        aiA_.setUseExtensions(useExtensions_);
        aiB_.setUseExtensions(useExtensions_);
        aiA_.setPerfectClassic3(perfectClassic3_);
        aiB_.setPerfectClassic3(perfectClassic3_);
        aiA_.setAllowOpeningShortcut(enginePreset_ == EnginePreset::Fast);
        aiB_.setAllowOpeningShortcut(enginePreset_ == EnginePreset::Fast);
    }

    void setOnAIMoveCallback(const std::function<void(const Board&, Player, const MoveEvaluation&, const AIStatistics&)>& cb)
    {
        std::lock_guard<std::mutex> lk(cbMutex_);
        onAIMoveCallback_ = cb;
    }

    void newGame(int rows, int cols, int winLength, GameMode mode, OpeningRule openingRule = OpeningRule::None) {
        std::lock_guard<std::recursive_mutex> lk(stateMutex_);
        board_ = Board(rows, cols, winLength);
        mode_ = mode;
        gameOver_ = false;
        moveNumber_ = 0;
        creditedLinesX_ = 0;
        creditedLinesO_ = 0;
        openingRule_ = openingRule;
        resetOpeningState();
        aiA_.setMode(mode_);
        aiB_.setMode(mode_);
        aiA_.clearCache();
        aiB_.clearCache();
        aiA_.setMoveGenMode(moveGenMode_);
        aiB_.setMoveGenMode(moveGenMode_);
        aiA_.setUseLMR(useLMR_);
        aiB_.setUseLMR(useLMR_);
        aiA_.setUseExtensions(useExtensions_);
        aiB_.setUseExtensions(useExtensions_);
        aiA_.setPerfectClassic3(perfectClassic3_);
        aiB_.setPerfectClassic3(perfectClassic3_);
        aiA_.setAllowOpeningShortcut(enginePreset_ == EnginePreset::Fast);
        aiB_.setAllowOpeningShortcut(enginePreset_ == EnginePreset::Fast);
    }

    void setMoveGenMode(MoveGenMode mode) {
        std::lock_guard<std::recursive_mutex> lk(stateMutex_);
        moveGenMode_ = mode;
        aiA_.setMoveGenMode(moveGenMode_);
        aiB_.setMoveGenMode(moveGenMode_);
    }

    void setUseLMR(bool v) {
        std::lock_guard<std::recursive_mutex> lk(stateMutex_);
        useLMR_ = v;
        aiA_.setUseLMR(useLMR_);
        aiB_.setUseLMR(useLMR_);
    }

    void setUseExtensions(bool v) {
        std::lock_guard<std::recursive_mutex> lk(stateMutex_);
        useExtensions_ = v;
        aiA_.setUseExtensions(useExtensions_);
        aiB_.setUseExtensions(useExtensions_);
    }

    void setPerfectClassic3(bool v) {
        std::lock_guard<std::recursive_mutex> lk(stateMutex_);
        perfectClassic3_ = v;
        aiA_.setPerfectClassic3(perfectClassic3_);
        aiB_.setPerfectClassic3(perfectClassic3_);
    }

    void setEnginePreset(EnginePreset preset, int boardRows = -1, int boardCols = -1) {
        std::lock_guard<std::recursive_mutex> lk(stateMutex_);
        enginePreset_ = preset;
        MoveGenMode mode = MoveGenMode::Hybrid;
        bool lmr = true;
        bool ext = true;
        bool perfect = true;
        getPresetSettings(preset, mode, lmr, ext, perfect);
        int rows = boardRows > 0 ? boardRows : board_.getRows();
        int cols = boardCols > 0 ? boardCols : board_.getCols();
        if (preset == EnginePreset::Strict && rows * cols >= 64) {
            mode = MoveGenMode::Hybrid;
            lmr = true;
            ext = false;
        }
        setMoveGenMode(mode);
        setUseLMR(lmr);
        setUseExtensions(ext);
        setPerfectClassic3(perfect);
        aiA_.setAllowOpeningShortcut(preset == EnginePreset::Fast);
        aiB_.setAllowOpeningShortcut(preset == EnginePreset::Fast);
    }

    EnginePreset enginePreset() const { return enginePreset_; }
    void getPresetSettings(EnginePreset preset, MoveGenMode& mode, bool& lmr, bool& ext, bool& perfect) const {
        if (preset == EnginePreset::Strict) {
            mode = MoveGenMode::Full;
            lmr = false;
            ext = false;
            perfect = false;
        } else {
            mode = MoveGenMode::Hybrid;
            lmr = true;
            ext = true;
            perfect = false;
        }
    }
    void setOpeningRule(OpeningRule rule) {
        std::lock_guard<std::recursive_mutex> lk(stateMutex_);
        openingRule_ = rule;
    }
    OpeningRule openingRule() const {
        std::lock_guard<std::recursive_mutex> lk(stateMutex_);
        return openingRule_;
    }
    OpeningPhase openingPhase() const {
        std::lock_guard<std::recursive_mutex> lk(stateMutex_);
        return openingPhase_;
    }
    Seat seatToMove() const {
        std::lock_guard<std::recursive_mutex> lk(stateMutex_);
        return seatToMove_;
    }
    Seat seatForSide(Player side) const {
        std::lock_guard<std::recursive_mutex> lk(stateMutex_);
        return seatOf(side);
    }
    Player sideForSeat(Seat seat) const {
        std::lock_guard<std::recursive_mutex> lk(stateMutex_);
        return sideOf(seat);
    }
    bool swapUsed() const {
        std::lock_guard<std::recursive_mutex> lk(stateMutex_);
        return swapUsed_;
    }
    bool isOpeningChoiceRequired() const {
        std::lock_guard<std::recursive_mutex> lk(stateMutex_);
        return openingPhase_ == OpeningPhase::Pie_OfferSwap ||
               openingPhase_ == OpeningPhase::Swap2_B_ChooseOption ||
               openingPhase_ == OpeningPhase::Swap2_A_FinalChooseSide ||
               openingPhase_ == OpeningPhase::Swap2P_B_ChooseSide ||
               openingPhase_ == OpeningPhase::Swap2P_A_FinalChooseSide;
    }
    bool canPlaceStoneNow() const {
        std::lock_guard<std::recursive_mutex> lk(stateMutex_);
        if (openingPhase_ == OpeningPhase::Normal) return true;
        return openingPhase_ == OpeningPhase::Swap2_A_Place1_X ||
               openingPhase_ == OpeningPhase::Swap2_A_Place2_O ||
               openingPhase_ == OpeningPhase::Swap2_A_Place3_X ||
               openingPhase_ == OpeningPhase::Swap2_B_PlaceExtraO ||
               openingPhase_ == OpeningPhase::Swap2_B_Place4_O ||
               openingPhase_ == OpeningPhase::Swap2_B_Place5_X ||
               openingPhase_ == OpeningPhase::Swap2P_A_Place1_X ||
               openingPhase_ == OpeningPhase::Swap2P_A_Place2_O ||
               openingPhase_ == OpeningPhase::Swap2P_B_Place3_SelectedSide;
    }
    DynamicArray<Swap2Option> availableSwap2Options() const {
        std::lock_guard<std::recursive_mutex> lk(stateMutex_);
        DynamicArray<Swap2Option> opts;
        if (openingPhase_ == OpeningPhase::Swap2_B_ChooseOption) {
            opts.push_back(Swap2Option::TakeX);
            opts.push_back(Swap2Option::TakeO_AndPlaceExtraO);
            opts.push_back(Swap2Option::PlaceTwoAndGiveChoice);
        }
        return opts;
    }
    DynamicArray<Player> availableSideChoices() const {
        std::lock_guard<std::recursive_mutex> lk(stateMutex_);
        DynamicArray<Player> opts;
        if (openingPhase_ == OpeningPhase::Swap2_A_FinalChooseSide ||
            openingPhase_ == OpeningPhase::Swap2P_B_ChooseSide ||
            openingPhase_ == OpeningPhase::Swap2P_A_FinalChooseSide) {
            opts.push_back(Player::X);
            opts.push_back(Player::O);
        }
        return opts;
    }
    Seat lastOpeningChoiceSeat() const {
        std::lock_guard<std::recursive_mutex> lk(stateMutex_);
        return lastOpeningChoiceSeat_;
    }
    OpeningPhase lastOpeningChoicePhase() const {
        std::lock_guard<std::recursive_mutex> lk(stateMutex_);
        return lastOpeningChoicePhase_;
    }
    std::string lastOpeningChoiceAction() const {
        std::lock_guard<std::recursive_mutex> lk(stateMutex_);
        return lastOpeningChoiceAction_;
    }

    Board boardSnapshot() const {
        std::lock_guard<std::recursive_mutex> lk(stateMutex_);
        return board_;
    }

    GameMode mode() const {
        std::lock_guard<std::recursive_mutex> lk(stateMutex_);
        return mode_;
    }
    Player currentPlayer() const {
        std::lock_guard<std::recursive_mutex> lk(stateMutex_);
        return currentPlayer_;
    }
    bool isGameOver() const {
        std::lock_guard<std::recursive_mutex> lk(stateMutex_);
        return gameOver_;
    }
    int moveNumber() const {
        std::lock_guard<std::recursive_mutex> lk(stateMutex_);
        return moveNumber_;
    }

    bool canPieSwap() const {
        std::lock_guard<std::recursive_mutex> lk(stateMutex_);
        return openingPhase_ == OpeningPhase::Pie_OfferSwap;
    }
    bool canSwap2ChooseOption() const {
        std::lock_guard<std::recursive_mutex> lk(stateMutex_);
        return openingPhase_ == OpeningPhase::Swap2_B_ChooseOption;
    }
    bool canSwap2FinalChooseSide() const {
        std::lock_guard<std::recursive_mutex> lk(stateMutex_);
        return openingPhase_ == OpeningPhase::Swap2_A_FinalChooseSide;
    }
    bool canSwap2PlusChooseSideB() const {
        std::lock_guard<std::recursive_mutex> lk(stateMutex_);
        return openingPhase_ == OpeningPhase::Swap2P_B_ChooseSide;
    }
    bool canSwap2PlusFinalChooseSideA() const {
        std::lock_guard<std::recursive_mutex> lk(stateMutex_);
        return openingPhase_ == OpeningPhase::Swap2P_A_FinalChooseSide;
    }

    bool choosePieSwap(bool doSwap) {
        std::lock_guard<std::recursive_mutex> lk(stateMutex_);
        if (!canPieSwap()) return false;
        recordOpeningChoice(seatToMove_, openingPhase_, doSwap ? "PieSwap=Swap" : "PieSwap=Stay");
        if (doSwap) {
            swapSidesForSeats();
        }
        openingPhase_ = OpeningPhase::Normal;
        setNextToMove(Player::O);
        return true;
    }

    bool chooseSwap2Option(Swap2Option opt) {
        std::lock_guard<std::recursive_mutex> lk(stateMutex_);
        if (!canSwap2ChooseOption()) return false;
        seatToMove_ = Seat::B;
        if (opt == Swap2Option::TakeX) {
            recordOpeningChoice(seatToMove_, openingPhase_, "Swap2Option=TakeX");
        } else if (opt == Swap2Option::TakeO_AndPlaceExtraO) {
            recordOpeningChoice(seatToMove_, openingPhase_, "Swap2Option=TakeO_AndPlaceExtraO");
        } else if (opt == Swap2Option::PlaceTwoAndGiveChoice) {
            recordOpeningChoice(seatToMove_, openingPhase_, "Swap2Option=PlaceTwoAndGiveChoice");
        }
        if (opt == Swap2Option::TakeX) {
            swapSidesForSeats();
            openingPhase_ = OpeningPhase::Normal;
            setNextToMove(Player::O);
            return true;
        }
        if (opt == Swap2Option::TakeO_AndPlaceExtraO) {
            openingPhase_ = OpeningPhase::Swap2_B_PlaceExtraO;
            setNextToMove(Player::O);
            return true;
        }
        if (opt == Swap2Option::PlaceTwoAndGiveChoice) {
            openingPhase_ = OpeningPhase::Swap2_B_Place4_O;
            setNextToMove(Player::O);
            return true;
        }
        return false;
    }

    bool chooseSwap2FinalSide(Player sideForA) {
        std::lock_guard<std::recursive_mutex> lk(stateMutex_);
        if (!canSwap2FinalChooseSide()) return false;
        seatToMove_ = Seat::A;
        recordOpeningChoice(seatToMove_, openingPhase_, sideForA == Player::X ? "Swap2FinalSide=X" : "Swap2FinalSide=O");
        bool aCurrentlyX = (seatX_ == Seat::A);
        if ((sideForA == Player::X) != aCurrentlyX) {
            swapSidesForSeats();
        }
        openingPhase_ = OpeningPhase::Normal;
        setNextToMove(Player::O);
        return true;
    }

    bool chooseSwap2PlusSideB(Player sideForB) {
        std::lock_guard<std::recursive_mutex> lk(stateMutex_);
        if (!canSwap2PlusChooseSideB()) return false;
        seatToMove_ = Seat::B;
        recordOpeningChoice(seatToMove_, openingPhase_, sideForB == Player::X ? "Swap2PlusSideB=X" : "Swap2PlusSideB=O");
        bool bCurrentlyX = (seatX_ == Seat::B);
        if ((sideForB == Player::X) != bCurrentlyX) {
            swapSidesForSeats();
        }
        swap2PlusChosenSide_ = sideForB;
        openingPhase_ = OpeningPhase::Swap2P_B_Place3_SelectedSide;
        setNextToMove(sideForB);
        return true;
    }

    bool chooseSwap2PlusFinalSideA(Player sideForA) {
        std::lock_guard<std::recursive_mutex> lk(stateMutex_);
        if (!canSwap2PlusFinalChooseSideA()) return false;
        seatToMove_ = Seat::A;
        recordOpeningChoice(seatToMove_, openingPhase_, sideForA == Player::X ? "Swap2PlusFinalSideA=X" : "Swap2PlusFinalSideA=O");
        bool aCurrentlyX = (seatX_ == Seat::A);
        if ((sideForA == Player::X) != aCurrentlyX) {
            swapSidesForSeats();
        }
        openingPhase_ = OpeningPhase::Normal;
        Player nextSide = (swap2PlusChosenSide_ == Player::X) ? Player::O : Player::X;
        setNextToMove(nextSide);
        return true;
    }

    int creditedLinesX() const {
        std::lock_guard<std::recursive_mutex> lk(stateMutex_);
        return creditedLinesX_;
    }
    int creditedLinesO() const {
        std::lock_guard<std::recursive_mutex> lk(stateMutex_);
        return creditedLinesO_;
    }

    
    int score() const {
        std::lock_guard<std::recursive_mutex> lk(stateMutex_);
        return creditedLinesO_ - creditedLinesX_;
    }

    OpeningDecision computeOpeningDecisionForCurrentSeatAI(int depthHint,
                                                           bool memoHint,
                                                           int timeLimitMs,
                                                           std::atomic<bool>* cancelFlag) const
    {
        OpeningDecision decision;
        OpeningPhase phaseSnapshot = OpeningPhase::Normal;
        Seat seatToMoveSnapshot = Seat::A;
        Seat seatXSnapshot = Seat::A;
        Seat seatOSnapshot = Seat::B;
        Player currentPlayerSnapshot = Player::X;
        int scoreXSnapshot = 0;
        int scoreOSnapshot = 0;
        Board boardSnapshot;
        {
            std::lock_guard<std::recursive_mutex> lk(stateMutex_);
            phaseSnapshot = openingPhase_;
            seatToMoveSnapshot = seatToMove_;
            seatXSnapshot = seatX_;
            seatOSnapshot = seatO_;
            currentPlayerSnapshot = currentPlayer_;
            scoreXSnapshot = creditedLinesX_;
            scoreOSnapshot = creditedLinesO_;
            boardSnapshot = board_;
        }

        decision.phase = phaseSnapshot;
        if (phaseSnapshot == OpeningPhase::Normal) {
            return decision;
        }

        SearchParams params = makeOpeningParams(depthHint, memoHint);
        int effectiveLimitMs = timeLimitMs > 0
            ? std::min(timeLimitMs, OPENING_TOTAL_LIMIT_MS)
            : OPENING_TOTAL_LIMIT_MS;
        OpeningTimeScope scope(*this, effectiveLimitMs);

        auto sideOfSeat = [&](Seat s) {
            return (s == seatXSnapshot) ? Player::X : Player::O;
        };

        if (phaseSnapshot == OpeningPhase::Pie_OfferSwap) {
            Player seatSide = sideOfSeat(seatToMoveSnapshot);
            int stayScore = evaluateForSeat(boardSnapshot, currentPlayerSnapshot, seatSide,
                                            scoreXSnapshot, scoreOSnapshot, params, cancelFlag);
            Player swappedSide = (seatSide == Player::X) ? Player::O : Player::X;
            int swapScore = evaluateForSeat(boardSnapshot, currentPlayerSnapshot, swappedSide,
                                            scoreXSnapshot, scoreOSnapshot, params, cancelFlag);
            decision.pieSwap = (swapScore > stayScore);
            decision.valid = true;
            decision.actionText = decision.pieSwap
                ? "Поменялся сторонами (Pie-swap)"
                : "Оставил стороны (Pie-swap)";
            return decision;
        }

        if (phaseSnapshot == OpeningPhase::Swap2_B_ChooseOption) {
            int takeX = evaluateSwap2OptionTakeX(boardSnapshot, scoreXSnapshot, scoreOSnapshot, params, cancelFlag);
            int extraO = evaluateSwap2OptionExtraO(boardSnapshot, scoreXSnapshot, scoreOSnapshot, params, cancelFlag);
            int placeTwo = evaluateSwap2BPlaceTwoValue(boardSnapshot, scoreXSnapshot, scoreOSnapshot, params, cancelFlag);
            decision.swap2Option = Swap2Option::TakeX;
            int best = takeX;
            decision.actionText = "Взял X (Swap2)";
            if (extraO > best) {
                best = extraO;
                decision.swap2Option = Swap2Option::TakeO_AndPlaceExtraO;
                decision.actionText = "Взял O + доп. O (Swap2)";
            }
            if (placeTwo > best) {
                best = placeTwo;
                decision.swap2Option = Swap2Option::PlaceTwoAndGiveChoice;
                decision.actionText = "Поставил 2 и дал выбор (Swap2)";
            }
            decision.valid = true;
            return decision;
        }

        if (phaseSnapshot == OpeningPhase::Swap2_A_FinalChooseSide) {
            int scoreX = evaluateForSeat(boardSnapshot, currentPlayerSnapshot, Player::X,
                                         scoreXSnapshot, scoreOSnapshot, params, cancelFlag);
            int scoreO = evaluateForSeat(boardSnapshot, currentPlayerSnapshot, Player::O,
                                         scoreXSnapshot, scoreOSnapshot, params, cancelFlag);
            decision.swap2FinalSideA = (scoreX >= scoreO) ? Player::X : Player::O;
            decision.actionText = (decision.swap2FinalSideA == Player::X)
                ? "Игрок A выбрал X (Swap2)"
                : "Игрок A выбрал O (Swap2)";
            decision.valid = true;
            return decision;
        }

        if (phaseSnapshot == OpeningPhase::Swap2P_B_ChooseSide) {
            int bestX = evaluateSwap2PlusBestForBChosenSide(boardSnapshot, scoreXSnapshot, scoreOSnapshot,
                                                           Player::X, params, cancelFlag);
            int bestO = evaluateSwap2PlusBestForBChosenSide(boardSnapshot, scoreXSnapshot, scoreOSnapshot,
                                                           Player::O, params, cancelFlag);
            decision.swap2PlusSideB = (bestX >= bestO) ? Player::X : Player::O;
            decision.actionText = (decision.swap2PlusSideB == Player::X)
                ? "Игрок B выбрал X (Swap2+)"
                : "Игрок B выбрал O (Swap2+)";
            decision.valid = true;
            return decision;
        }

        if (phaseSnapshot == OpeningPhase::Swap2P_A_FinalChooseSide) {
            int scoreX = evaluateForSeat(boardSnapshot, currentPlayerSnapshot, Player::X,
                                         scoreXSnapshot, scoreOSnapshot, params, cancelFlag);
            int scoreO = evaluateForSeat(boardSnapshot, currentPlayerSnapshot, Player::O,
                                         scoreXSnapshot, scoreOSnapshot, params, cancelFlag);
            decision.swap2PlusFinalSideA = (scoreX >= scoreO) ? Player::X : Player::O;
            decision.actionText = (decision.swap2PlusFinalSideA == Player::X)
                ? "Игрок A выбрал X (Swap2+)"
                : "Игрок A выбрал O (Swap2+)";
            decision.valid = true;
            return decision;
        }

        return decision;
    }

    bool applyOpeningDecision(const OpeningDecision& d) {
        if (!d.valid) return false;
        if (openingPhase() != d.phase) return false;

        switch (d.phase) {
        case OpeningPhase::Pie_OfferSwap:
            return choosePieSwap(d.pieSwap);
        case OpeningPhase::Swap2_B_ChooseOption:
            return chooseSwap2Option(d.swap2Option);
        case OpeningPhase::Swap2_A_FinalChooseSide:
            return chooseSwap2FinalSide(d.swap2FinalSideA);
        case OpeningPhase::Swap2P_B_ChooseSide:
            return chooseSwap2PlusSideB(d.swap2PlusSideB);
        case OpeningPhase::Swap2P_A_FinalChooseSide:
            return chooseSwap2PlusFinalSideA(d.swap2PlusFinalSideA);
        default:
            break;
        }
        return false;
    }

    bool autoResolveOpeningChoiceForCurrentAI(int depthHint,
                                              bool memoHint,
                                              std::atomic<bool>* cancelFlag = nullptr,
                                              int timeLimitMs = -1)
    {
        OpeningPhase phaseSnapshot = OpeningPhase::Normal;
        Seat seatToMoveSnapshot = Seat::A;
        Player currentPlayerSnapshot = Player::X;
        OpeningRule openingRuleSnapshot = OpeningRule::None;

        {
            std::lock_guard<std::recursive_mutex> lk(stateMutex_);
            if (gameOver_) return false;
            if (cancelFlag && cancelFlag->load(std::memory_order_relaxed)) return false;
            if (!(openingPhase_ == OpeningPhase::Pie_OfferSwap ||
                  openingPhase_ == OpeningPhase::Swap2_B_ChooseOption ||
                  openingPhase_ == OpeningPhase::Swap2_A_FinalChooseSide ||
                  openingPhase_ == OpeningPhase::Swap2P_B_ChooseSide ||
                  openingPhase_ == OpeningPhase::Swap2P_A_FinalChooseSide)) {
                return false;
            }
            phaseSnapshot = openingPhase_;
            seatToMoveSnapshot = seatToMove_;
            currentPlayerSnapshot = currentPlayer_;
            openingRuleSnapshot = openingRule_;
        }

        if (cancelFlag && cancelFlag->load(std::memory_order_relaxed)) return false;
        OpeningDecision decision = computeOpeningDecisionForCurrentSeatAI(depthHint, memoHint, timeLimitMs, cancelFlag);
        if (!decision.valid) return false;
        if (cancelFlag && cancelFlag->load(std::memory_order_relaxed)) return false;

        {
            std::lock_guard<std::recursive_mutex> lk(stateMutex_);
            if (openingPhase_ != phaseSnapshot ||
                seatToMove_ != seatToMoveSnapshot ||
                currentPlayer_ != currentPlayerSnapshot ||
                openingRule_ != openingRuleSnapshot) {
                return false;
            }
        }

        if (cancelFlag && cancelFlag->load(std::memory_order_relaxed)) return false;
        return applyOpeningDecision(decision);
    }

    
    MoveStatus applyMove(int row, int col) {
        std::lock_guard<std::recursive_mutex> lk(stateMutex_);
        if (gameOver_) {
            return MoveStatus::GameAlreadyOver;
        }

        MoveStatus st = MoveStatus::InvalidCell;
        switch (openingPhase_) {
        case OpeningPhase::Normal:
            return applyNormalMove(row, col);
        case OpeningPhase::Pie_OfferSwap:
        case OpeningPhase::Swap2_B_ChooseOption:
        case OpeningPhase::Swap2_A_FinalChooseSide:
        case OpeningPhase::Swap2P_B_ChooseSide:
        case OpeningPhase::Swap2P_A_FinalChooseSide:
            return MoveStatus::InvalidCell;
        case OpeningPhase::Swap2_A_Place1_X:
            seatToMove_ = Seat::A;
            currentPlayer_ = Player::X;
            st = placeStoneForced(CellState::X, row, col);
            if (st != MoveStatus::Ok) return st;
            openingPhase_ = OpeningPhase::Swap2_A_Place2_O;
            currentPlayer_ = Player::O;
            return MoveStatus::Ok;
        case OpeningPhase::Swap2_A_Place2_O:
            seatToMove_ = Seat::A;
            currentPlayer_ = Player::O;
            st = placeStoneForced(CellState::O, row, col);
            if (st != MoveStatus::Ok) return st;
            openingPhase_ = OpeningPhase::Swap2_A_Place3_X;
            currentPlayer_ = Player::X;
            return MoveStatus::Ok;
        case OpeningPhase::Swap2_A_Place3_X:
            seatToMove_ = Seat::A;
            currentPlayer_ = Player::X;
            st = placeStoneForced(CellState::X, row, col);
            if (st != MoveStatus::Ok) return st;
            openingPhase_ = OpeningPhase::Swap2_B_ChooseOption;
            setNextToMove(sideOf(Seat::B));
            return MoveStatus::Ok;
        case OpeningPhase::Swap2_B_PlaceExtraO:
            seatToMove_ = Seat::B;
            currentPlayer_ = Player::O;
            st = placeStoneForced(CellState::O, row, col);
            if (st != MoveStatus::Ok) return st;
            openingPhase_ = OpeningPhase::Normal;
            setNextToMove(Player::X);
            return MoveStatus::Ok;
        case OpeningPhase::Swap2_B_Place4_O:
            seatToMove_ = Seat::B;
            currentPlayer_ = Player::O;
            st = placeStoneForced(CellState::O, row, col);
            if (st != MoveStatus::Ok) return st;
            openingPhase_ = OpeningPhase::Swap2_B_Place5_X;
            currentPlayer_ = Player::X;
            return MoveStatus::Ok;
        case OpeningPhase::Swap2_B_Place5_X:
            seatToMove_ = Seat::B;
            currentPlayer_ = Player::X;
            st = placeStoneForced(CellState::X, row, col);
            if (st != MoveStatus::Ok) return st;
            openingPhase_ = OpeningPhase::Swap2_A_FinalChooseSide;
            seatToMove_ = Seat::A;
            currentPlayer_ = Player::O;
            return MoveStatus::Ok;
        case OpeningPhase::Swap2P_A_Place1_X:
            seatToMove_ = Seat::A;
            currentPlayer_ = Player::X;
            st = placeStoneForced(CellState::X, row, col);
            if (st != MoveStatus::Ok) return st;
            openingPhase_ = OpeningPhase::Swap2P_A_Place2_O;
            currentPlayer_ = Player::O;
            return MoveStatus::Ok;
        case OpeningPhase::Swap2P_A_Place2_O:
            seatToMove_ = Seat::A;
            currentPlayer_ = Player::O;
            st = placeStoneForced(CellState::O, row, col);
            if (st != MoveStatus::Ok) return st;
            openingPhase_ = OpeningPhase::Swap2P_B_ChooseSide;
            setNextToMove(sideOf(Seat::B));
            return MoveStatus::Ok;
        case OpeningPhase::Swap2P_B_Place3_SelectedSide:
            seatToMove_ = Seat::B;
            currentPlayer_ = swap2PlusChosenSide_;
            st = placeStoneForced(playerToCell(swap2PlusChosenSide_), row, col);
            if (st != MoveStatus::Ok) return st;
            openingPhase_ = OpeningPhase::Swap2P_A_FinalChooseSide;
            seatToMove_ = Seat::A;
            currentPlayer_ = (swap2PlusChosenSide_ == Player::X) ? Player::O : Player::X;
            return MoveStatus::Ok;
        }
        return MoveStatus::InvalidCell;
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
                    if (b.getNoCheck(row, col + k) != player) {
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
                    if (b.getNoCheck(row + k, col) != player) {
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
                    if (b.getNoCheck(row + k, col + k) != player) {
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
                    if (b.getNoCheck(row + k, col - k) != player) {
                        win = false;
                        break;
                    }
                }
                if (win) ++count;
            }
        }

        return count;
    }

    
    MoveEvaluation findBestMoveForSeat(Seat seat, Player sideToMove, int depth, bool useMemoization, AIStatistics& outStats, MoveEvaluation* bestSoFar = nullptr, std::atomic<bool>* cancelFlag = nullptr, int timeLimitMs = -1)
    {
        Player seatSide = sideOf(seat);

        if (openingPhase_ != OpeningPhase::Normal) {
            SearchParams params = makeOpeningParams(depth, useMemoization);
            int maxCount = openingMaxCount(board_, 24, 12, 10);
            DynamicArray<Coord> moves = getOpeningCandidates(board_, maxCount);
            MoveEvaluation bestMove;
            bestMove.score = std::numeric_limits<int>::min();
            int effectiveLimitMs = timeLimitMs > 0 ? std::min(timeLimitMs, OPENING_TOTAL_LIMIT_MS) : OPENING_TOTAL_LIMIT_MS;
            OpeningTimeScope scope(*this, effectiveLimitMs);

            switch (openingPhase_) {
            case OpeningPhase::Swap2_A_Place1_X: {
                int best = std::numeric_limits<int>::max();
                for (size_t i = 0; i < moves.size(); ++i) {
                    if (openingTimeExceeded(cancelFlag)) break;
                    Board temp = board_;
                    int sx = creditedLinesX_;
                    int so = creditedLinesO_;
                    applyTempMove(temp, moves[i], CellState::X, sx, so);
                    int val = evaluateSwap2AfterAFirstX(temp, sx, so, params, cancelFlag);
                    if (val < best) {
                        best = val;
                        bestMove = MoveEvaluation(moves[i], -val);
                    }
                }
                outStats.reset();
                return bestMove;
            }
            case OpeningPhase::Swap2_A_Place2_O: {
                int best = std::numeric_limits<int>::max();
                for (size_t i = 0; i < moves.size(); ++i) {
                    if (openingTimeExceeded(cancelFlag)) break;
                    Board temp = board_;
                    int sx = creditedLinesX_;
                    int so = creditedLinesO_;
                    applyTempMove(temp, moves[i], CellState::O, sx, so);
                    int val = evaluateSwap2AfterASecondO(temp, sx, so, params, cancelFlag);
                    if (val < best) {
                        best = val;
                        bestMove = MoveEvaluation(moves[i], -val);
                    }
                }
                outStats.reset();
                return bestMove;
            }
            case OpeningPhase::Swap2_A_Place3_X: {
                int best = std::numeric_limits<int>::max();
                for (size_t i = 0; i < moves.size(); ++i) {
                    if (openingTimeExceeded(cancelFlag)) break;
                    Board temp = board_;
                    int sx = creditedLinesX_;
                    int so = creditedLinesO_;
                    applyTempMove(temp, moves[i], CellState::X, sx, so);
                    int val = evaluateSwap2BestOptionForB(temp, sx, so, params, cancelFlag);
                    if (val < best) {
                        best = val;
                        bestMove = MoveEvaluation(moves[i], -val);
                    }
                }
                outStats.reset();
                return bestMove;
            }
            case OpeningPhase::Swap2_B_PlaceExtraO: {
                int best = std::numeric_limits<int>::min();
                for (size_t i = 0; i < moves.size(); ++i) {
                    if (openingTimeExceeded(cancelFlag)) break;
                    Board temp = board_;
                    int sx = creditedLinesX_;
                    int so = creditedLinesO_;
                    applyTempMove(temp, moves[i], CellState::O, sx, so);
                    int val = evaluateForSeat(temp, Player::X, Player::O, sx, so, params, cancelFlag);
                    if (val > best) {
                        best = val;
                        bestMove = MoveEvaluation(moves[i], val);
                    }
                }
                outStats.reset();
                return bestMove;
            }
            case OpeningPhase::Swap2_B_Place4_O: {
                int best = std::numeric_limits<int>::min();
                for (size_t i = 0; i < moves.size(); ++i) {
                    if (openingTimeExceeded(cancelFlag)) break;
                    Board temp = board_;
                    int sx = creditedLinesX_;
                    int so = creditedLinesO_;
                    applyTempMove(temp, moves[i], CellState::O, sx, so);
                    int val = evaluateSwap2BBestAfterO(temp, sx, so, params, cancelFlag);
                    if (val > best) {
                        best = val;
                        bestMove = MoveEvaluation(moves[i], val);
                    }
                }
                outStats.reset();
                return bestMove;
            }
            case OpeningPhase::Swap2_B_Place5_X: {
                int best = std::numeric_limits<int>::min();
                for (size_t i = 0; i < moves.size(); ++i) {
                    if (openingTimeExceeded(cancelFlag)) break;
                    Board temp = board_;
                    int sx = creditedLinesX_;
                    int so = creditedLinesO_;
                    applyTempMove(temp, moves[i], CellState::X, sx, so);
                    int val = evaluateSwap2BFinalChoiceValue(temp, sx, so, params, cancelFlag);
                    if (val > best) {
                        best = val;
                        bestMove = MoveEvaluation(moves[i], val);
                    }
                }
                outStats.reset();
                return bestMove;
            }
            case OpeningPhase::Swap2P_A_Place1_X: {
                int best = std::numeric_limits<int>::max();
                for (size_t i = 0; i < moves.size(); ++i) {
                    if (openingTimeExceeded(cancelFlag)) break;
                    Board temp = board_;
                    int sx = creditedLinesX_;
                    int so = creditedLinesO_;
                    applyTempMove(temp, moves[i], CellState::X, sx, so);
                    int val = evaluateSwap2PlusAfterAFirstX(temp, sx, so, params, cancelFlag);
                    if (val < best) {
                        best = val;
                        bestMove = MoveEvaluation(moves[i], -val);
                    }
                }
                outStats.reset();
                return bestMove;
            }
            case OpeningPhase::Swap2P_A_Place2_O: {
                int best = std::numeric_limits<int>::max();
                for (size_t i = 0; i < moves.size(); ++i) {
                    if (openingTimeExceeded(cancelFlag)) break;
                    Board temp = board_;
                    int sx = creditedLinesX_;
                    int so = creditedLinesO_;
                    applyTempMove(temp, moves[i], CellState::O, sx, so);
                    int val = evaluateSwap2PlusAfterASecondO(temp, sx, so, params, cancelFlag);
                    if (val < best) {
                        best = val;
                        bestMove = MoveEvaluation(moves[i], -val);
                    }
                }
                outStats.reset();
                return bestMove;
            }
            case OpeningPhase::Swap2P_B_Place3_SelectedSide: {
                Player chosenSide = swap2PlusChosenSide_;
                CellState stone = playerToCell(chosenSide);
                Player toMove = (chosenSide == Player::X) ? Player::O : Player::X;
                int best = std::numeric_limits<int>::min();
                for (size_t i = 0; i < moves.size(); ++i) {
                    if (openingTimeExceeded(cancelFlag)) break;
                    Board temp = board_;
                    int sx = creditedLinesX_;
                    int so = creditedLinesO_;
                    applyTempMove(temp, moves[i], stone, sx, so);
                    int evalAX = evaluateForSeat(temp, toMove, Player::O, sx, so, params, cancelFlag);
                    int evalAO = evaluateForSeat(temp, toMove, Player::X, sx, so, params, cancelFlag);
                    int val = std::min(evalAX, evalAO);
                    if (val > best) {
                        best = val;
                        bestMove = MoveEvaluation(moves[i], val);
                    }
                }
                outStats.reset();
                return bestMove;
            }
            default:
                break;
            }
        }

        bool neutralOpeningMove = false;
        if (openingPhase_ != OpeningPhase::Normal) {
            switch (openingPhase_) {
            case OpeningPhase::Pie_OfferSwap:
            case OpeningPhase::Swap2_B_ChooseOption:
            case OpeningPhase::Swap2_A_FinalChooseSide:
            case OpeningPhase::Swap2P_B_ChooseSide:
            case OpeningPhase::Swap2P_A_FinalChooseSide:
                neutralOpeningMove = false;
                break;
            default:
                neutralOpeningMove = true;
                break;
            }
        }

        if (neutralOpeningMove || sideToMove != seatSide) {
            SearchParams params;
            params.setMaxDepth(depth);
            params.setUseMemoization(useMemoization);
            params.setMoveGenMode(moveGenMode_);
            params.setUseLMR(useLMR_);
            params.setUseExtensions(useExtensions_);
            params.setPerfectClassic3(perfectClassic3_);
            params.setBanCenterFirstMove(openingRule_ == OpeningRule::None);
            params.setTimeLimitMs(timeLimitMs);
            AnalysisResult res = analysePosition(board_, sideToMove, mode_, params,
                                                 creditedLinesX_, creditedLinesO_,
                                                 cancelFlag);
            outStats = res.stats;
            if (res.topMoves.empty()) {
                if (bestSoFar) {
                    *bestSoFar = MoveEvaluation(Coord(-1, -1), std::numeric_limits<int>::min());
                }
                return MoveEvaluation(Coord(-1, -1), std::numeric_limits<int>::min());
            }
            MoveEvaluation bestEval(res.topMoves[0].first, res.topMoves[0].second);
            int bestAbs = std::abs(bestEval.score);
            for (size_t i = 1; i < res.topMoves.size(); ++i) {
                const auto& candidate = res.topMoves[i];
                int absScore = std::abs(candidate.second);
                if (absScore < bestAbs ||
                    (absScore == bestAbs && candidate.second < bestEval.score)) {
                    bestAbs = absScore;
                    bestEval = MoveEvaluation(candidate.first, candidate.second);
                }
            }
            if (bestSoFar) {
                *bestSoFar = bestEval;
            }
            return bestEval;
        }

        MinimaxAI& ai = aiForSeat(seat);
        ai.setPlayer(sideToMove);
        ai.setMaxDepth(depth);
        ai.setUseMemoization(useMemoization);
        ai.setCancelFlag(cancelFlag);
        ai.setBestSoFar(bestSoFar);
        ai.setCredits(creditedLinesX_, creditedLinesO_);
        ai.setTimeLimitMs(timeLimitMs);
        ai.setMoveGenMode(moveGenMode_);
        ai.setUseLMR(useLMR_);
        ai.setUseExtensions(useExtensions_);
        ai.setPerfectClassic3(perfectClassic3_);
        ai.setBanCenterFirstMove(openingRule_ == OpeningRule::None);

        if (openingRule_ == OpeningRule::PieSwap &&
            mode_ == GameMode::LinesScore &&
            openingPhase_ == OpeningPhase::Normal &&
            moveNumber_ == 0 &&
            sideToMove == Player::X)
        {
            return chooseSwapAwareFirstMove(depth, useMemoization, outStats, timeLimitMs, cancelFlag);
        }

        Board searchBoard = board_;
        MoveEvaluation eval = ai.findBestMove(searchBoard);
        outStats = ai.getStatistics();
        return eval;
    }

    MoveEvaluation findBestMove(Player aiPlayer, int depth, bool useMemoization, AIStatistics& outStats, MoveEvaluation* bestSoFar = nullptr, std::atomic<bool>* cancelFlag = nullptr, int timeLimitMs = -1)
    {
        return findBestMoveForSeat(seatOf(aiPlayer), aiPlayer, depth, useMemoization, outStats, bestSoFar, cancelFlag, timeLimitMs);
    }

    
    AIVsAIResult runAIVsAIGame(int depthX, bool memoX, int depthO, bool memoO, const std::string& csvFilename, std::atomic<bool>* cancelFlag = nullptr, int timeLimitMsX = -1, int timeLimitMsO = -1)
    {
        int rows   = board_.getRows();
        int cols   = board_.getCols();
        int winLen = board_.getWinLength();
        newGame(rows, cols, winLen, mode_, openingRule_);

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
            Seat chooser = seatToMove_;
            Player sideToMove = currentPlayer_;
            int  depth = (chooser == Seat::A) ? depthX : depthO;
            bool memo  = (chooser == Seat::A) ? memoX : memoO;

        if (openingPhase_ != OpeningPhase::Normal) {
            Player chooserSide = sideOf(chooser);
                if (autoResolveOpeningChoiceForCurrentAI(depth, memo, cancelFlag)) {
                    AIVsAIMoveInfo info;
                    info.isSwap   = true;
                    info.seat     = lastOpeningChoiceSeat_;
                    info.phase    = lastOpeningChoicePhase_;
                    info.action   = lastOpeningChoiceAction_;
                    info.player   = chooserSide;
                    info.move     = Coord(-1, -1);
                    info.evalScore = 0;
                    info.stats    = AIStatistics();
                result.moves.push_back(info);
                    if (csv.is_open()) {
                        csv << moveIndex << ",CHOICE,,,,,,,,\n";
                    }
                    continue;
                }
            }

            AIStatistics stats;
            int timeLimit = (chooser == Seat::A) ? timeLimitMsX : timeLimitMsO;
            MoveEvaluation bestSoFar(Coord(-1, -1), std::numeric_limits<int>::min());
            MoveEvaluation eval = findBestMoveForSeat(chooser, sideToMove, depth, memo, stats, &bestSoFar, cancelFlag, timeLimit);

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
            info.isSwap   = false;
            info.seat      = chooser;
            info.phase     = OpeningPhase::Normal;
            info.action.clear();
            info.player    = sideToMove;
            info.move      = chosen.move;
            info.evalScore = chosen.score;
            info.stats     = stats;
            result.moves.push_back(info);

            if (csv.is_open()) {
                csv << moveIndex << ","
                    << (sideToMove == Player::X ? "X" : "O") << ","
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
                cb(board_, sideToMove, chosen, stats);
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

        Seat chooser = seatToMove_;
        Player sideToMove = currentPlayer_;
        int  depth = (chooser == Seat::A) ? depthX : depthO;
        bool memo  = (chooser == Seat::A) ? memoX : memoO;

        
        if (openingPhase_ != OpeningPhase::Normal) {
            Player chooserSide = sideOf(chooser);
            if (autoResolveOpeningChoiceForCurrentAI(depth, memo, cancelFlag)) {
                outMove.isSwap   = true;
                outMove.seat     = lastOpeningChoiceSeat_;
                outMove.phase    = lastOpeningChoicePhase_;
                outMove.action   = lastOpeningChoiceAction_;
                outMove.player   = chooserSide;
                outMove.move     = Coord(-1, -1);
                outMove.evalScore = 0;
                outMove.stats    = AIStatistics();
                return true;
            }
        }

        AIStatistics stats;
        int timeLimit = (chooser == Seat::A) ? timeLimitMsX : timeLimitMsO;
        MoveEvaluation bestSoFar(Coord(-1, -1), std::numeric_limits<int>::min());
        MoveEvaluation eval = findBestMoveForSeat(chooser, sideToMove, depth, memo, stats, &bestSoFar, cancelFlag, timeLimit);

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

        outMove.isSwap    = false;
        outMove.action.clear();
        outMove.player    = sideToMove;
        outMove.seat      = chooser;
        outMove.phase     = OpeningPhase::Normal;
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

    OpeningRule  openingRule_  = OpeningRule::None;
    OpeningPhase openingPhase_ = OpeningPhase::Normal;
    Seat seatToMove_ = Seat::A;
    Seat seatX_ = Seat::A;
    Seat seatO_ = Seat::B;
    Player swap2PlusChosenSide_ = Player::X;
    bool swapUsed_ = false;
    Seat lastOpeningChoiceSeat_ = Seat::A;
    OpeningPhase lastOpeningChoicePhase_ = OpeningPhase::Normal;
    std::string lastOpeningChoiceAction_;

    EnginePreset enginePreset_ = EnginePreset::Fast;
    MoveGenMode moveGenMode_ = MoveGenMode::Hybrid;
    bool useLMR_ = true;
    bool useExtensions_ = true;
    bool perfectClassic3_ = false;
    static constexpr int OPENING_ANALYSIS_LIMIT_MS = 120;
    static constexpr int OPENING_TOTAL_LIMIT_MS = 1000;
    mutable int openingTimeLimitMs_ = -1;
    mutable std::chrono::steady_clock::time_point openingStartTime_{};

    MinimaxAI aiA_;
    MinimaxAI aiB_;

    std::function<void(const Board&, Player, const MoveEvaluation&, const AIStatistics&)> onAIMoveCallback_;
    mutable std::mutex cbMutex_;
    mutable std::recursive_mutex stateMutex_;
    
    class OpeningTimeScope {
    public:
        OpeningTimeScope(const GameController& owner, int limitMs)
            : owner_(owner)
            , prevLimit_(owner.openingTimeLimitMs_)
            , prevStart_(owner.openingStartTime_)
        {
            owner_.openingTimeLimitMs_ = limitMs;
            owner_.openingStartTime_ = std::chrono::steady_clock::now();
        }
        ~OpeningTimeScope() {
            owner_.openingTimeLimitMs_ = prevLimit_;
            owner_.openingStartTime_ = prevStart_;
        }
    private:
        const GameController& owner_;
        int prevLimit_;
        std::chrono::steady_clock::time_point prevStart_;
    };

    bool openingTimeExceeded(std::atomic<bool>* cancelFlag) const {
        if (cancelFlag && cancelFlag->load(std::memory_order_relaxed)) {
            return true;
        }
        if (openingTimeLimitMs_ <= 0) return false;
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - openingStartTime_).count();
        return elapsed >= openingTimeLimitMs_;
    }

    int openingMaxCount(const Board& board, int smallMax, int midMax, int largeMax) const {
        int total = board.getRows() * board.getCols();
        int area = total;
        int limit = smallMax;
        if (area >= 100) {
            limit = largeMax;
        } else if (area >= 64) {
            limit = midMax;
        }
        return std::min(limit, total);
    }

    CellState playerToCell(Player p) const {
        return p == Player::X ? CellState::X : CellState::O;
    }

    Seat seatOf(Player side) const {
        return (side == Player::X) ? seatX_ : seatO_;
    }

    Player sideOf(Seat s) const {
        return (s == seatX_) ? Player::X : Player::O;
    }

    MinimaxAI& aiForSeat(Seat seat) {
        return (seat == Seat::A) ? aiA_ : aiB_;
    }

    void swapSidesForSeats() {
        std::swap(seatX_, seatO_);
        swapUsed_ = true;
        aiA_.setPlayer(sideOf(Seat::A));
        aiB_.setPlayer(sideOf(Seat::B));
    }

    void setNextToMove(Player side) {
        currentPlayer_ = side;
        seatToMove_ = seatOf(side);
    }

    void resetOpeningState() {
        openingPhase_ = OpeningPhase::Normal;
        seatToMove_ = Seat::A;
        seatX_ = Seat::A;
        seatO_ = Seat::B;
        swap2PlusChosenSide_ = Player::X;
        swapUsed_ = false;
        lastOpeningChoiceSeat_ = Seat::A;
        lastOpeningChoicePhase_ = OpeningPhase::Normal;
        lastOpeningChoiceAction_.clear();
        aiA_.setPlayer(Player::X);
        aiB_.setPlayer(Player::O);
        setNextToMove(Player::X);

        if (openingRule_ == OpeningRule::Swap2) {
            openingPhase_ = OpeningPhase::Swap2_A_Place1_X;
        } else if (openingRule_ == OpeningRule::Swap2Plus) {
            openingPhase_ = OpeningPhase::Swap2P_A_Place1_X;
        }
    }

    SearchParams makeOpeningParams(int depthHint, bool memoHint) const {
        SearchParams params;
        params.setMaxDepth(std::max(1, std::min(depthHint, 4)));
        params.setUseMemoization(memoHint);
        params.setMoveGenMode(moveGenMode_);
        params.setUseLMR(useLMR_);
        params.setUseExtensions(useExtensions_);
        params.setPerfectClassic3(perfectClassic3_);
        params.setBanCenterFirstMove(openingRule_ == OpeningRule::None);
        params.setTimeLimitMs(OPENING_ANALYSIS_LIMIT_MS);
        return params;
    }

    void appendUnique(DynamicArray<Coord>& list, const Coord& mv) const {
        for (size_t i = 0; i < list.size(); ++i) {
            if (list[i] == mv) return;
        }
        list.push_back(mv);
    }

    int openingMoveHeuristic(const Board& board, const Coord& mv) const {
        int rows = board.getRows();
        int cols = board.getCols();
        int centerRow = rows / 2;
        int centerCol = cols / 2;
        int score = 0;
        int dist = std::abs(mv.row() - centerRow) + std::abs(mv.col() - centerCol);
        score -= dist * 3;
        for (int dr = -1; dr <= 1; ++dr) {
            for (int dc = -1; dc <= 1; ++dc) {
                if (dr == 0 && dc == 0) continue;
                int rr = mv.row() + dr;
                int cc = mv.col() + dc;
                if (rr < 0 || rr >= rows || cc < 0 || cc >= cols) continue;
                if (board.getNoCheck(rr, cc) != CellState::Empty) {
                    score += 2;
                }
            }
        }
        return score;
    }

    DynamicArray<Coord> getOpeningCandidates(const Board& board, int maxCount) const {
        DynamicArray<Coord> moves;
        int rows = board.getRows();
        int cols = board.getCols();
        int total = rows * cols;
        int maxKeep = std::min(maxCount, total);
        int centerRow = rows / 2;
        int centerCol = cols / 2;
        int winLen = board.getWinLength();
        int radius = std::min(3, std::max(2, winLen / 2));

        bool limitReached = false;
        for (int r = centerRow - radius; r <= centerRow + radius && !limitReached; ++r) {
            for (int c = centerCol - radius; c <= centerCol + radius; ++c) {
                if (r < 0 || r >= rows || c < 0 || c >= cols) continue;
                if (!board.isEmpty(r, c)) continue;
                appendUnique(moves, Coord(r, c));
                if (moves.size() >= static_cast<size_t>(maxKeep)) {
                    limitReached = true;
                    break;
                }
            }
        }

        if (!limitReached) {
            DynamicArray<Coord> frontier = board.getCandidateMoves(1);
            for (size_t i = 0; i < frontier.size(); ++i) {
                if (!board.isEmpty(frontier[i].row(), frontier[i].col())) continue;
                appendUnique(moves, frontier[i]);
                if (moves.size() >= static_cast<size_t>(maxKeep)) {
                    limitReached = true;
                    break;
                }
            }
        }

        if (!limitReached && moves.size() < static_cast<size_t>(maxKeep)) {
            DynamicArray<Coord> wider = board.getCandidateMoves(2);
            for (size_t i = 0; i < wider.size(); ++i) {
                if (!board.isEmpty(wider[i].row(), wider[i].col())) continue;
                appendUnique(moves, wider[i]);
                if (moves.size() >= static_cast<size_t>(maxKeep)) {
                    break;
                }
            }
        }

        if (moves.empty()) {
            moves = board.getEmptyCells();
        }

        if (!moves.empty()) {
            std::sort(moves.begin(), moves.end(), [&](const Coord& a, const Coord& b) {
                return openingMoveHeuristic(board, a) > openingMoveHeuristic(board, b);
            });
            if (moves.size() > static_cast<size_t>(maxKeep)) {
                DynamicArray<Coord> limited;
                limited.reserve(static_cast<size_t>(maxKeep));
                for (int i = 0; i < maxKeep; ++i) {
                    limited.push_back(moves[static_cast<size_t>(i)]);
                }
                moves = std::move(limited);
            }
        }
        return moves;
    }

    void applyTempMove(Board& board, const Coord& mv, CellState stone, int& scoreX, int& scoreO) const {
        board.set(mv, stone);
        if (mode_ == GameMode::LinesScore) {
            int gained = board.countLinesFromMove(mv, stone);
            int delta = std::min(2, std::max(0, gained));
            if (stone == CellState::X) scoreX += delta;
            else if (stone == CellState::O) scoreO += delta;
        }
    }

    void recordOpeningChoice(Seat seat, OpeningPhase phase, const std::string& action) {
        lastOpeningChoiceSeat_ = seat;
        lastOpeningChoicePhase_ = phase;
        lastOpeningChoiceAction_ = action;
    }

    int scoreForSeat(Player seatSide, Player toMove, int scoreForToMove) const {
        return (seatSide == toMove) ? scoreForToMove : -scoreForToMove;
    }

    int evaluateForSeat(const Board& board, Player toMove, Player seatSide, int scoreX, int scoreO, const SearchParams& params, std::atomic<bool>* cancelFlag) const {
        if (openingTimeExceeded(cancelFlag)) {
            return 0;
        }
        AnalysisResult res = analysePosition(board, toMove, mode_, params, scoreX, scoreO, cancelFlag);
        return scoreForSeat(seatSide, toMove, res.bestScore);
    }

    int evaluateImbalance(const Board& board, int scoreX, int scoreO, const SearchParams& params, std::atomic<bool>* cancelFlag) const {
        if (openingTimeExceeded(cancelFlag)) {
            return 0;
        }
        AnalysisResult resX = analysePosition(board, Player::X, mode_, params, scoreX, scoreO, cancelFlag);
        if (openingTimeExceeded(cancelFlag)) {
            return std::abs(resX.bestScore);
        }
        AnalysisResult resO = analysePosition(board, Player::O, mode_, params, scoreX, scoreO, cancelFlag);
        int absX = std::abs(resX.bestScore);
        int absO = std::abs(resO.bestScore);
        return std::max(absX, absO);
    }

    int evaluateSwap2BFinalChoiceValue(const Board& board, int scoreX, int scoreO, const SearchParams& params, std::atomic<bool>* cancelFlag) const {
        Player toMove = Player::O;
        if (openingTimeExceeded(cancelFlag)) return 0;
        int evalAX = evaluateForSeat(board, toMove, Player::O, scoreX, scoreO, params, cancelFlag);
        if (openingTimeExceeded(cancelFlag)) return evalAX;
        int evalAO = evaluateForSeat(board, toMove, Player::X, scoreX, scoreO, params, cancelFlag);
        return std::min(evalAX, evalAO);
    }

    int evaluateSwap2BBestAfterO(const Board& board, int scoreX, int scoreO, const SearchParams& params, std::atomic<bool>* cancelFlag) const {
        int maxCount = openingMaxCount(board, 10, 8, 6);
        DynamicArray<Coord> xMoves = getOpeningCandidates(board, maxCount);
        int best = std::numeric_limits<int>::min();
        for (size_t i = 0; i < xMoves.size(); ++i) {
            if (openingTimeExceeded(cancelFlag)) break;
            Board temp = board;
            int sx = scoreX;
            int so = scoreO;
            applyTempMove(temp, xMoves[i], CellState::X, sx, so);
            int val = evaluateSwap2BFinalChoiceValue(temp, sx, so, params, cancelFlag);
            if (val > best) best = val;
        }
        if (openingTimeExceeded(cancelFlag) && best == std::numeric_limits<int>::min()) {
            return 0;
        }
        return best;
    }

    int evaluateSwap2BPlaceTwoValue(const Board& board, int scoreX, int scoreO, const SearchParams& params, std::atomic<bool>* cancelFlag) const {
        int maxCount = openingMaxCount(board, 12, 8, 6);
        DynamicArray<Coord> oMoves = getOpeningCandidates(board, maxCount);
        int best = std::numeric_limits<int>::min();
        for (size_t i = 0; i < oMoves.size(); ++i) {
            if (openingTimeExceeded(cancelFlag)) break;
            Board temp = board;
            int sx = scoreX;
            int so = scoreO;
            applyTempMove(temp, oMoves[i], CellState::O, sx, so);
            int val = evaluateSwap2BBestAfterO(temp, sx, so, params, cancelFlag);
            if (val > best) best = val;
        }
        if (openingTimeExceeded(cancelFlag) && best == std::numeric_limits<int>::min()) {
            return 0;
        }
        return best;
    }

    int evaluateSwap2OptionExtraO(const Board& board, int scoreX, int scoreO, const SearchParams& params, std::atomic<bool>* cancelFlag) const {
        int maxCount = openingMaxCount(board, 12, 8, 6);
        DynamicArray<Coord> oMoves = getOpeningCandidates(board, maxCount);
        int best = std::numeric_limits<int>::min();
        for (size_t i = 0; i < oMoves.size(); ++i) {
            if (openingTimeExceeded(cancelFlag)) break;
            Board temp = board;
            int sx = scoreX;
            int so = scoreO;
            applyTempMove(temp, oMoves[i], CellState::O, sx, so);
            int val = evaluateForSeat(temp, Player::X, Player::O, sx, so, params, cancelFlag);
            if (val > best) best = val;
        }
        if (openingTimeExceeded(cancelFlag) && best == std::numeric_limits<int>::min()) {
            return 0;
        }
        if (best == std::numeric_limits<int>::min()) {
            best = evaluateForSeat(board, Player::X, Player::O, scoreX, scoreO, params, cancelFlag);
        }
        return best;
    }

    int evaluateSwap2OptionTakeX(const Board& board, int scoreX, int scoreO, const SearchParams& params, std::atomic<bool>* cancelFlag) const {
        return evaluateForSeat(board, Player::O, Player::X, scoreX, scoreO, params, cancelFlag);
    }

    int evaluateSwap2BestOptionForB(const Board& board, int scoreX, int scoreO, const SearchParams& params, std::atomic<bool>* cancelFlag) const {
        int takeX = evaluateSwap2OptionTakeX(board, scoreX, scoreO, params, cancelFlag);
        if (openingTimeExceeded(cancelFlag)) return takeX;
        int extraO = evaluateSwap2OptionExtraO(board, scoreX, scoreO, params, cancelFlag);
        if (openingTimeExceeded(cancelFlag)) return std::max(takeX, extraO);
        int placeTwo = evaluateSwap2BPlaceTwoValue(board, scoreX, scoreO, params, cancelFlag);
        return std::max(takeX, std::max(extraO, placeTwo));
    }

    int evaluateSwap2AfterASecondO(const Board& board, int scoreX, int scoreO, const SearchParams& params, std::atomic<bool>* cancelFlag) const {
        int maxCount = openingMaxCount(board, 12, 8, 6);
        DynamicArray<Coord> xMoves = getOpeningCandidates(board, maxCount);
        int best = std::numeric_limits<int>::max();
        for (size_t i = 0; i < xMoves.size(); ++i) {
            if (openingTimeExceeded(cancelFlag)) break;
            Board temp = board;
            int sx = scoreX;
            int so = scoreO;
            applyTempMove(temp, xMoves[i], CellState::X, sx, so);
            int val = evaluateSwap2BestOptionForB(temp, sx, so, params, cancelFlag);
            if (val < best) best = val;
        }
        if (openingTimeExceeded(cancelFlag) && best == std::numeric_limits<int>::max()) {
            return 0;
        }
        if (best == std::numeric_limits<int>::max()) {
            best = evaluateSwap2BestOptionForB(board, scoreX, scoreO, params, cancelFlag);
        }
        return best;
    }

    int evaluateSwap2AfterAFirstX(const Board& board, int scoreX, int scoreO, const SearchParams& params, std::atomic<bool>* cancelFlag) const {
        int maxCount = openingMaxCount(board, 12, 8, 6);
        DynamicArray<Coord> oMoves = getOpeningCandidates(board, maxCount);
        int best = std::numeric_limits<int>::max();
        for (size_t i = 0; i < oMoves.size(); ++i) {
            if (openingTimeExceeded(cancelFlag)) break;
            Board temp = board;
            int sx = scoreX;
            int so = scoreO;
            applyTempMove(temp, oMoves[i], CellState::O, sx, so);
            int val = evaluateSwap2AfterASecondO(temp, sx, so, params, cancelFlag);
            if (val < best) best = val;
        }
        if (openingTimeExceeded(cancelFlag) && best == std::numeric_limits<int>::max()) {
            return 0;
        }
        if (best == std::numeric_limits<int>::max()) {
            best = evaluateSwap2AfterASecondO(board, scoreX, scoreO, params, cancelFlag);
        }
        return best;
    }

    int evaluateSwap2PlusBestForBChosenSide(const Board& board, int scoreX, int scoreO, Player chosenSide, const SearchParams& params, std::atomic<bool>* cancelFlag) const {
        int maxCount = openingMaxCount(board, 12, 8, 6);
        DynamicArray<Coord> moves = getOpeningCandidates(board, maxCount);
        int best = std::numeric_limits<int>::min();
        CellState stone = playerToCell(chosenSide);
        Player toMove = (chosenSide == Player::X) ? Player::O : Player::X;
        for (size_t i = 0; i < moves.size(); ++i) {
            if (openingTimeExceeded(cancelFlag)) break;
            Board temp = board;
            int sx = scoreX;
            int so = scoreO;
            applyTempMove(temp, moves[i], stone, sx, so);
            int evalAX = evaluateForSeat(temp, toMove, Player::O, sx, so, params, cancelFlag);
            int evalAO = evaluateForSeat(temp, toMove, Player::X, sx, so, params, cancelFlag);
            int val = std::min(evalAX, evalAO);
            if (val > best) best = val;
        }
        if (openingTimeExceeded(cancelFlag) && best == std::numeric_limits<int>::min()) {
            return 0;
        }
        return best;
    }

    int evaluateSwap2PlusAfterASecondO(const Board& board, int scoreX, int scoreO, const SearchParams& params, std::atomic<bool>* cancelFlag) const {
        int bestX = evaluateSwap2PlusBestForBChosenSide(board, scoreX, scoreO, Player::X, params, cancelFlag);
        if (openingTimeExceeded(cancelFlag)) return bestX;
        int bestO = evaluateSwap2PlusBestForBChosenSide(board, scoreX, scoreO, Player::O, params, cancelFlag);
        return std::max(bestX, bestO);
    }

    int evaluateSwap2PlusAfterAFirstX(const Board& board, int scoreX, int scoreO, const SearchParams& params, std::atomic<bool>* cancelFlag) const {
        int maxCount = openingMaxCount(board, 12, 8, 6);
        DynamicArray<Coord> oMoves = getOpeningCandidates(board, maxCount);
        int best = std::numeric_limits<int>::max();
        for (size_t i = 0; i < oMoves.size(); ++i) {
            if (openingTimeExceeded(cancelFlag)) break;
            Board temp = board;
            int sx = scoreX;
            int so = scoreO;
            applyTempMove(temp, oMoves[i], CellState::O, sx, so);
            int val = evaluateSwap2PlusAfterASecondO(temp, sx, so, params, cancelFlag);
            if (val < best) best = val;
        }
        if (openingTimeExceeded(cancelFlag) && best == std::numeric_limits<int>::max()) {
            return 0;
        }
        if (best == std::numeric_limits<int>::max()) {
            best = evaluateSwap2PlusAfterASecondO(board, scoreX, scoreO, params, cancelFlag);
        }
        return best;
    }

    MoveStatus placeStoneForced(CellState stone, int row, int col) {
        if (gameOver_) return MoveStatus::GameAlreadyOver;

        int rows = board_.getRows();
        int cols = board_.getCols();
        if (row < 0 || row >= rows || col < 0 || col >= cols) {
            return MoveStatus::InvalidCell;
        }
        if (!board_.isEmpty(row, col)) {
            return MoveStatus::InvalidCell;
        }

        board_.set(row, col, stone);
        ++moveNumber_;

        if (mode_ == GameMode::LinesScore) {
            int newLines = board_.countLinesFromMove(Coord(row, col), stone);
            int delta = std::min(2, std::max(0, newLines));
            if (stone == CellState::X) {
                creditedLinesX_ += delta;
            } else if (stone == CellState::O) {
                creditedLinesO_ += delta;
            }
            if (board_.isFull()) {
                gameOver_ = true;
            }
        } else {
            if (board_.checkWin(stone) || board_.isFull()) {
                gameOver_ = true;
            }
        }
        return MoveStatus::Ok;
    }

    MoveStatus applyNormalMove(int row, int col) {
        int rows = board_.getRows();
        int cols = board_.getCols();
        int centerRow = rows / 2;
        int centerCol = cols / 2;

        if (mode_ == GameMode::LinesScore &&
            openingRule_ == OpeningRule::None &&
            currentPlayer_ == Player::X &&
            moveNumber_ == 0 &&
            (rows % 2 == 1) &&
            (cols % 2 == 1) &&
            row == centerRow && col == centerCol)
        {
            return MoveStatus::ForbiddenFirstCenter;
        }

        CellState stone = playerToCell(currentPlayer_);
        MoveStatus st = placeStoneForced(stone, row, col);
        if (st != MoveStatus::Ok) return st;

        if (openingRule_ == OpeningRule::PieSwap && moveNumber_ == 1) {
            openingPhase_ = OpeningPhase::Pie_OfferSwap;
            setNextToMove(Player::O);
            return MoveStatus::Ok;
        }

        if (!gameOver_) {
            Player next = (currentPlayer_ == Player::X) ? Player::O : Player::X;
            setNextToMove(next);
        }

        return MoveStatus::Ok;
    }

    
    MoveEvaluation chooseSwapAwareFirstMove(int depth, bool memo, AIStatistics& outStats, int timeLimitMs = -1, std::atomic<bool>* cancelFlag = nullptr) const {
        int total = board_.getRows() * board_.getCols();
        int maxCount = openingMaxCount(board_, 24, 12, 10);
        DynamicArray<Coord> moves = getOpeningCandidates(board_, maxCount);
        if (moves.empty()) {
            outStats.reset();
            return MoveEvaluation(Coord(-1, -1), 0);
        }

        SearchParams params = makeOpeningParams(depth, memo);

        int bestImbalance = std::numeric_limits<int>::max();
        int bestSum = std::numeric_limits<int>::max();
        MoveEvaluation bestMove(Coord(-1, -1), std::numeric_limits<int>::min());

        int effectiveLimitMs = timeLimitMs > 0 ? std::min(timeLimitMs, OPENING_TOTAL_LIMIT_MS) : OPENING_TOTAL_LIMIT_MS;
        OpeningTimeScope scope(*this, effectiveLimitMs);
        for (size_t i = 0; i < moves.size(); ++i) {
            if (openingTimeExceeded(cancelFlag)) break;
            Board temp = board_;
            int sx = creditedLinesX_;
            int so = creditedLinesO_;
            applyTempMove(temp, moves[i], CellState::X, sx, so);
            AnalysisResult resX = analysePosition(temp, Player::X, mode_, params, sx, so, cancelFlag);
            AnalysisResult resO = analysePosition(temp, Player::O, mode_, params, sx, so, cancelFlag);
            int absX = std::abs(resX.bestScore);
            int absO = std::abs(resO.bestScore);
            int imbalance = std::max(absX, absO);
            int sum = absX + absO;
            if (imbalance < bestImbalance || (imbalance == bestImbalance && sum < bestSum)) {
                bestImbalance = imbalance;
                bestSum = sum;
                bestMove = MoveEvaluation(moves[i], -imbalance);
            }
        }

        outStats.reset();
        if (openingTimeLimitMs_ > 0) {
            auto now = std::chrono::steady_clock::now();
            outStats.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - openingStartTime_).count();
        }
        outStats.completedDepth = 1;
        return bestMove;
    }
};
