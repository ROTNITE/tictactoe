#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QApplication>
#include <QThread>
#include <QFont>
#include <QLayoutItem>
#include <QLabel>
#include <QChar>
#include <QDialog>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <limits>
#include <QtConcurrent/QtConcurrent>
#include <QSignalBlocker>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , controller_(3, 3, 3, GameMode::LinesScore) 
{
    ui->setupUi(this);

    applyModernStyle();

    
    ui->comboMode->clear();
    ui->comboMode->addItem("По линиям");    
    ui->comboMode->addItem("Классический"); 
    ui->comboMode->setCurrentIndex(0);

    
    ui->comboGameType->clear();
    ui->comboGameType->addItem("ИИ против игрока");    
    ui->comboGameType->addItem("Игрок против игрока"); 
    ui->comboGameType->addItem("ИИ против ИИ");        
    ui->comboGameType->setCurrentIndex(0);

    
    ui->comboPlayerSide->setCurrentIndex(0); 

    
    ui->comboAIVsAISpeed->clear();
    ui->comboAIVsAISpeed->addItem("Пошагово");     
    ui->comboAIVsAISpeed->addItem("Автоматически"); 
    ui->comboAIVsAISpeed->setCurrentIndex(1);

    
    ui->spinBoardRows->setMinimum(3);
    ui->spinBoardRows->setMaximum(15);
    ui->spinBoardRows->setValue(3);

    ui->spinBoardCols->setMinimum(3);
    ui->spinBoardCols->setMaximum(15);
    ui->spinBoardCols->setValue(3);

    ui->spinWinLength->setMinimum(3);
    ui->spinWinLength->setMaximum(3); 
    ui->spinWinLength->setValue(3);

    
    ui->spinDepth->setMinimum(1);
    ui->spinDepth->setMaximum(15);
    ui->spinDepth->setValue(9);
    ui->checkDynamicDepth->setChecked(false);
    ui->spinTimeLimit->setMinimum(1);
    ui->spinTimeLimit->setMaximum(20);
    ui->spinTimeLimit->setValue(10);
    ui->checkMemo->setChecked(true);
    ui->checkHintDynamic->setChecked(false);
    ui->spinHintTime->setMinimum(1);
    ui->spinHintTime->setMaximum(20);
    ui->spinHintTime->setValue(10);

    
    ui->checkAutoAi->setChecked(true);

    
    connect(ui->btnNewGame,   &QPushButton::clicked, this,             &MainWindow::onNewGameClicked);
    connect(ui->btnEndGame,   &QPushButton::clicked, this,             &MainWindow::onEndGameClicked);
    connect(ui->btnShowStats, &QPushButton::clicked, this,             &MainWindow::onShowStatsClicked);
    connect(ui->btnBestMove,  &QPushButton::clicked, this,             &MainWindow::onBestMoveClicked);
    connect(ui->btnCancelAi,  &QPushButton::clicked, this,             &MainWindow::onCancelAiClicked);
    connect(ui->btnAIVsAIStep,&QPushButton::clicked, this,             &MainWindow::onAIVsAIStepClicked);
    connect(ui->btnHint,      &QPushButton::clicked, this,             &MainWindow::onHintClicked);

    connect(ui->comboMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onModeChanged);
    connect(ui->comboGameType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onGameTypeChanged);
    connect(ui->comboAIVsAISpeed, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onAivAiSpeedChanged);
    connect(ui->comboPlayerSide, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onGameTypeChanged);
    connect(ui->spinTimeLimit, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onTimeLimitChanged);
    connect(ui->checkHintDynamic, &QCheckBox::checkStateChanged, this, &MainWindow::onHintDynamicToggled);
    connect(ui->spinHintTime, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onHintTimeChanged);
    connect(ui->checkAutoAi, &QCheckBox::checkStateChanged, this, &MainWindow::onAutoAiToggled);
    connect(ui->checkEvalHuman, &QCheckBox::checkStateChanged, this, &MainWindow::onEvalHumanToggled);
    connect(ui->checkDynamicDepth, &QCheckBox::checkStateChanged, this, &MainWindow::onDynamicDepthToggled);
    connect(ui->checkSwapRule, &QCheckBox::checkStateChanged, this, &MainWindow::onSwapRuleToggled);
    connect(&aiWatcher_, &QFutureWatcher<AiSearchResult>::finished, this, &MainWindow::onAiSearchFinished);
    connect(&hintWatcher_, &QFutureWatcher<AiSearchResult>::finished, this, &MainWindow::onHintFinished);
    connect(&aiVsAiWatcher_, &QFutureWatcher<AIVsAIResult>::finished, this, &MainWindow::onAiVsAiFinished);
    connect(&aiVsAiStepWatcher_, &QFutureWatcher<AIVsAIStepResult>::finished, this, &MainWindow::onAiVsAiStepFinished);

    
    connect(ui->spinBoardRows, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onBoardSizeChanged);
    connect(ui->spinBoardCols, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onBoardSizeChanged);

    
    onBoardSizeChanged(0);

    currentRows_      = ui->spinBoardRows->value();
    currentCols_      = ui->spinBoardCols->value();
    currentWinLength_ = ui->spinWinLength->value();
    currentGameType_    = GameType::HumanVsAI;
    currentAIVsAISpeed_ = AIVsAISpeed::Automatic;
    autoAiEnabled_      = true;
    humanIsX_           = (ui->comboPlayerSide->currentIndex() == 0);
    evaluateHumanMoves_ = ui->checkEvalHuman->isChecked();
    dynamicDepthMode_   = ui->checkDynamicDepth->isChecked();
    dynamicHintMode_    = ui->checkHintDynamic->isChecked();
    timeLimitMs_        = ui->spinTimeLimit->value() * 1000;
    hintTimeLimitMs_    = ui->spinHintTime->value() * 1000;
    swapRuleEnabled_    = ui->checkSwapRule->isChecked();
    swapPromptShown_    = false;
    hintInProgress_     = false;
    hintCanceled_       = false;

    
    rebuildBoard();
    refreshBoardView();
    updateStatusLabels();
    recentMoves_.clear();
    hintLine_.clear();
    refreshBigInfoDisplay();
    ui->labelAiMove->setText("—");
    updateGameStateLabel(false);
    ui->labelStatus->setText("Игра не идёт");
    ui->groupBoxInfo->setVisible(false);
    detailedStats_.clear();
    ui->btnCancelAi->setEnabled(false);
    updateGameTypeUiState();
    setSettingsEnabled(true);
    updateSwapUiState();
}

MainWindow::~MainWindow()
{
    sessionId_.fetch_add(1, std::memory_order_relaxed);
    stopAllAi(false);
    delete ui;
}

void MainWindow::stopAllAi(bool resetCancelFlag)
{
    controller_.setOnAIMoveCallback(nullptr);
    aiCancelFlag_.store(true, std::memory_order_relaxed);
    hintCancelFlag_.store(true, std::memory_order_relaxed);
    if (aiSearchInProgress_) {
        aiWatcher_.waitForFinished();
        aiSearchInProgress_ = false;
        aiSearchCanceled_ = false;
    }
    if (hintInProgress_) {
        hintWatcher_.waitForFinished();
        hintInProgress_ = false;
        hintCanceled_ = false;
        currentHintTask_ = HintTask::None;
        evalMoveForHint_ = Coord(-1, -1);
        evalMoverLabel_.clear();
    }
    if (aiVsAiRunning_) {
        aiVsAiWatcher_.waitForFinished();
        aiVsAiRunning_ = false;
    }
    if (aiVsAiStepBusy_) {
        aiVsAiStepWatcher_.waitForFinished();
        aiVsAiStepBusy_ = false;
    }
    aiVsAiStepMode_ = false;
    ui->btnCancelAi->setEnabled(false);
    if (resetCancelFlag) {
        aiCancelFlag_.store(false, std::memory_order_relaxed);
        hintCancelFlag_.store(false, std::memory_order_relaxed);
        aiSearchTicket_ = -1;
        hintTicket_ = -1;
        aiVsAiTicket_ = -1;
        aiVsAiStepTicket_ = -1;
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Alt && !event->isAutoRepeat()) {
        refreshBoardView();
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Alt && !event->isAutoRepeat()) {
        refreshBoardView();
    }
    QMainWindow::keyReleaseEvent(event);
}


void MainWindow::applyModernStyle()
{
    QString style = R"(
        QMainWindow {
            background-color: #020617;
        }

        QWidget#boardWidget {
            background-color: #020617;
        }

        QGroupBox {
            border: 1px solid #1f2937;
            border-radius: 10px;
            margin-top: 12px;
            background-color: rgba(15, 23, 42, 0.9);
        }

        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            padding: 4px 10px;
            color: #e5e7eb;
            font-weight: 600;
        }

        QLabel {
            color: #e5e7eb;
        }

        QLabel#labelStatus {
            color: #ffedd5;
            font-size: 18px;
            font-weight: 800;
            font-style: italic;
        }

        QGroupBox#groupBoxInfo {
            border: 1px solid #22d3ee;
            background-color: rgba(30, 41, 59, 0.95);
        }

        QGroupBox#groupBoxInfo QLabel {
            color: #e0f2fe;
            font-size: 12px;
        }

        QLabel#labelCurrentPlayer,
        QLabel#labelLinesX,
        QLabel#labelLinesO,
        QLabel#labelScore {
            color: #f8fafc;
            font-weight: 700;
            font-size: 14px;
        }

        QLabel#labelAiMove {
            color: #f1f5f9;
            font-size: 18px;
            font-weight: 800;
        }
        QLabel#labelAiMoveTitle {
            color: #bae6fd;
            font-size: 13px;
        }

        QGroupBox#groupBoxBigInfo,
        QGroupBox#groupBoxStatus {
            border: 1px solid #f97316;
            background-color: rgba(251, 146, 60, 0.08);
        }
        QGroupBox#groupBoxBigInfo QLabel,
        QGroupBox#groupBoxStatus QLabel {
            color: #ffedd5;
        }
        QLabel#labelBigInfo {
            font-size: 22px;
            font-weight: 800;
        }

        QComboBox {
            background-color: #020617;
            border: 1px solid #334155;
            border-radius: 6px;
            padding: 4px 8px;
            color: #e5e7eb;
        }
        QComboBox::drop-down {
            border: none;
        }

        QSpinBox {
            background-color: #020617;
            border: 1px solid #334155;
            border-radius: 6px;
            padding: 2px 8px;
            color: #e5e7eb;
        }

        QCheckBox {
            color: #e5e7eb;
        }

        QCheckBox::indicator {
            width: 16px;
            height: 16px;
        }
        QCheckBox::indicator:unchecked {
            border: 1px solid #334155;
            background-color: #020617;
            border-radius: 4px;
        }
        QCheckBox::indicator:checked {
            border: 1px solid #22c55e;
            background-color: #22c55e;
            border-radius: 4px;
        }

        QPushButton {
            background-color: #0f172a;
            color: #e5e7eb;
            border-radius: 8px;
            padding: 6px 14px;
            border: 1px solid #1f2937;
            font-weight: 600;
        }
        QPushButton:hover {
            background-color: #1f2937;
        }
        QPushButton:pressed {
            background-color: #111827;
        }

        QPushButton#btnNewGame,
        QPushButton#btnBestMove {
            background-color: #2563eb;
            border: 1px solid #1d4ed8;
        }
        QPushButton#btnNewGame:hover,
        QPushButton#btnBestMove:hover {
            background-color: #1d4ed8;
        }
        QPushButton#btnNewGame:pressed,
        QPushButton#btnBestMove:pressed {
            background-color: #1e40af;
        }

        QPushButton#btnAIVsAIStep {
            background-color: #10b981;
            border: 1px solid #059669;
        }
        QPushButton#btnAIVsAIStep:hover {
            background-color: #059669;
        }
        QPushButton#btnAIVsAIStep:pressed {
            background-color: #047857;
        }

        QPushButton#btnEndGame {
            background-color: #ef4444;
            border: 1px solid #dc2626;
        }
        QPushButton#btnEndGame:hover {
            background-color: #dc2626;
        }
        QPushButton#btnEndGame:pressed {
            background-color: #b91c1c;
        }

        QStatusBar {
            background-color: #020617;
            color: #9ca3af;
        }

        QMenuBar {
            background-color: #020617;
            color: #e5e7eb;
        }
        QMenuBar::item:selected {
            background-color: #1f2937;
        }
    )";

    this->setStyleSheet(style);

    QFont f = this->font();
    f.setPointSize(11);
    this->setFont(f);
}


void MainWindow::onBoardSizeChanged(int newValue)
{
    Q_UNUSED(newValue);

    int rows = ui->spinBoardRows->value();
    int cols = ui->spinBoardCols->value();

    
    int maxLen = (rows < cols ? rows : cols);
    if (maxLen < 3) maxLen = 3;

    ui->spinWinLength->setMaximum(maxLen);

    if (ui->spinWinLength->minimum() < 3) {
        ui->spinWinLength->setMinimum(3);
    }

    int winLen = ui->spinWinLength->value();
    if (winLen < 3) {
        winLen = 3;
    } else if (winLen > maxLen) {
        winLen = maxLen;
    }
    ui->spinWinLength->setValue(winLen);
}


void MainWindow::rebuildBoard()
{
    int rows = currentRows_;
    int cols = currentCols_;

    QGridLayout *grid = ui->gridLayoutBoard;
    if (!grid) return;

    
    while (QLayoutItem *item = grid->takeAt(0)) {
        if (QWidget *w = item->widget()) {
            w->deleteLater();
        }
        delete item;
    }

    int minCell = 36;
    int minWidth  = (cols + 1) * minCell;
    int minHeight = (rows + 1) * minCell;
    ui->boardWidget->setMinimumSize(minWidth, minHeight);

    
    boardButtons_.clear();
    boardButtons_.resize(rows);
    for (int r = 0; r < rows; ++r) {
        boardButtons_[r].resize(cols);
    }

    
    auto colName = [](int c) -> QString {
        QString name;
        int x = c;
        while (true) {
            int rem = x % 26;
            name.prepend(QChar('A' + rem));
            x = x / 26 - 1;
            if (x < 0) break;
        }
        return name;
    };

    QLabel *corner = new QLabel(this);
    corner->setText("");
    corner->setAlignment(Qt::AlignCenter);
    grid->addWidget(corner, 0, 0);

    for (int c = 0; c < cols; ++c) {
        QLabel *lbl = new QLabel(this);
        lbl->setText(colName(c));
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet("color: #38bdf8; font-weight:700;");
        grid->addWidget(lbl, 0, c + 1);
    }
    for (int r = 0; r < rows; ++r) {
        QLabel *lbl = new QLabel(this);
        lbl->setText(QString::number(r + 1));
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet("color: #38bdf8; font-weight:700;");
        grid->addWidget(lbl, r + 1, 0);
    }

    
    cellStyleBase_ = R"(
        QPushButton {
            background-color: #020617;
            border-radius: 12px;
            border: 2px solid #0ea5e9;
            color: #e5e7eb;
        }
        QPushButton:hover {
            background-color: #0b1120;
            border-color: #22d3ee;
        }
        QPushButton:pressed {
            background-color: #020617;
            border-color: #38bdf8;
        }
    )";

    cellStyleHighlightX_ = R"(
        QPushButton {
            background-color: #1e293b;
            border-radius: 12px;
            border: 3px solid #f472b6;
            color: #f8fafc;
        }
    )";

    cellStyleHighlightO_ = R"(
        QPushButton {
            background-color: #1e293b;
            border-radius: 12px;
            border: 3px solid #22c55e;
            color: #f8fafc;
        }
    )";

    int maxDim = (rows > cols ? rows : cols);

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            QPushButton *btn = new QPushButton(this);
            btn->setText("");
            btn->setMinimumSize(36, 36);
            btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

            QFont f = btn->font();
            if (maxDim <= 3)        f.setPointSize(30);
            else if (maxDim <= 5)   f.setPointSize(24);
            else if (maxDim <= 10)  f.setPointSize(18);
            else                    f.setPointSize(14);
            f.setBold(true);
            btn->setFont(f);

            btn->setStyleSheet(cellStyleBase_);

            grid->addWidget(btn, r + 1, c + 1);
            boardButtons_[r][c] = btn;

            
            connect(btn, &QPushButton::clicked, this, [this, r, c]() {handleBoardClick(r, c); });
        }
    }
}




void MainWindow::refreshBoardView()
{
    const Board& b = controller_.board();
    int rows = b.getRows();
    int cols = b.getCols();

    if (rows != boardButtons_.size())
        return;
    for (int r = 0; r < rows; ++r) {
        if (r >= boardButtons_.size()) return;
        if (cols != boardButtons_[r].size()) return;
    }

    bool altHeld = QApplication::keyboardModifiers().testFlag(Qt::AltModifier);
    QSet<QPoint> winX;
    QSet<QPoint> winO;
    if (altHeld) {
        winX = findWinningCells(CellState::X);
        winO = findWinningCells(CellState::O);
    }

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            CellState cell = b.get(r, c);
            QString text;
            if (cell == CellState::X)      text = "X";
            else if (cell == CellState::O) text = "O";
            else                           text = "";

            QPushButton* btn = boardButtons_[r][c];
            if (!btn) continue;

            btn->setText(text);

            QString styleToApply = cellStyleBase_;
            if (altHeld) {
                QPoint pt(c, r);
                if (winX.contains(pt)) {
                    styleToApply = cellStyleHighlightX_;
                } else if (winO.contains(pt)) {
                    styleToApply = cellStyleHighlightO_;
                }
            } else if (lastMove_.row() == r && lastMove_.col() == c && cell != CellState::Empty) {
                
                styleToApply = (cell == CellState::X) ? cellStyleHighlightX_ : cellStyleHighlightO_;
            }
            btn->setStyleSheet(styleToApply);
        }
    }
}

QSet<QPoint> MainWindow::findWinningCells(CellState player) const
{
    QSet<QPoint> result;
    if (player == CellState::Empty) {
        return result;
    }

    const Board& b = controller_.board();
    int rows   = b.getRows();
    int cols   = b.getCols();
    int winLen = b.getWinLength();

    auto tryAddLine = [&](int startRow, int startCol, int dRow, int dCol) {
        for (int k = 0; k < winLen; ++k) {
            if (b.get(startRow + k * dRow, startCol + k * dCol) != player) {
                return;
            }
        }
        for (int k = 0; k < winLen; ++k) {
            result.insert(QPoint(startCol + k * dCol, startRow + k * dRow));
        }
    };

    
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col <= cols - winLen; ++col) {
            tryAddLine(row, col, 0, 1);
        }
    }

    
    for (int col = 0; col < cols; ++col) {
        for (int row = 0; row <= rows - winLen; ++row) {
            tryAddLine(row, col, 1, 0);
        }
    }

    
    for (int row = 0; row <= rows - winLen; ++row) {
        for (int col = 0; col <= cols - winLen; ++col) {
            tryAddLine(row, col, 1, 1);
        }
    }

    
    for (int row = 0; row <= rows - winLen; ++row) {
        for (int col = winLen - 1; col < cols; ++col) {
            tryAddLine(row, col, 1, -1);
        }
    }

    return result;
}

void MainWindow::setSettingsEnabled(bool enabled)
{
    settingsLocked_ = !enabled;
    ui->comboMode->setEnabled(enabled);
    ui->spinBoardRows->setEnabled(enabled);
    ui->spinBoardCols->setEnabled(enabled);
    ui->spinWinLength->setEnabled(enabled);
    ui->spinDepth->setEnabled(enabled);
    ui->checkMemo->setEnabled(enabled);
    ui->checkEvalHuman->setEnabled(enabled);
    ui->checkDynamicDepth->setEnabled(enabled);
    
    ui->spinTimeLimit->setEnabled(ui->checkDynamicDepth->isChecked());
    ui->checkHintDynamic->setEnabled(enabled);
    ui->spinHintTime->setEnabled(ui->checkHintDynamic->isChecked());
    ui->checkSwapRule->setEnabled(enabled && controller_.mode() == GameMode::LinesScore);
    
    ui->checkAutoAi->setEnabled(currentGameType_ == GameType::HumanVsAI);
    ui->comboGameType->setEnabled(enabled);
    ui->comboAIVsAISpeed->setEnabled(enabled && currentGameType_ == GameType::AIVsAI);
    ui->btnHint->setEnabled(!controller_.isGameOver() && currentGameType_ != GameType::AIVsAI);
    if (hintInProgress_) {
        ui->btnHint->setEnabled(false);
    }
    ui->comboPlayerSide->setEnabled(enabled && currentGameType_ == GameType::HumanVsAI);
    updateGameTypeUiState();
    updateSwapUiState();
}

MainWindow::GameType MainWindow::gameTypeFromUI() const
{
    int idx = ui->comboGameType->currentIndex();
    if (idx == 1) return GameType::HumanVsHuman;
    if (idx == 2) return GameType::AIVsAI;
    return GameType::HumanVsAI;
}

MainWindow::AIVsAISpeed MainWindow::aiVsAiSpeedFromUI() const
{
    int idx = ui->comboAIVsAISpeed->currentIndex();
    if (idx == 0) return AIVsAISpeed::Step;
    return AIVsAISpeed::Automatic;
}

bool MainWindow::isPlayerAi(Player p) const
{
    if (currentGameType_ == GameType::HumanVsHuman) return false;
    if (currentGameType_ == GameType::HumanVsAI) {
        return humanIsX_ ? (p == Player::O) : (p == Player::X);
    }
    return true; 
}

void MainWindow::updateGameTypeUiState()
{
    currentGameType_ = gameTypeFromUI();
    currentAIVsAISpeed_ = aiVsAiSpeedFromUI();

    bool aiGame  = (currentGameType_ == GameType::HumanVsAI);
    bool aiVsAi  = (currentGameType_ == GameType::AIVsAI);
    bool unlocked = !settingsLocked_;
    bool busy = aiSearchInProgress_ || aiVsAiRunning_ || aiVsAiStepBusy_ || hintInProgress_;

    if (!aiGame) {
        autoAiEnabled_ = false;
        ui->checkAutoAi->setChecked(false);
    } else {
        autoAiEnabled_ = ui->checkAutoAi->isChecked();
    }

    ui->checkAutoAi->setEnabled(aiGame && !controller_.isGameOver());
    ui->comboAIVsAISpeed->setEnabled(unlocked && aiVsAi);

    ui->btnBestMove->setEnabled(aiGame && !controller_.isGameOver());
    ui->btnAIVsAIStep->setEnabled(aiVsAi && currentAIVsAISpeed_ == AIVsAISpeed::Step && !controller_.isGameOver() && !aiVsAiStepBusy_);
    ui->btnHint->setEnabled(!aiVsAi && !controller_.isGameOver() && !hintInProgress_);
    ui->btnCancelAi->setEnabled(aiSearchInProgress_ || aiVsAiRunning_ || aiVsAiStepBusy_ || hintInProgress_);
    ui->btnNewGame->setEnabled(!busy);
    
    ui->btnEndGame->setEnabled(!controller_.isGameOver() || busy);
    ui->spinTimeLimit->setEnabled(ui->checkDynamicDepth->isChecked());
    bool hintDynamicAllowed = !aiVsAi;
    if (aiVsAi && ui->checkHintDynamic->isChecked()) {
        ui->checkHintDynamic->setChecked(false);
        dynamicHintMode_ = false;
    }
    ui->checkHintDynamic->setEnabled(unlocked && hintDynamicAllowed);
    ui->spinHintTime->setEnabled(hintDynamicAllowed && ui->checkHintDynamic->isChecked());
    updateSwapUiState();
}

void MainWindow::updateSwapUiState()
{
    bool ruleActive = controller_.isSwapRuleEnabled();
    bool available = ruleActive && controller_.isSwapAvailable();

    
    if (available && hintLine_.isEmpty() && currentGameType_ != GameType::AIVsAI) {
        setBigInfo("Правило обмена: второй игрок может забрать первый ход.");
    }
}

bool MainWindow::applySwapIfPossible(const QString& initiator, bool silent, bool alreadyApplied)
{
    if (controller_.mode() != GameMode::LinesScore) {
        return false;
    }
    if (!alreadyApplied && (!controller_.isSwapRuleEnabled() || !controller_.isSwapAvailable())) {
        return false;
    }
    if (!alreadyApplied) {
        if (!controller_.applySwapRule()) {
            return false;
        }
    }
    swapPromptShown_ = true;

    if (currentGameType_ == GameType::HumanVsAI) {
        humanIsX_ = !humanIsX_;
        QSignalBlocker blocker(ui->comboPlayerSide);
        ui->comboPlayerSide->setCurrentIndex(humanIsX_ ? 0 : 1);
    }

    if (!silent) {
        QString msg;
        if (currentGameType_ == GameType::HumanVsAI) {
            msg = QString("%1 применил правило обмена. Вы теперь играете за %2, ИИ за %3.")
                      .arg(initiator)
                      .arg(humanIsX_ ? "X" : "O")
                      .arg(humanIsX_ ? "O" : "X");
        } else if (currentGameType_ == GameType::HumanVsHuman) {
            msg = QString("%1 применил правило обмена. Игроки поменялись сторонами.")
                      .arg(initiator);
        } else {
            msg = QString("%1 применил правило обмена.").arg(initiator);
        }
        setBigInfo(msg);
        if (currentGameType_ == GameType::HumanVsAI) {
            QMessageBox::information(this, "Правило обмена", msg);
        }
    }

    refreshBoardView();
    updateStatusLabels();
    updateGameTypeUiState();
    updateSwapUiState();
    autoPlayAiIfNeeded();
    return true;
}

void MainWindow::startAutoAIVsAIGame(bool autoMode)
{
    if (aiVsAiRunning_) return;
    aiVsAiStepMode_ = false;
    const int ticket = sessionId_.load(std::memory_order_relaxed);
    aiVsAiTicket_ = ticket;
    aiCancelFlag_.store(false, std::memory_order_relaxed);
    aiVsAiRunning_ = true;
    setSettingsEnabled(false);
    updateGameTypeUiState();
    recentMoves_.clear();
    hintLine_.clear();
    refreshBigInfoDisplay();
    ui->labelAiMove->setText("-");
    ui->labelStatus->setText("Игра идёт (AI vs AI автоматически).");

    int  depth = ui->spinDepth->value();
    if (dynamicDepthMode_) {
        depth = std::max(depth, 12);
    }
    bool memo  = ui->checkMemo->isChecked();
    int sleepMs = autoMode ? 60 : 600;

    controller_.setOnAIMoveCallback([this, sleepMs, ticket](const Board& board, Player who, const MoveEvaluation& eval, const AIStatistics&) {if (ticket != sessionId_.load(std::memory_order_relaxed)) return; if (aiCancelFlag_.load(std::memory_order_relaxed)) return; QString whoText = QString("ИИ (%1)").arg(who == Player::X ? "X" : "O"); Coord mv = eval.move; int score = eval.score;  const bool moveInRange = (mv.row() >= 0 && mv.col() >= 0 && mv.row() < board.getRows() && mv.col() < board.getCols());  if (!moveInRange) {QMetaObject::invokeMethod(this, [this, whoText, ticket]() {if (ticket != sessionId_.load(std::memory_order_relaxed)) return; setBigInfo(QString("%1 применил правило обмена.").arg(whoText)); refreshBoardView(); updateStatusLabels(); }, Qt::QueuedConnection); if (sleepMs > 0) {QThread::msleep(static_cast<unsigned long>(sleepMs)); } return; }  QMetaObject::invokeMethod(this, [this, whoText, mv, score, ticket]() {if (ticket != sessionId_.load(std::memory_order_relaxed)) return; setLastMove(mv); updateAiMoveLabel(whoText, mv, QString::number(score)); refreshBoardView(); updateStatusLabels(); }, Qt::QueuedConnection); if (sleepMs > 0) {QThread::msleep(static_cast<unsigned long>(sleepMs)); } });

    int timeLimit = dynamicDepthMode_ ? ui->spinTimeLimit->value() * 1000 : -1;

    aiVsAiWatcher_.setFuture(QtConcurrent::run([this, depth, memo, timeLimit, ticket]() {AIVsAIResult res = controller_.runAIVsAIGame(depth, memo, depth, memo, std::string(), &aiCancelFlag_, timeLimit, timeLimit); if (ticket != sessionId_.load(std::memory_order_relaxed)) {res.moves.clear(); } return res; }));
    ui->btnCancelAi->setEnabled(true);
}

void MainWindow::startStepAIVsAIGame()
{
    setSettingsEnabled(false);
    updateGameTypeUiState();

    int  depth = ui->spinDepth->value();
    if (dynamicDepthMode_) {
        depth = std::max(depth, 12);
    }
    bool memo  = ui->checkMemo->isChecked();
    aiVsAiStepTicket_ = sessionId_.load(std::memory_order_relaxed);
    aiStepDepthX_ = depth;
    aiStepDepthO_ = depth;
    aiStepMemoX_  = memo;
    aiStepMemoO_  = memo;
    int timeLimit = dynamicDepthMode_ ? ui->spinTimeLimit->value() * 1000 : -1;
    aiStepTimeLimitMsX_ = timeLimit;
    aiStepTimeLimitMsO_ = timeLimit;
    aiVsAiStepMode_ = true;

    refreshBoardView();
    updateStatusLabels();
    recentMoves_.clear();
    hintLine_.clear();
    refreshBigInfoDisplay();
    ui->labelAiMove->setText("-");
    ui->labelStatus->setText("Игра идёт");
}

void MainWindow::onAiVsAiFinished()
{
    if (aiVsAiTicket_ != sessionId_.load(std::memory_order_relaxed)) return;
    aiVsAiRunning_ = false;
    controller_.setOnAIMoveCallback(nullptr);
    ui->btnCancelAi->setEnabled(false);

    AIVsAIResult result = aiVsAiWatcher_.result();
    bool cancelled = aiCancelFlag_.load(std::memory_order_relaxed);

    refreshBoardView();
    updateStatusLabels();

    QString modeText = (currentAIVsAISpeed_ == AIVsAISpeed::Automatic)
        ? "Автоматически" : "Пошагово";

    ui->labelStatus->setText(cancelled ? "Игра остановлена" : "Игра не идёт");
    detailedStats_ = QString("AI vs AI (%1) итог. Линии X: %2, Линии O: %3, Score (O - X): %4.%5")
                         .arg(modeText)
                         .arg(result.finalLinesX)
                         .arg(result.finalLinesO)
                         .arg(result.finalScore)
                         .arg(cancelled ? " Остановлено пользователем." : "");
    if (!cancelled && controller_.swapUsed()) {
        detailedStats_ += " Применено правило обмена.";
    }

    setSettingsEnabled(true);
    updateGameTypeUiState();
    updateGameStateLabel(false);
    if (!cancelled) {
        showGameOverMessage();
    }
}

void MainWindow::endCurrentGameEarly()
{
    sessionId_.fetch_add(1, std::memory_order_relaxed);
    stopAllAi(true);
    controller_.newGame(currentRows_, currentCols_, currentWinLength_, controller_.mode(), swapRuleEnabled_);
    swapPromptShown_ = false;
    refreshBoardView();
    updateStatusLabels();
    ui->labelStatus->setText("Игра не идёт");
    ui->labelAiMove->setText("-");
    setLastMove(Coord(-1, -1));
    recentMoves_.clear();
    hintLine_.clear();
    refreshBigInfoDisplay();
    detailedStats_.clear();
    setSettingsEnabled(true);
    updateGameTypeUiState();
    updateGameStateLabel(false);
}

QString MainWindow::coordToHuman(int row, int col) const
{
    
    QString letters;
    int x = col;
    while (true) {
        int rem = x % 26;
        letters.prepend(QChar('A' + rem));
        x = x / 26 - 1;
        if (x < 0) break;
    }
    return QString("%1%2").arg(letters).arg(row + 1);
}

void MainWindow::updateAiMoveLabel(const QString& who, const Coord& move, const QString& scoreText)
{
    QString coord = coordToHuman(move.row(), move.col());
    QString smallText = scoreText.isEmpty()
        ? QString("%1: %2").arg(who).arg(coord)
        : QString("%1: %2 (оценка %3)").arg(who).arg(coord).arg(scoreText);

    ui->labelAiMove->setText(smallText);
    addRecentMove(who, move, scoreText);
}

void MainWindow::setBigInfo(const QString& text)
{
    hintLine_ = text.trimmed();
    refreshBigInfoDisplay();
}

void MainWindow::refreshBigInfoDisplay()
{
    QStringList lines = recentMoves_;
    if (!hintLine_.isEmpty()) {
        lines.append(hintLine_);
    }

    if (lines.isEmpty()) {
        ui->labelBigInfo->setText("—");
        return;
    }

    ui->labelBigInfo->setText(lines.join("\n"));
}

void MainWindow::addRecentMove(const QString& who, const Coord& move, const QString& scoreText)
{
    QString coord = coordToHuman(move.row(), move.col());
    QString line = scoreText.isEmpty()
        ? QString("%1: %2").arg(who).arg(coord)
        : QString("%1: %2 (оценка %3)").arg(who).arg(coord).arg(scoreText);

    recentMoves_.prepend(line);
    while (recentMoves_.size() > 2) {
        recentMoves_.removeLast();
    }

    refreshBigInfoDisplay();
}

void MainWindow::setLastMove(const Coord& mv)
{
    lastMove_ = mv;
}

int MainWindow::evaluatePositionFor(Player p, int depth, bool memo)
{
    Player opponent = (p == Player::X) ? Player::O : Player::X;
    AIStatistics stats;
    
    const int maxEvalDepth = 4;
    int cappedDepth = depth > maxEvalDepth ? maxEvalDepth : depth;
    MoveEvaluation eval = controller_.findBestMove(opponent, cappedDepth, memo, stats);
    return -eval.score;
}

void MainWindow::startAsyncHint(Player p, bool isEvaluation, const QString& moverLabel, const Coord& evalMove)
{
    if (hintInProgress_) return;
    hintInProgress_ = true;
    hintTicket_ = sessionId_.load(std::memory_order_relaxed);
    hintPlayer_ = p;
    currentHintTask_ = isEvaluation ? HintTask::HumanEval : HintTask::Hint;
    evalMoveForHint_ = evalMove;
    evalMoverLabel_ = moverLabel;
    hintCancelFlag_.store(false, std::memory_order_relaxed);
    hintCanceled_ = false;

    int depth = ui->spinDepth->value();
    if (dynamicHintMode_) {
        depth = depth < 12 ? 12 : depth;
    }
    bool memo = ui->checkMemo->isChecked();
    int timeLimit = dynamicHintMode_ ? hintTimeLimitMs_ : -1;

    int ticket = hintTicket_;
    hintWatcher_.setFuture(QtConcurrent::run([this, p, depth, memo, timeLimit, ticket]() {AiSearchResult out; MoveEvaluation bestSoFar(Coord(-1, -1), std::numeric_limits<int>::min()); AIStatistics stats;  out.result = controller_.findBestMove(p, depth, memo, stats, &bestSoFar, &hintCancelFlag_, timeLimit); out.best   = bestSoFar; out.stats  = stats; out.cancelled = hintCancelFlag_.load(std::memory_order_relaxed);  if (ticket != sessionId_.load(std::memory_order_relaxed)) {out.result = MoveEvaluation(Coord(-1, -1), std::numeric_limits<int>::min()); out.best   = out.result; } return out; }));

    ui->btnCancelAi->setEnabled(true);
    setBigInfo(isEvaluation ? QStringLiteral("Идёт оценка вашего хода...") : QStringLiteral("Идёт расчёт подсказки..."));
    updateGameTypeUiState();
}

void MainWindow::updateGameStateLabel(bool running)
{
    if (ui->labelGameState) {
        ui->labelGameState->setVisible(false);
    }
    if (!ui->labelStatus) return;
    if (running) {
        ui->labelStatus->setStyleSheet("color: #22c55e; font-weight: 800; font-style: italic; font-size: 18px;");
    } else {
        ui->labelStatus->setStyleSheet("color: #ef4444; font-weight: 800; font-style: italic; font-size: 18px;");
    }
}

void MainWindow::showPopupMessage(const QString& text, QMessageBox::Icon icon)
{
    QMessageBox msg(icon, icon == QMessageBox::Warning ? "Внимание" : "Информация", text, QMessageBox::Ok, this);
    msg.exec();
}

bool MainWindow::isMoveValid(const MoveEvaluation& mv) const
{
    const Board& b = controller_.board();
    int rows = b.getRows();
    int cols = b.getCols();
    if (mv.move.row() < 0 || mv.move.col() < 0 || mv.move.row() >= rows || mv.move.col() >= cols) return false;
    if (!b.isEmpty(mv.move.row(), mv.move.col())) return false;
    return true;
}

void MainWindow::startAsyncAiMove(Player aiPlayer)
{
    if (aiSearchInProgress_) return;
    aiSearchInProgress_ = true;
    aiSearchTicket_ = sessionId_.load(std::memory_order_relaxed);
    aiSearchPlayer_ = aiPlayer;
    aiCancelFlag_.store(false, std::memory_order_relaxed);
    aiSearchCanceled_ = false;

    int  depth = ui->spinDepth->value();
    if (dynamicDepthMode_) {
        depth = std::max(depth, 12);
    }
    bool memo  = ui->checkMemo->isChecked();
    int  timeLimit = dynamicDepthMode_ ? ui->spinTimeLimit->value() * 1000 : -1;

    int ticket = aiSearchTicket_;
    aiWatcher_.setFuture(QtConcurrent::run([this, aiPlayer, depth, memo, timeLimit, ticket]() {AiSearchResult out; MoveEvaluation bestSoFar(Coord(-1, -1), std::numeric_limits<int>::min()); AIStatistics stats;  out.result = controller_.findBestMove(aiPlayer, depth, memo, stats, &bestSoFar, &aiCancelFlag_, timeLimit); out.best   = bestSoFar; out.stats  = stats; out.cancelled = aiCancelFlag_.load(std::memory_order_relaxed);  if (ticket != sessionId_.load(std::memory_order_relaxed)) {out.result = MoveEvaluation(Coord(-1, -1), std::numeric_limits<int>::min()); out.best   = out.result; } return out; }));

    ui->btnCancelAi->setEnabled(true);
    ui->labelStatus->setText("Игра идёт");
    updateGameTypeUiState();
}

void MainWindow::onAiSearchFinished()
{
    const int currentSession = sessionId_.load(std::memory_order_relaxed);
    if (aiSearchTicket_ != currentSession) {
        return;
    }

    ui->btnCancelAi->setEnabled(false);
    aiSearchInProgress_ = false;
    updateGameTypeUiState();
    AiSearchResult pack = aiWatcher_.result();
    bool cancelled = aiSearchCanceled_ || aiCancelFlag_.load(std::memory_order_relaxed) || pack.cancelled;

    MoveEvaluation chosen = isMoveValid(pack.best) ? pack.best : pack.result;
    if (!isMoveValid(chosen)) {
        showPopupMessage("Не удалось вычислить ход ИИ.", QMessageBox::Warning);
        return;
    }

    MoveStatus st = controller_.applyMove(chosen.move.row(), chosen.move.col());
    if (st != MoveStatus::Ok) {
        showPopupMessage("Применить ход ИИ не удалось.", QMessageBox::Warning);
        return;
    }
    setLastMove(chosen.move);

    QString who = QString("ИИ (%1)").arg(aiSearchPlayer_ == Player::X ? "X" : "O");
    updateAiMoveLabel(who, chosen.move, QString::number(chosen.score));
    refreshBoardView();
    updateStatusLabels();

    QString coord = coordToHuman(chosen.move.row(), chosen.move.col());
    QString cancelNote = cancelled ? " Поиск прерван, использован лучший найденный ход." : "";
    QString depthNote;
    if (dynamicDepthMode_) {
        depthNote = QString(" Достигнутая глубина: %1.").arg(pack.stats.completedDepth);
    }

    detailedStats_ = QString("Ход ИИ %1: %2 (оценка %3). Узлов %4, кеш hits %5, время %6 мс.%7%8")
                         .arg(aiSearchPlayer_ == Player::X ? "X" : "O")
                         .arg(coord)
                         .arg(chosen.score)
                         .arg(static_cast<qulonglong>(pack.stats.nodesVisited))
                         .arg(static_cast<qulonglong>(pack.stats.cacheHits))
                         .arg(static_cast<long long>(pack.stats.timeMs))
                         .arg(cancelNote)
                         .arg(depthNote);

    if (controller_.isGameOver()) {
        ui->labelStatus->setText("Игра не идёт");
        showGameOverMessage();
    } else {
        ui->labelStatus->setText("Игра идёт");
    }
}

void MainWindow::onHintFinished()
{
    const int currentSession = sessionId_.load(std::memory_order_relaxed);
    if (hintTicket_ != currentSession) {
        return;
    }

    ui->btnCancelAi->setEnabled(false);
    hintInProgress_ = false;
    updateGameTypeUiState();
    AiSearchResult pack = hintWatcher_.result();
    bool cancelled = hintCanceled_ || hintCancelFlag_.load(std::memory_order_relaxed) || pack.cancelled;

    MoveEvaluation chosen = isMoveValid(pack.best) ? pack.best : pack.result;
    if (!isMoveValid(chosen)) {
        if (!cancelled) {
            showPopupMessage("Не удалось вычислить подсказку.", QMessageBox::Warning);
        }
        currentHintTask_ = HintTask::None;
        return;
    }

    QString depthNote;
    if (dynamicHintMode_) {
        depthNote = QString(" Достигнутая глубина: %1.").arg(pack.stats.completedDepth);
    }

    if (currentHintTask_ == HintTask::Hint) {
        QString who = (hintPlayer_ == Player::X) ? "X" : "O";
        QString coord = coordToHuman(chosen.move.row(), chosen.move.col());
        QString text = QString("Рекомендация (%1): %2 (оценка %3).%4")
                           .arg(who)
                           .arg(coord)
                           .arg(chosen.score)
                           .arg(depthNote);

        detailedStats_ = QString("Подсказка (%1): %2 (оценка %3). Узлов %4, сгенерировано %5, кеш hits %6, misses %7, время %8 мс.%9")
                             .arg(who)
                             .arg(coord)
                             .arg(chosen.score)
                             .arg(static_cast<qulonglong>(pack.stats.nodesVisited))
                             .arg(static_cast<qulonglong>(pack.stats.nodesGenerated))
                             .arg(static_cast<qulonglong>(pack.stats.cacheHits))
                             .arg(static_cast<qulonglong>(pack.stats.cacheMisses))
                             .arg(static_cast<long long>(pack.stats.timeMs))
                             .arg(depthNote);

        setBigInfo(text);
        showPopupMessage(text, QMessageBox::Information);
    } else if (currentHintTask_ == HintTask::HumanEval) {
        
        int scoreForMover = -chosen.score;
        QString mover = evalMoverLabel_.isEmpty() ? QStringLiteral("Ваш ход") : evalMoverLabel_;
        QString coord = coordToHuman(evalMoveForHint_.row(), evalMoveForHint_.col());
        QString text = QString("%1 %2: оценка %3.%4")
                           .arg(mover)
                           .arg(coord)
                           .arg(scoreForMover)
                           .arg(depthNote);
        detailedStats_ = QString("Оценка хода %1: %2 (score %3). Узлов %4, сгенерировано %5, кеш hits %6, misses %7, время %8 мс.%9")
                             .arg(mover)
                             .arg(coord)
                             .arg(scoreForMover)
                             .arg(static_cast<qulonglong>(pack.stats.nodesVisited))
                             .arg(static_cast<qulonglong>(pack.stats.nodesGenerated))
                             .arg(static_cast<qulonglong>(pack.stats.cacheHits))
                             .arg(static_cast<qulonglong>(pack.stats.cacheMisses))
                             .arg(static_cast<long long>(pack.stats.timeMs))
                             .arg(depthNote);
        setBigInfo(text);
    }

    currentHintTask_ = HintTask::None;
    evalMoveForHint_ = Coord(-1, -1);
    evalMoverLabel_.clear();

    if (currentGameType_ == GameType::HumanVsAI) {
        autoPlayAiIfNeeded();
    }
}




void MainWindow::updateStatusLabels()
{
    if (controller_.isGameOver()) {
        ui->labelCurrentPlayer->setText("-");
        setBigInfo("Партия завершена");
        updateGameStateLabel(false);
    } else {
        Player p = controller_.currentPlayer();
        ui->labelCurrentPlayer->setText(p == Player::X ? "X" : "O");
        updateGameStateLabel(true);
    }

    if (controller_.mode() == GameMode::LinesScore) {
        ui->labelLinesX->setText(QString::number(controller_.creditedLinesX()));
        ui->labelLinesO->setText(QString::number(controller_.creditedLinesO()));
        ui->labelScore->setText(QString::number(controller_.score()));
    } else {
        int linesX = GameController::countLinesFor(controller_.board(),
                                                   CellState::X);
        int linesO = GameController::countLinesFor(controller_.board(),
                                                   CellState::O);
        ui->labelLinesX->setText(QString::number(linesX));
        ui->labelLinesO->setText(QString::number(linesO));
        ui->labelScore->setText(QString::number(linesO - linesX));
    }

    updateSwapUiState();
    offerSwapToHumanIfNeeded();
}




void MainWindow::showGameOverMessage()
{
    const Board& b = controller_.board();
    QString msg;

    if (controller_.mode() == GameMode::Classic) {
        bool xWin = b.checkWin(CellState::X);
        bool oWin = b.checkWin(CellState::O);
        if (xWin && !oWin) {
            msg = "Партия завершена.\nПобедил X.";
        } else if (oWin && !xWin) {
            msg = "Партия завершена.\nПобедил O.";
        } else {
            msg = "Партия завершена.\nНичья.";
        }
    } else {
        int linesX = controller_.creditedLinesX();
        int linesO = controller_.creditedLinesO();
        int score  = controller_.score();
        msg = QString("Партия завершена (режим по линиям).\n"
                      "Линии X: %1\nЛинии O: %2\nScore (O - X): %3")
                  .arg(linesX)
                  .arg(linesO)
                  .arg(score);
        if (score > 0) {
            msg += "\nРезультат: победа O.";
        } else if (score < 0) {
            msg += "\nРезультат: победа X.";
        } else {
            msg += "\nРезультат: ничья по счёту.";
        }
    }

    QMessageBox::information(this, "Игра окончена", msg);
    setSettingsEnabled(true);
    updateGameStateLabel(false);
}

void MainWindow::offerSwapToHumanIfNeeded()
{
    if (swapPromptShown_) return;
    if (!controller_.isSwapRuleEnabled()) return;
    if (!controller_.isSwapAvailable()) return;
    if (controller_.isGameOver()) return;

    Player current = controller_.currentPlayer();
    if (isPlayerAi(current)) return;

    swapPromptShown_ = true;
    QString question = QStringLiteral("Поменяться сторонами? После обмена вы будете играть за X, "
                                      "оппонент за O. Ход по-прежнему остаётся за O.");
    QMessageBox::StandardButton res = QMessageBox::question(
        this,
        QStringLiteral("Правило обмена"),
        question,
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (res == QMessageBox::Yes) {
        applySwapIfPossible("Вы");
    } else {
        setBigInfo("Вы оставили роли без обмена.");
    }
}

bool MainWindow::maybeAiTakesSwap()
{
    if (!controller_.isSwapRuleEnabled()) return false;
    if (!controller_.isSwapAvailable()) return false;

    Player current = controller_.currentPlayer();
    if (!isPlayerAi(current)) return false;

    int depth = ui->spinDepth->value();
    bool memo = ui->checkMemo->isChecked();
    bool swapped = controller_.maybeAutoSwapForCurrent(depth, memo, 0, &aiCancelFlag_);
    if (swapped) {
        applySwapIfPossible("ИИ", false, true);
        return true;
    }
    return false;
}

void MainWindow::autoPlayAiIfNeeded()
{
    if (hintInProgress_) return;
    if (currentGameType_ != GameType::HumanVsAI) return;
    if (!autoAiEnabled_) return;
    if (aiVsAiStepMode_) return;
    if (controller_.isGameOver()) return;
    if (aiSearchInProgress_) return;
    if (maybeAiTakesSwap()) return;

    Player aiPlayer = humanIsX_ ? Player::O : Player::X;
    if (controller_.currentPlayer() != aiPlayer) return;

    startAsyncAiMove(aiPlayer);
}




void MainWindow::handleBoardClick(int row, int col)
{
    if (aiSearchInProgress_) {
        showPopupMessage("ИИ сейчас думает. Дождитесь завершения или нажмите \"Прервать ход ИИ\".");
        return;
    }
    if (hintInProgress_) {
        showPopupMessage("Идёт вычисление оценки/подсказки. Дождитесь завершения или нажмите \"Прервать ход ИИ\".");
        return;
    }

    
    if (aiVsAiStepMode_) {
        showPopupMessage("Сейчас активен пошаговый режим AI vs AI. Нажимайте \"Следующий ход AI vs AI\".");
        return;
    }

    if (controller_.isGameOver()) {
        showPopupMessage("Партия уже завершена. Нажмите <Новая партия>.");
        return;
    }

    if (currentGameType_ == GameType::AIVsAI) {
        showPopupMessage("Сейчас активен режим AI vs AI. Используйте кнопки управления AI.");
        return;
    }

    Player aiPlayer = humanIsX_ ? Player::O : Player::X;
    if (currentGameType_ == GameType::HumanVsAI &&
        controller_.currentPlayer() == aiPlayer) {
        showPopupMessage(QString("Сейчас ход ИИ %1. Нажмите \"Ход ИИ\" или включите автоход.") .arg(aiPlayer == Player::X ? "X" : "O"));
        return;
    }

    if (!settingsLocked_) {
        setSettingsEnabled(false);
    }

    Player mover = controller_.currentPlayer();

    MoveStatus st = controller_.applyMove(row, col);
    switch (st) {
    case MoveStatus::InvalidCell:
        showPopupMessage("Нельзя походить в эту клетку.");
        return;
    case MoveStatus::ForbiddenFirstCenter:
        showPopupMessage("По правилам баланса первый ход X в центр запрещён.");
        return;
    case MoveStatus::GameAlreadyOver:
        showPopupMessage("Партия уже завершена.");
        return;
    case MoveStatus::Ok:
        break;
    }

    QString moveScore;
    QString moverLabel = (currentGameType_ == GameType::HumanVsAI)
                             ? QString("Вы (%1)").arg(mover == Player::X ? "X" : "O")
                             : QString("Игрок %1").arg(mover == Player::X ? "X" : "O");
    setLastMove(Coord(row, col));
    addRecentMove(moverLabel, Coord(row, col), moveScore);
    if (!hintLine_.isEmpty()) {
        hintLine_.clear();
        refreshBigInfoDisplay();
    }

    refreshBoardView();
    updateStatusLabels();

    bool evalStarted = false;
    if (evaluateHumanMoves_) {
        
        Player opponent = (mover == Player::X) ? Player::O : Player::X;
        startAsyncHint(opponent, true, moverLabel, Coord(row, col));
        evalStarted = true;
    }

    if (controller_.isGameOver()) {
        ui->labelStatus->setText("Игра не идёт");
        showGameOverMessage();
    } else if (!evalStarted) {
        autoPlayAiIfNeeded();
    }
}




void MainWindow::onNewGameClicked()
{
    sessionId_.fetch_add(1, std::memory_order_relaxed);
    stopAllAi(true);

    int rows   = ui->spinBoardRows->value();
    int cols   = ui->spinBoardCols->value();
    int winLen = ui->spinWinLength->value();

    int maxLen = (rows < cols ? rows : cols);
    if (maxLen < 3) maxLen = 3;

    if (winLen < 3) {
        winLen = 3;
    } else if (winLen > maxLen) {
        winLen = maxLen;
    }
    ui->spinWinLength->setValue(winLen);

    currentRows_      = rows;
    currentCols_      = cols;
    currentWinLength_ = winLen;

    GameMode mode =
        (ui->comboMode->currentIndex() == 0)
            ? GameMode::LinesScore
            : GameMode::Classic;

    swapRuleEnabled_ = (mode == GameMode::LinesScore) &&
                       (ui->checkSwapRule->checkState() != Qt::Unchecked);

    controller_.newGame(rows, cols, winLen, mode, swapRuleEnabled_);

    currentGameType_    = gameTypeFromUI();
    currentAIVsAISpeed_ = aiVsAiSpeedFromUI();
    autoAiEnabled_      = ui->checkAutoAi->isChecked();
    humanIsX_           = (ui->comboPlayerSide->currentIndex() == 0);
    evaluateHumanMoves_ = ui->checkEvalHuman->isChecked();
    dynamicDepthMode_   = ui->checkDynamicDepth->isChecked();
    dynamicHintMode_    = ui->checkHintDynamic->isChecked();
    timeLimitMs_        = ui->spinTimeLimit->value() * 1000;
    hintTimeLimitMs_    = ui->spinHintTime->value() * 1000;
    swapPromptShown_    = false;
    hintInProgress_     = false;
    hintCanceled_       = false;
    currentHintTask_    = HintTask::None;
    evalMoveForHint_    = Coord(-1, -1);
    evalMoverLabel_.clear();
    recentMoves_.clear();
    hintLine_.clear();
    detailedStats_.clear();
    setLastMove(Coord(-1, -1));

    setSettingsEnabled(false);
    updateGameTypeUiState();

    rebuildBoard();
    refreshBoardView();
    updateStatusLabels();
    refreshBigInfoDisplay();
    ui->labelAiMove->setText("-");
    updateGameStateLabel(true);
    updateSwapUiState();

    if (currentGameType_ == GameType::HumanVsHuman) {
        ui->labelStatus->setText("Игра идёт");
        return;
    }

    if (currentGameType_ == GameType::HumanVsAI) {
        QString aiSide = humanIsX_ ? "O" : "X";
        QString humanSide = humanIsX_ ? "X" : "O";
        QString starter = humanIsX_ ? QString("вы (%1)").arg(humanSide)
                                    : QString("ИИ (%1)").arg("X");
        ui->labelStatus->setText("Игра идёт");
        autoPlayAiIfNeeded();
        return;
    }

    
    if (currentAIVsAISpeed_ == AIVsAISpeed::Step) {
        startStepAIVsAIGame();
        return;
    }

    ui->labelStatus->setText("Игра идёт (AI vs AI автоматически).");
    startAutoAIVsAIGame(true);
}




void MainWindow::onBestMoveClicked()
{
    if (currentGameType_ != GameType::HumanVsAI) {
        showPopupMessage("Подсказка/ход ИИ доступна только в режиме ИИ против игрока.");
        return;
    }

    if (controller_.isGameOver()) {
        showPopupMessage("Партия завершена. Начните новую.");
        setBigInfo("Партия завершена");
        return;
    }

    Player aiPlayer = humanIsX_ ? Player::O : Player::X;
    if (controller_.currentPlayer() != aiPlayer) {
        showPopupMessage(QString("Сейчас не ход %1. Сначала сделайте ход %2.") .arg(aiPlayer == Player::X ? "X" : "O") .arg(aiPlayer == Player::X ? "O" : "X"));
        return;
    }

    if (maybeAiTakesSwap()) {
        return;
    }

    startAsyncAiMove(aiPlayer);
}




void MainWindow::onHintClicked()
{
    if (hintInProgress_) {
        showPopupMessage("Подсказка уже вычисляется. Дождитесь завершения или нажмите \"Прервать ход ИИ\".");
        return;
    }
    if (currentGameType_ == GameType::AIVsAI) {
        showPopupMessage("Подсказка недоступна в режиме AI vs AI.");
        return;
    }
    if (isPlayerAi(controller_.currentPlayer())) {
        showPopupMessage("Сейчас ход ИИ. Подсказка будет доступна после его хода.", QMessageBox::Information);
        return;
    }

    if (controller_.isGameOver()) {
        showPopupMessage("Партия завершена. Подсказка не требуется.");
        setBigInfo("Партия завершена");
        return;
    }

    Player p = controller_.currentPlayer();
    startAsyncHint(p, false, QString(), Coord(-1, -1));
}




void MainWindow::onAIVsAIStepClicked()
{
    if (aiVsAiStepBusy_) return;
    if (currentGameType_ != GameType::AIVsAI ||
        currentAIVsAISpeed_ != AIVsAISpeed::Step) {
        showPopupMessage("Выберите 'ИИ против ИИ' и скорость 'Пошагово', затем запустите новую партию.");
        return;
    }

    if (!aiVsAiStepMode_) {
        showPopupMessage("Запустите новую партию в режиме AI vs AI (пошагово).");
        return;
    }

    if (controller_.isGameOver()) {
        showPopupMessage("Партия уже завершена. Нажмите 'Новая партия'.");
        aiVsAiStepMode_ = false;
        showGameOverMessage();
        return;
    }

    aiCancelFlag_.store(false, std::memory_order_relaxed);
    aiVsAiStepBusy_ = true;
    ui->btnCancelAi->setEnabled(true);

    aiVsAiStepWatcher_.setFuture(QtConcurrent::run([this]() {AIVsAIStepResult r; r.ok = controller_.stepAIVsAIMove(aiStepDepthX_, aiStepMemoX_, aiStepDepthO_, aiStepMemoO_, r.info, &aiCancelFlag_, aiStepTimeLimitMsX_, aiStepTimeLimitMsO_); return r; }));
}




void MainWindow::onModeChanged(int index)
{
    Q_UNUSED(index);
    onNewGameClicked();
}

void MainWindow::onGameTypeChanged(int index)
{
    Q_UNUSED(index);
    updateGameTypeUiState();
}

void MainWindow::onAivAiSpeedChanged(int index)
{
    Q_UNUSED(index);
    updateGameTypeUiState();
}

void MainWindow::onSwapRuleToggled(Qt::CheckState state)
{
    swapRuleEnabled_ = (state != Qt::Unchecked) &&
                       (controller_.mode() == GameMode::LinesScore);
    updateSwapUiState();
}

void MainWindow::onSwapNowClicked()
{
    if (controller_.mode() != GameMode::LinesScore) {
        showPopupMessage("Правило обмена доступно только в режиме по линиям.");
        return;
    }
    if (!controller_.isSwapRuleEnabled()) {
        showPopupMessage("Включите галочку \"Правило обмена\", чтобы его использовать.");
        return;
    }
    if (!controller_.isSwapAvailable()) {
        showPopupMessage("Обмен доступен только сразу после первого хода X.");
        return;
    }
    applySwapIfPossible("Вы");
}

void MainWindow::onAutoAiToggled(Qt::CheckState state)
{
    autoAiEnabled_ = (state != Qt::Unchecked);
    updateGameTypeUiState();
    autoPlayAiIfNeeded();
}

void MainWindow::onEvalHumanToggled(Qt::CheckState state)
{
    evaluateHumanMoves_ = (state != Qt::Unchecked);
}

void MainWindow::onDynamicDepthToggled(Qt::CheckState state)
{
    dynamicDepthMode_ = (state != Qt::Unchecked);
    ui->spinTimeLimit->setEnabled(dynamicDepthMode_);
    timeLimitMs_ = ui->spinTimeLimit->value() * 1000;
}

void MainWindow::onTimeLimitChanged(int seconds)
{
    Q_UNUSED(seconds);
    timeLimitMs_ = ui->spinTimeLimit->value() * 1000;
}

void MainWindow::onHintDynamicToggled(Qt::CheckState state)
{
    dynamicHintMode_ = (state != Qt::Unchecked);
    ui->spinHintTime->setEnabled(dynamicHintMode_);
    hintTimeLimitMs_ = ui->spinHintTime->value() * 1000;
}

void MainWindow::onHintTimeChanged(int seconds)
{
    Q_UNUSED(seconds);
    hintTimeLimitMs_ = ui->spinHintTime->value() * 1000;
}

void MainWindow::onAiVsAiStepFinished()
{
    if (aiVsAiStepTicket_ != sessionId_.load(std::memory_order_relaxed)) return;

    aiVsAiStepBusy_ = false;
    ui->btnCancelAi->setEnabled(false);

    AIVsAIStepResult r = aiVsAiStepWatcher_.result();
    bool cancelled = aiCancelFlag_.load(std::memory_order_relaxed);

    if (!r.ok || cancelled) {
        if (controller_.isGameOver()) {
            aiVsAiStepMode_ = false;
            showGameOverMessage();
        }
        updateGameTypeUiState();
        return;
    }

    QString who = (r.info.player == Player::X) ? "X" : "O";
    QString whoLabel = QString("ИИ (%1)").arg(who);
    const AIStatistics &s = r.info.stats;

    if (r.info.isSwap) {
        
        detailedStats_ = QString("ИИ (%1) применил правило обмена.").arg(who);
        if (controller_.swapUsed()) {
            detailedStats_ += " Правило обмена было применено в этой партии.";
        }
        setBigInfo(detailedStats_);
        ui->labelStatus->setText("Игра идёт");

        
        refreshBoardView();
        updateStatusLabels();
    } else {
        QString depthNote;
        if (dynamicDepthMode_) {
            depthNote = QString(" Достигнутая глубина: %1.").arg(s.completedDepth);
        }

        detailedStats_ = QString(
                             "ИИ (%1) ход %2 (оценка %3). Узлов %4, сгенерировано %5, кеш hits %6, misses %7, время %8 мс.%9")
                             .arg(who)
                             .arg(coordToHuman(r.info.move.row(), r.info.move.col()))
                             .arg(r.info.evalScore)
                             .arg(static_cast<qulonglong>(s.nodesVisited))
                             .arg(static_cast<qulonglong>(s.nodesGenerated))
                             .arg(static_cast<qulonglong>(s.cacheHits))
                             .arg(static_cast<qulonglong>(s.cacheMisses))
                             .arg(static_cast<long long>(s.timeMs))
                             .arg(depthNote);
        if (controller_.swapUsed()) {
            detailedStats_ += " Правило обмена было применено в этой партии.";
        }

        
        setLastMove(r.info.move);
        updateAiMoveLabel(whoLabel, r.info.move, QString::number(r.info.evalScore));

        refreshBoardView();
        updateStatusLabels();

        ui->labelStatus->setText("Игра идёт");
    }

    if (controller_.isGameOver()) {
        aiVsAiStepMode_ = false;
        showGameOverMessage();
    }

    updateGameTypeUiState();
}

void MainWindow::onShowStatsClicked()
{
    QDialog dlg(this);
    dlg.setWindowTitle("Статистика");
    dlg.setStyleSheet(this->styleSheet());

    QVBoxLayout *vbox = new QVBoxLayout(&dlg);
    QFormLayout *form = new QFormLayout();
    auto addRow = [&](const QString& title, const QString& value) {
        QLabel *lblTitle = new QLabel(title);
        QLabel *lblValue = new QLabel(value);
        lblValue->setWordWrap(true);
        form->addRow(lblTitle, lblValue);
    };

    QString modeText = (controller_.mode() == GameMode::LinesScore)
                           ? "По линиям"
                           : "Классика";
    QString typeText;
    if (currentGameType_ == GameType::HumanVsAI) {
        typeText = "ИИ против игрока";
    } else if (currentGameType_ == GameType::HumanVsHuman) {
        typeText = "Игрок против игрока";
    } else {
        typeText = "ИИ против ИИ";
    }

    addRow("Режим:", modeText);
    addRow("Тип:", typeText);
    addRow("Размер поля:", QString("%1 × %2").arg(currentRows_).arg(currentCols_));
    addRow("Длина линии:", QString::number(currentWinLength_));

    QString currentTurn = controller_.isGameOver()
                              ? "—"
                              : (controller_.currentPlayer() == Player::X ? "X" : "O");
    addRow("Ходит:", currentTurn);

    int linesX = 0;
    int linesO = 0;
    int score  = 0;
    if (controller_.mode() == GameMode::LinesScore) {
        linesX = controller_.creditedLinesX();
        linesO = controller_.creditedLinesO();
        score  = controller_.score();
    } else {
        linesX = GameController::countLinesFor(controller_.board(), CellState::X);
        linesO = GameController::countLinesFor(controller_.board(), CellState::O);
        score  = linesO - linesX;
    }

    addRow("Линии X:", QString::number(linesX));
    addRow("Линии O:", QString::number(linesO));
    addRow("Score (O - X):", QString::number(score));

    addRow("Автоход ИИ:", autoAiEnabled_ ? "включен" : "выключен");
    addRow("Текущий статус:", ui->labelStatus->text());

    QString movesText = recentMoves_.isEmpty()
                            ? "—"
                            : recentMoves_.join("\n");
    addRow("Последние ходы:", movesText);
    if (!hintLine_.isEmpty()) {
        addRow("Подсказка:", hintLine_);
    }
    addRow("Статистика:", detailedStats_.isEmpty() ? QString("—") : detailedStats_);

    vbox->addLayout(form);

    QDialogButtonBox *buttons =
        new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    vbox->addWidget(buttons);

    dlg.exec();
}

void MainWindow::onEndGameClicked()
{
    endCurrentGameEarly();
}

void MainWindow::onCancelAiClicked()
{
    if (aiSearchInProgress_) {
        aiCancelFlag_.store(true, std::memory_order_relaxed);
        aiSearchCanceled_ = true;
        ui->btnCancelAi->setEnabled(false);
    }
    if (hintInProgress_) {
        hintCancelFlag_.store(true, std::memory_order_relaxed);
        hintCanceled_ = true;
        ui->btnCancelAi->setEnabled(false);
    }
    if (aiVsAiRunning_ || aiVsAiStepBusy_) {
        aiCancelFlag_.store(true, std::memory_order_relaxed);
        ui->btnCancelAi->setEnabled(false);
    }
}
