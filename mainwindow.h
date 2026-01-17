#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QVector>
#include <QSet>
#include <QPoint>
#include <QKeyEvent>
#include <QStringList>
#include <QMessageBox>
#include <QLabel>
#include <QFuture>
#include <QFutureWatcher>
#include <atomic>

#include "GameController.hpp"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    enum class GameType {
        HumanVsAI,
        HumanVsHuman,
        AIVsAI
    };

    enum class AIVsAISpeed {
        Step,
        Automatic
    };

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private:
    
    void applyModernStyle();
    void rebuildBoard();          
    void refreshBoardView();
    void updateStatusLabels();
    void showGameOverMessage();
    void handleBoardClick(int row, int col);
    void autoPlayAiIfNeeded();
    void setSettingsEnabled(bool enabled);
    QSet<QPoint> findWinningCells(CellState player) const;
    GameType gameTypeFromUI() const;
    AIVsAISpeed aiVsAiSpeedFromUI() const;
    void updateGameTypeUiState();
    void startAutoAIVsAIGame(bool autoMode);
    void startStepAIVsAIGame();
    void endCurrentGameEarly();
    void onAiVsAiFinished();
    void onAiVsAiStepFinished();
    QString coordToHuman(int row, int col) const;
    void updateAiMoveLabel(Player mover, const Coord& move, const QString& scoreText = QString());
    void setBigInfo(const QString& text);
    void refreshBigInfoDisplay();
    void addRecentMove(Player mover, const Coord& move, const QString& scoreText);
    void setLastMove(const Coord& mv);
    void startAsyncHint(Player p, bool isEvaluation, const QString& moverLabel, const Coord& evalMove);
    void updateGameStateLabel(bool running);
    void showPopupMessage(const QString& text, QMessageBox::Icon icon = QMessageBox::Warning);
    void startAsyncAiMove(Player aiPlayer);
    void onAiSearchFinished();
    bool isPlayerAi(Player p) const;
    bool isSeatAi(Seat seat) const;
    bool isCurrentSeatAi() const;
    bool isMoveValid(const MoveEvaluation& mv) const;
    void stopAllAi(bool resetCancelFlag = false);
    void applyEnginePresetFromUi();
    void applyAdvancedSettingsFromUi();
    void syncAdvancedSettingsUi(EnginePreset preset);
    void syncHumanSideFromSeat();
    bool resolveOpeningChoiceIfNeeded();
    void updateOpeningUiState();
    OpeningRule openingRuleFromUi() const;
    void showOpeningRuleInfoDialog();
    QString formatMoveLine(Player mover, const Coord& move, const QString& scoreText) const;
    QString seatLabel(Seat seat) const;
    QString sideLabel(Player side) const;
    void updateTurnIndicator();

private slots:
    void onNewGameClicked();
    void onBestMoveClicked();
    void onAIVsAIStepClicked();
    void onModeChanged(int index);
    void onBoardSizeChanged(int newValue);  
    void onGameTypeChanged(int index);
    void onAivAiSpeedChanged(int index);
    void onAutoAiToggled(Qt::CheckState state);
    void onEndGameClicked();
    void onHintClicked();
    void onShowStatsClicked();
    void onCancelAiClicked();
    void onDynamicDepthToggled(Qt::CheckState state);
    void onTimeLimitChanged(int seconds);
    void onHintFinished();
    void onEnginePresetChanged(int index);
    void onAdvancedToggled(bool checked);
    void onMoveGenModeChanged(int index);
    void onUseLmrToggled(Qt::CheckState state);
    void onUseExtensionsToggled(Qt::CheckState state);
    void onOpeningRuleChanged(int index);

private:
    class AiSearchResult {
    public:
        MoveEvaluation result;
        MoveEvaluation best;
        AIStatistics stats;
        bool cancelled = false;
    };

    enum class HintTask {
        None,
        Hint,
        HumanEval
    };

    Ui::MainWindow *ui;
    GameController  controller_;

    int currentRows_      = 3;
    int currentCols_      = 3;
    int currentWinLength_ = 3;

    
    QVector<QVector<QPushButton*>> boardButtons_;

    
    bool aiVsAiStepMode_ = false;
    bool aiVsAiRunning_  = false;
    bool aiVsAiStepBusy_ = false;
    int  aiStepDepthX_   = 0;
    int  aiStepDepthO_   = 0;
    bool aiStepMemoX_    = true;
    bool aiStepMemoO_    = true;
    int  aiStepTimeLimitMsX_ = -1;
    int  aiStepTimeLimitMsO_ = -1;
    class AIVsAIStepResult {
    public:
        bool ok = false;
        AIVsAIMoveInfo info;
    };

    
    QString cellStyleBase_;
    QString cellStyleHighlightX_;
    QString cellStyleHighlightO_;

    
    bool settingsLocked_ = false;

    
    GameType currentGameType_ = GameType::HumanVsAI;
    AIVsAISpeed currentAIVsAISpeed_ = AIVsAISpeed::Automatic;
    bool autoAiEnabled_ = true;
    bool humanIsX_ = true;
    bool dynamicDepthMode_ = false;
    int  timeLimitMs_ = 8000;
    QStringList recentMoves_;
    QString hintLine_;
    QString detailedStats_;
    bool aiSearchInProgress_ = false;
    bool hintInProgress_ = false;
    Player aiSearchPlayer_ = Player::O;
    Player hintPlayer_ = Player::O;
    Player evalMoverSide_ = Player::X;
    HintTask currentHintTask_ = HintTask::None;
    Coord evalMoveForHint_ = Coord(-1, -1);
    QString evalMoverLabel_;
    std::atomic<bool> aiCancelFlag_{false};
    std::atomic<bool> hintCancelFlag_{false};
    Coord lastMove_ = Coord(-1, -1);
    QFutureWatcher<AiSearchResult> aiWatcher_;
    QFutureWatcher<AiSearchResult> hintWatcher_;
    QFutureWatcher<AIVsAIResult> aiVsAiWatcher_;
    QFutureWatcher<AIVsAIStepResult> aiVsAiStepWatcher_;
    bool aiSearchCanceled_ = false;
    bool hintCanceled_ = false;
    std::atomic<int> sessionId_{0};
    int aiSearchTicket_ = 0;
    int hintTicket_ = 0;
    int aiVsAiTicket_ = 0;
    int aiVsAiStepTicket_ = 0;
    OpeningRule openingRule_ = OpeningRule::None;
    Seat humanSeat_ = Seat::A;
    QLabel* turnIndicatorLabel_ = nullptr;
};

#endif
