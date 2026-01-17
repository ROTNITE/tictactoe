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
#include <QGroupBox>
#include <QAbstractButton>
#include <algorithm>
#include <limits>
#include <QtConcurrent/QtConcurrent>
#include <QSignalBlocker>

namespace {
bool isOpeningChoicePhase(OpeningPhase phase)
{
    switch (phase) {
    case OpeningPhase::Pie_OfferSwap:
    case OpeningPhase::Swap2_B_ChooseOption:
    case OpeningPhase::Swap2_A_FinalChooseSide:
    case OpeningPhase::Swap2P_B_ChooseSide:
    case OpeningPhase::Swap2P_A_FinalChooseSide:
        return true;
    default:
        return false;
    }
}
}


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , controller_(3, 3, 3, GameMode::LinesScore) 
{
    ui->setupUi(this);

    applyModernStyle();

    if (ui->statusbar) {
        turnIndicatorLabel_ = new QLabel(this);
        turnIndicatorLabel_->setObjectName("labelTurnIndicator");
        turnIndicatorLabel_->setText(QStringLiteral("«—»"));
        turnIndicatorLabel_->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        turnIndicatorLabel_->setStyleSheet("color: #ffedd5; font-weight: 800;");
        ui->statusbar->addPermanentWidget(turnIndicatorLabel_);
    }

    
    ui->comboMode->clear();
    ui->comboMode->addItem("По линиям");    
    ui->comboMode->addItem("Классический"); 
    ui->comboMode->setCurrentIndex(0);
    ui->comboOpeningRule->clear();
    ui->comboOpeningRule->addItem("Без открытия");
    ui->comboOpeningRule->addItem("Pie-swap");
    ui->comboOpeningRule->addItem("Swap2");
    ui->comboOpeningRule->addItem("Swap2+");
    ui->comboOpeningRule->setCurrentIndex(0);

    
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
    ui->spinTimeLimit->setEnabled(ui->checkDynamicDepth->isChecked());
    ui->checkMemo->setChecked(true);
    ui->comboEnginePreset->clear();
    ui->comboEnginePreset->addItem("Fast");
    ui->comboEnginePreset->addItem("Strict");
    ui->comboEnginePreset->setCurrentIndex(0);
    ui->comboMoveGenMode->setCurrentIndex(2);
    ui->checkUseLmr->setChecked(true);
    ui->checkUseExtensions->setChecked(true);
    ui->groupBoxAdvanced->setChecked(false);

    
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
    connect(ui->checkAutoAi, &QCheckBox::checkStateChanged, this, &MainWindow::onAutoAiToggled);
    connect(ui->checkDynamicDepth, &QCheckBox::checkStateChanged, this, &MainWindow::onDynamicDepthToggled);
    connect(ui->comboOpeningRule, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onOpeningRuleChanged);
    connect(ui->comboEnginePreset, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onEnginePresetChanged);
    connect(ui->groupBoxAdvanced, &QGroupBox::toggled, this, &MainWindow::onAdvancedToggled);
    connect(ui->comboMoveGenMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onMoveGenModeChanged);
    connect(ui->checkUseLmr, &QCheckBox::checkStateChanged, this, &MainWindow::onUseLmrToggled);
    connect(ui->checkUseExtensions, &QCheckBox::checkStateChanged, this, &MainWindow::onUseExtensionsToggled);
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
    humanSeat_          = (ui->comboPlayerSide->currentIndex() == 0) ? Seat::A : Seat::B;
    syncHumanSideFromSeat();
    dynamicDepthMode_   = ui->checkDynamicDepth->isChecked();
    timeLimitMs_        = ui->spinTimeLimit->value() * 1000;
    openingRule_        = openingRuleFromUi();
    controller_.setOpeningRule(openingRule_);
    hintInProgress_     = false;
    hintCanceled_       = false;
    applyEnginePresetFromUi();

    
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
    updateOpeningUiState();
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

        QGroupBox::indicator {
            width: 16px;
            height: 16px;
        }
        QGroupBox::indicator:unchecked {
            border: 1px solid #334155;
            background-color: #020617;
            border-radius: 4px;
        }
        QGroupBox::indicator:checked {
            border: 1px solid #22c55e;
            background-color: #22c55e;
            border-radius: 4px;
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

        QGroupBox::indicator {
            width: 16px;
            height: 16px;
        }
        QGroupBox::indicator:unchecked {
            border: 1px solid #334155;
            background-color: #020617;
            border-radius: 4px;
        }
        QGroupBox::indicator:checked {
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
    Board b = controller_.boardSnapshot();
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

    Board b = controller_.boardSnapshot();
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
    ui->checkDynamicDepth->setEnabled(enabled);
    ui->checkMemo->setEnabled(enabled);
    ui->comboEnginePreset->setEnabled(enabled);
    ui->groupBoxAdvanced->setEnabled(enabled);
    ui->comboOpeningRule->setEnabled(enabled);
    ui->spinTimeLimit->setEnabled(enabled && ui->checkDynamicDepth->isChecked());
    
    ui->checkAutoAi->setEnabled(currentGameType_ == GameType::HumanVsAI);
    ui->comboGameType->setEnabled(enabled);
    ui->comboAIVsAISpeed->setEnabled(enabled && currentGameType_ == GameType::AIVsAI);
    ui->btnHint->setEnabled(!controller_.isGameOver() && currentGameType_ != GameType::AIVsAI);
    if (hintInProgress_) {
        ui->btnHint->setEnabled(false);
    }
    ui->comboPlayerSide->setEnabled(enabled && currentGameType_ == GameType::HumanVsAI);
    updateGameTypeUiState();
    updateOpeningUiState();
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
    return isSeatAi(controller_.seatForSide(p));
}

bool MainWindow::isSeatAi(Seat seat) const
{
    if (currentGameType_ == GameType::HumanVsHuman) return false;
    if (currentGameType_ == GameType::HumanVsAI) return seat != humanSeat_;
    return true;
}

bool MainWindow::isCurrentSeatAi() const
{
    return isSeatAi(controller_.seatToMove());
}

void MainWindow::syncHumanSideFromSeat()
{
    Seat seatX = controller_.seatForSide(Player::X);
    humanIsX_ = (seatX == humanSeat_);
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
    ui->checkDynamicDepth->setEnabled(unlocked);
    ui->spinTimeLimit->setEnabled(unlocked && ui->checkDynamicDepth->isChecked());
    updateOpeningUiState();
}

void MainWindow::updateOpeningUiState()
{
    if (!hintLine_.isEmpty() || currentGameType_ == GameType::AIVsAI) return;

    switch (controller_.openingPhase()) {
    case OpeningPhase::Pie_OfferSwap:
        setBigInfo("Открытие Pie-swap: второй игрок может поменяться сторонами.");
        break;
    case OpeningPhase::Swap2_B_ChooseOption:
        setBigInfo("Открытие Swap2: выберите опцию (взять X, взять O, или поставить две и дать выбор).");
        break;
    case OpeningPhase::Swap2_A_FinalChooseSide:
        setBigInfo("Открытие Swap2: игрок A выбирает сторону.");
        break;
    case OpeningPhase::Swap2P_B_ChooseSide:
        setBigInfo("Открытие Swap2+: игрок B выбирает сторону.");
        break;
    case OpeningPhase::Swap2P_A_FinalChooseSide:
        setBigInfo("Открытие Swap2+: игрок A выбирает сторону.");
        break;
    default:
        if (hintLine_.startsWith("Открытие")) {
            hintLine_.clear();
            refreshBigInfoDisplay();
        }
        break;
    }
}

OpeningRule MainWindow::openingRuleFromUi() const
{
    int idx = ui->comboOpeningRule->currentIndex();
    if (idx == 1) return OpeningRule::PieSwap;
    if (idx == 2) return OpeningRule::Swap2;
    if (idx == 3) return OpeningRule::Swap2Plus;
    return OpeningRule::None;
}

void MainWindow::onOpeningRuleChanged(int index)
{
    Q_UNUSED(index);
    openingRule_ = openingRuleFromUi();
    controller_.setOpeningRule(openingRule_);
    updateOpeningUiState();
}

void MainWindow::showOpeningRuleInfoDialog()
{
    QString text;
    switch (openingRule_) {
    case OpeningRule::None:
        text = QStringLiteral("Правило: без открытия.\nСтороны фиксированы, игра начинается обычными ходами.");
        break;
    case OpeningRule::PieSwap:
        text = QStringLiteral(
            "Правило: Pie-swap.\nПервый ход делает X. После этого второй игрок может поменяться сторонами.");
        break;
    case OpeningRule::Swap2:
        text = QStringLiteral(
            "Правило: Swap2.\nX ставит 1-й и 3-й камень, O ставит 2-й. Затем игрок B выбирает опцию: "
            "взять X, взять O + доп. O, или поставить два камня и дать выбор стороны игроку A.");
        break;
    case OpeningRule::Swap2Plus:
        text = QStringLiteral(
            "Правило: Swap2+.\nИгрок A ставит X и O, затем игрок B выбирает сторону и ставит 3-й камень "
            "за выбранную сторону. После этого игрок A выбирает сторону.");
        break;
    }

    QMessageBox::information(this, QStringLiteral("Правило игры"), text);
}

bool MainWindow::resolveOpeningChoiceIfNeeded()
{
    if (!isOpeningChoicePhase(controller_.openingPhase())) return false;
    if (currentGameType_ == GameType::AIVsAI) return false;

    if (isCurrentSeatAi()) {
        int depth = ui->spinDepth->value();
        if (dynamicDepthMode_) {
            depth = std::max(depth, 12);
        }
        bool memo = ui->checkMemo->isChecked();
        bool resolved = controller_.autoResolveOpeningChoiceForCurrentAI(depth, memo, &aiCancelFlag_);
        if (resolved) {
            syncHumanSideFromSeat();
            setBigInfo("ИИ сделал выбор открытия.");
        }
        return resolved;
    }

    OpeningPhase phase = controller_.openingPhase();
    if (phase == OpeningPhase::Pie_OfferSwap) {
        QMessageBox::StandardButton res = QMessageBox::question(
            this,
            QStringLiteral("Открытие Pie-swap"),
            QStringLiteral("Поменяться сторонами?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        controller_.choosePieSwap(res == QMessageBox::Yes);
        syncHumanSideFromSeat();
        if (res == QMessageBox::Yes) {
            setBigInfo("Вы поменялись сторонами.");
        } else {
            setBigInfo("Вы оставили роли без обмена.");
        }
        return true;
    }

    if (phase == OpeningPhase::Swap2_B_ChooseOption) {
        QMessageBox msg(this);
        msg.setWindowTitle("Открытие Swap2");
        msg.setText("Выберите опцию:");
        QAbstractButton* takeX = msg.addButton("Взять X", QMessageBox::AcceptRole);
        QAbstractButton* takeO = msg.addButton("Взять O + доп. O", QMessageBox::AcceptRole);
        QAbstractButton* placeTwo = msg.addButton("Поставить 2 и дать выбор", QMessageBox::AcceptRole);
        QAbstractButton* cancel = msg.addButton(QMessageBox::Cancel);
        msg.exec();
        if (msg.clickedButton() == cancel) return false;
        if (msg.clickedButton() == takeX) {
            controller_.chooseSwap2Option(Swap2Option::TakeX);
        } else if (msg.clickedButton() == takeO) {
            controller_.chooseSwap2Option(Swap2Option::TakeO_AndPlaceExtraO);
        } else if (msg.clickedButton() == placeTwo) {
            controller_.chooseSwap2Option(Swap2Option::PlaceTwoAndGiveChoice);
        }
        syncHumanSideFromSeat();
        return true;
    }

    if (phase == OpeningPhase::Swap2_A_FinalChooseSide) {
        QMessageBox msg(this);
        msg.setWindowTitle("Открытие Swap2");
        msg.setText("Выберите сторону для игрока A:");
        QAbstractButton* takeX = msg.addButton("Играть за X", QMessageBox::AcceptRole);
        QAbstractButton* takeO = msg.addButton("Играть за O", QMessageBox::AcceptRole);
        QAbstractButton* cancel = msg.addButton(QMessageBox::Cancel);
        msg.exec();
        if (msg.clickedButton() == cancel) return false;
        controller_.chooseSwap2FinalSide(msg.clickedButton() == takeX ? Player::X : Player::O);
        syncHumanSideFromSeat();
        return true;
    }

    if (phase == OpeningPhase::Swap2P_B_ChooseSide) {
        QMessageBox msg(this);
        msg.setWindowTitle("Открытие Swap2+");
        msg.setText("Выберите сторону для игрока B:");
        QAbstractButton* takeX = msg.addButton("Играть за X", QMessageBox::AcceptRole);
        QAbstractButton* takeO = msg.addButton("Играть за O", QMessageBox::AcceptRole);
        QAbstractButton* cancel = msg.addButton(QMessageBox::Cancel);
        msg.exec();
        if (msg.clickedButton() == cancel) return false;
        controller_.chooseSwap2PlusSideB(msg.clickedButton() == takeX ? Player::X : Player::O);
        syncHumanSideFromSeat();
        return true;
    }

    if (phase == OpeningPhase::Swap2P_A_FinalChooseSide) {
        QMessageBox msg(this);
        msg.setWindowTitle("Открытие Swap2+");
        msg.setText("Выберите сторону для игрока A:");
        QAbstractButton* takeX = msg.addButton("Играть за X", QMessageBox::AcceptRole);
        QAbstractButton* takeO = msg.addButton("Играть за O", QMessageBox::AcceptRole);
        QAbstractButton* cancel = msg.addButton(QMessageBox::Cancel);
        msg.exec();
        if (msg.clickedButton() == cancel) return false;
        controller_.chooseSwap2PlusFinalSideA(msg.clickedButton() == takeX ? Player::X : Player::O);
        syncHumanSideFromSeat();
        return true;
    }

    return false;
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

    controller_.setOnAIMoveCallback(
        [this, sleepMs, ticket](const Board& board, Player who, const MoveEvaluation& eval, const AIStatistics&) {
            if (ticket != sessionId_.load(std::memory_order_relaxed)) return;
            if (aiCancelFlag_.load(std::memory_order_relaxed)) return;
            Coord mv = eval.move;
            int score = eval.score;
            const bool moveInRange =
                (mv.row() >= 0 && mv.col() >= 0 && mv.row() < board.getRows() && mv.col() < board.getCols());
            if (!moveInRange) {
                QMetaObject::invokeMethod(this, [this, who, ticket]() {
                    if (ticket != sessionId_.load(std::memory_order_relaxed)) return;
                    QString whoText = QString("%1 (%2)")
                                          .arg(seatLabel(controller_.seatForSide(who)))
                                          .arg(sideLabel(who));
                    setBigInfo(QString("%1 сделал выбор открытия.").arg(whoText));
                    refreshBoardView();
                    updateStatusLabels();
                }, Qt::QueuedConnection);
                if (sleepMs > 0) {
                    QThread::msleep(static_cast<unsigned long>(sleepMs));
                }
                return;
            }
            QMetaObject::invokeMethod(this, [this, who, mv, score, ticket]() {
                if (ticket != sessionId_.load(std::memory_order_relaxed)) return;
                setLastMove(mv);
                updateAiMoveLabel(who, mv, QString::number(score));
                refreshBoardView();
                updateStatusLabels();
            }, Qt::QueuedConnection);
            if (sleepMs > 0) {
                QThread::msleep(static_cast<unsigned long>(sleepMs));
            }
        });

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
        detailedStats_ += " В этой партии менялись стороны.";
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
    controller_.newGame(currentRows_, currentCols_, currentWinLength_, controller_.mode(), openingRule_);
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

QString MainWindow::seatLabel(Seat seat) const
{
    return (seat == Seat::A) ? QStringLiteral("Игрок A") : QStringLiteral("Игрок B");
}

QString MainWindow::sideLabel(Player side) const
{
    return (side == Player::X) ? QStringLiteral("X") : QStringLiteral("O");
}

void MainWindow::updateTurnIndicator()
{
    if (!turnIndicatorLabel_) return;

    if (controller_.isGameOver()) {
        turnIndicatorLabel_->setText(QStringLiteral("«партия завершена»"));
        return;
    }

    Player p = controller_.currentPlayer();
    Seat seat = controller_.seatToMove();
    QString whoText = QStringLiteral("«ходит %1 (%2)»")
                          .arg(seatLabel(seat))
                          .arg(sideLabel(p));
    turnIndicatorLabel_->setText(whoText);
}

QString MainWindow::formatMoveLine(Player mover, const Coord& move, const QString& scoreText) const
{
    QString coord = coordToHuman(move.row(), move.col());
    QString seatText = seatLabel(controller_.seatForSide(mover));
    QString sideText = sideLabel(mover);
    if (scoreText.isEmpty()) {
        return QString("%1: %2 (%3)").arg(seatText, coord, sideText);
    }
    return QString("%1: %2 (%3, оценка %4)").arg(seatText, coord, sideText, scoreText);
}

void MainWindow::updateAiMoveLabel(Player mover, const Coord& move, const QString& scoreText)
{
    QString line = formatMoveLine(mover, move, scoreText);
    ui->labelAiMove->setText(line);
    addRecentMove(mover, move, scoreText);
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

void MainWindow::addRecentMove(Player mover, const Coord& move, const QString& scoreText)
{
    QString line = formatMoveLine(mover, move, scoreText);
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
    bool memo = ui->checkMemo->isChecked();
    int timeLimit = -1;

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
    Board b = controller_.boardSnapshot();
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
    Seat seat = controller_.seatToMove();

    int ticket = aiSearchTicket_;
    aiWatcher_.setFuture(QtConcurrent::run([this, seat, aiPlayer, depth, memo, timeLimit, ticket]() {AiSearchResult out; MoveEvaluation bestSoFar(Coord(-1, -1), std::numeric_limits<int>::min()); AIStatistics stats;  out.result = controller_.findBestMoveForSeat(seat, aiPlayer, depth, memo, stats, &bestSoFar, &aiCancelFlag_, timeLimit); out.best   = bestSoFar; out.stats  = stats; out.cancelled = aiCancelFlag_.load(std::memory_order_relaxed);  if (ticket != sessionId_.load(std::memory_order_relaxed)) {out.result = MoveEvaluation(Coord(-1, -1), std::numeric_limits<int>::min()); out.best   = out.result; } return out; }));

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

    updateAiMoveLabel(aiSearchPlayer_, chosen.move, QString::number(chosen.score));
    refreshBoardView();
    updateStatusLabels();

    QString coord = coordToHuman(chosen.move.row(), chosen.move.col());
    QString cancelNote = cancelled ? " Поиск прерван, использован лучший найденный ход." : "";
    QString depthNote;
    if (dynamicDepthMode_) {
        depthNote = QString(" Достигнутая глубина: %1.").arg(pack.stats.completedDepth);
    }
    detailedStats_ = QString("Ход %1 (%2): %3 (оценка %4). Узлов %5, кеш hits %6, время %7 мс.%8%9")
                         .arg(seatLabel(controller_.seatForSide(aiSearchPlayer_)))
                         .arg(sideLabel(aiSearchPlayer_))
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
        autoPlayAiIfNeeded();
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

    if (currentHintTask_ == HintTask::Hint) {
        QString line = formatMoveLine(hintPlayer_, chosen.move, QString::number(chosen.score));
        QString text = QString("Рекомендация: %1.").arg(line);

        detailedStats_ = QString("Подсказка: %1. Узлов %2, сгенерировано %3, кеш hits %4, misses %5, время %6 мс.")
                             .arg(line)
                             .arg(static_cast<qulonglong>(pack.stats.nodesVisited))
                             .arg(static_cast<qulonglong>(pack.stats.nodesGenerated))
                             .arg(static_cast<qulonglong>(pack.stats.cacheHits))
                             .arg(static_cast<qulonglong>(pack.stats.cacheMisses))
                             .arg(static_cast<long long>(pack.stats.timeMs));

        setBigInfo(text);
        showPopupMessage(text, QMessageBox::Information);
    } else if (currentHintTask_ == HintTask::HumanEval) {
        
        int scoreForMover = -chosen.score;
        QString line = formatMoveLine(evalMoverSide_, evalMoveForHint_, QString::number(scoreForMover));
        QString text = QString("%1.").arg(line);
        detailedStats_ = QString("Оценка хода: %1. Узлов %2, сгенерировано %3, кеш hits %4, misses %5, время %6 мс.")
                             .arg(line)
                             .arg(static_cast<qulonglong>(pack.stats.nodesVisited))
                             .arg(static_cast<qulonglong>(pack.stats.nodesGenerated))
                             .arg(static_cast<qulonglong>(pack.stats.cacheHits))
                             .arg(static_cast<qulonglong>(pack.stats.cacheMisses))
                             .arg(static_cast<long long>(pack.stats.timeMs));
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
    syncHumanSideFromSeat();

    if (controller_.isGameOver()) {
        ui->labelCurrentPlayer->setText("-");
        setBigInfo("Партия завершена");
        updateGameStateLabel(false);
    } else {
        Player p = controller_.currentPlayer();
        Seat seat = controller_.seatToMove();
        QString whoText = QStringLiteral("«ходит %1 (%2)»")
                              .arg(seatLabel(seat))
                              .arg(sideLabel(p));
        ui->labelCurrentPlayer->setText(whoText);
        updateGameStateLabel(true);
    }

    if (controller_.mode() == GameMode::LinesScore) {
        ui->labelLinesX->setText(QString::number(controller_.creditedLinesX()));
        ui->labelLinesO->setText(QString::number(controller_.creditedLinesO()));
        ui->labelScore->setText(QString::number(controller_.score()));
    } else {
        Board boardSnapshot = controller_.boardSnapshot();
        int linesX = GameController::countLinesFor(boardSnapshot,
                                                   CellState::X);
        int linesO = GameController::countLinesFor(boardSnapshot,
                                                   CellState::O);
        ui->labelLinesX->setText(QString::number(linesX));
        ui->labelLinesO->setText(QString::number(linesO));
        ui->labelScore->setText(QString::number(linesO - linesX));
    }

    updateOpeningUiState();
    updateTurnIndicator();
}




void MainWindow::showGameOverMessage()
{
    Board b = controller_.boardSnapshot();
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

void MainWindow::autoPlayAiIfNeeded()
{
    if (hintInProgress_) return;
    if (currentGameType_ != GameType::HumanVsAI) return;
    if (!autoAiEnabled_) return;
    if (aiVsAiStepMode_) return;
    if (controller_.isGameOver()) return;
    if (aiSearchInProgress_) return;

    if (resolveOpeningChoiceIfNeeded()) {
        refreshBoardView();
        updateStatusLabels();
        if (controller_.isGameOver()) return;
    }
    if (isOpeningChoicePhase(controller_.openingPhase())) return;
    if (!isCurrentSeatAi()) return;

    startAsyncAiMove(controller_.currentPlayer());
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

    if (isOpeningChoicePhase(controller_.openingPhase())) {
        if (resolveOpeningChoiceIfNeeded()) {
            refreshBoardView();
            updateStatusLabels();
            autoPlayAiIfNeeded();
        } else {
            showPopupMessage("Сейчас требуется выбор открытия. Сделайте выбор.");
        }
        return;
    }

    if (currentGameType_ == GameType::HumanVsAI && isCurrentSeatAi()) {
        Player aiPlayer = controller_.currentPlayer();
        showPopupMessage(QString("Сейчас ход %1 (%2). Нажмите \"Ход ИИ\" или включите автоход.")
                             .arg(seatLabel(controller_.seatForSide(aiPlayer)))
                             .arg(sideLabel(aiPlayer)));
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
    setLastMove(Coord(row, col));
    addRecentMove(mover, Coord(row, col), moveScore);
    if (!hintLine_.isEmpty()) {
        hintLine_.clear();
        refreshBigInfoDisplay();
    }

    refreshBoardView();
    updateStatusLabels();

    if (controller_.isGameOver()) {
        ui->labelStatus->setText("Игра не идёт");
        showGameOverMessage();
    } else {
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

    if (ui->groupBoxAdvanced->isChecked()) {
        applyAdvancedSettingsFromUi();
    } else {
        applyEnginePresetFromUi();
    }

    openingRule_ = openingRuleFromUi();
    controller_.setOpeningRule(openingRule_);
    controller_.newGame(rows, cols, winLen, mode, openingRule_);

    currentGameType_    = gameTypeFromUI();
    currentAIVsAISpeed_ = aiVsAiSpeedFromUI();
    autoAiEnabled_      = ui->checkAutoAi->isChecked();
    humanSeat_          = (ui->comboPlayerSide->currentIndex() == 0) ? Seat::A : Seat::B;
    syncHumanSideFromSeat();
    dynamicDepthMode_   = ui->checkDynamicDepth->isChecked();
    timeLimitMs_        = ui->spinTimeLimit->value() * 1000;
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
    updateOpeningUiState();
    showOpeningRuleInfoDialog();

    if (currentGameType_ == GameType::HumanVsHuman) {
        ui->labelStatus->setText("Игра идёт");
        return;
    }

    if (currentGameType_ == GameType::HumanVsAI) {
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

    if (isOpeningChoicePhase(controller_.openingPhase())) {
        if (resolveOpeningChoiceIfNeeded()) {
            refreshBoardView();
            updateStatusLabels();
            autoPlayAiIfNeeded();
        } else {
            showPopupMessage("Сейчас требуется выбор открытия. Сделайте выбор.");
        }
        return;
    }

    if (!isCurrentSeatAi()) {
        showPopupMessage("Сейчас ход игрока. Сначала сделайте ход, затем можно запросить ход ИИ.");
        return;
    }

    startAsyncAiMove(controller_.currentPlayer());
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
    if (isOpeningChoicePhase(controller_.openingPhase())) {
        showPopupMessage("Сейчас требуется выбор открытия. Подсказка будет доступна после него.", QMessageBox::Information);
        return;
    }
    if (isCurrentSeatAi()) {
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

void MainWindow::onAutoAiToggled(Qt::CheckState state)
{
    autoAiEnabled_ = (state != Qt::Unchecked);
    updateGameTypeUiState();
    autoPlayAiIfNeeded();
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


void MainWindow::applyEnginePresetFromUi()
{
    EnginePreset preset =
        (ui->comboEnginePreset->currentIndex() == 1)
            ? EnginePreset::Strict
            : EnginePreset::Fast;
    controller_.setEnginePreset(preset, ui->spinBoardRows->value(), ui->spinBoardCols->value());
    syncAdvancedSettingsUi(preset);
}

void MainWindow::applyAdvancedSettingsFromUi()
{
    int modeIdx = ui->comboMoveGenMode->currentIndex();
    MoveGenMode genMode = MoveGenMode::Hybrid;
    if (modeIdx == 0) genMode = MoveGenMode::Full;
    else if (modeIdx == 1) genMode = MoveGenMode::Frontier;
    controller_.setMoveGenMode(genMode);
    controller_.setUseLMR(ui->checkUseLmr->isChecked());
    controller_.setUseExtensions(ui->checkUseExtensions->isChecked());
}

void MainWindow::syncAdvancedSettingsUi(EnginePreset preset)
{
    QSignalBlocker blockGen(ui->comboMoveGenMode);
    QSignalBlocker blockLmr(ui->checkUseLmr);
    QSignalBlocker blockExt(ui->checkUseExtensions);

    MoveGenMode genMode = MoveGenMode::Hybrid;
    bool lmr = true;
    bool ext = true;
    bool perfect = false;
    controller_.getPresetSettings(preset, genMode, lmr, ext, perfect);
    Q_UNUSED(perfect);
    int area = ui->spinBoardRows->value() * ui->spinBoardCols->value();
    if (preset == EnginePreset::Strict && area >= 64) {
        genMode = MoveGenMode::Hybrid;
        lmr = true;
        ext = false;
    }
    int genIndex = 2;
    if (genMode == MoveGenMode::Full) {
        genIndex = 0;
    } else if (genMode == MoveGenMode::Frontier) {
        genIndex = 1;
    }
    ui->comboMoveGenMode->setCurrentIndex(genIndex);
    ui->checkUseLmr->setChecked(lmr);
    ui->checkUseExtensions->setChecked(ext);
}

void MainWindow::onEnginePresetChanged(int index)
{
    Q_UNUSED(index);
    applyEnginePresetFromUi();
}

void MainWindow::onAdvancedToggled(bool checked)
{
    if (checked) {
        applyAdvancedSettingsFromUi();
    } else {
        applyEnginePresetFromUi();
    }
}

void MainWindow::onMoveGenModeChanged(int index)
{
    Q_UNUSED(index);
    if (ui->groupBoxAdvanced->isChecked()) {
        applyAdvancedSettingsFromUi();
    }
}

void MainWindow::onUseLmrToggled(Qt::CheckState state)
{
    Q_UNUSED(state);
    if (ui->groupBoxAdvanced->isChecked()) {
        applyAdvancedSettingsFromUi();
    }
}

void MainWindow::onUseExtensionsToggled(Qt::CheckState state)
{
    Q_UNUSED(state);
    if (ui->groupBoxAdvanced->isChecked()) {
        applyAdvancedSettingsFromUi();
    }
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

    Player who = r.info.player;
    const AIStatistics &s = r.info.stats;

    if (r.info.isSwap) {
        QString seatText = seatLabel(r.info.seat);
        QString sideText = sideLabel(who);
        QString action = r.info.action.empty()
                             ? QStringLiteral("выбор открытия")
                             : QString::fromStdString(r.info.action);
        detailedStats_ = QString("%1 (%2) сделал выбор открытия: %3.")
                             .arg(seatText)
                             .arg(sideText)
                             .arg(action);
        if (controller_.swapUsed()) {
            detailedStats_ += " В этой партии менялись стороны.";
        }
        setBigInfo(detailedStats_);
        ui->labelStatus->setText("Игра идёт");

        
        refreshBoardView();
        updateStatusLabels();
    } else {
        detailedStats_ = QString(
                             "%1 (%2) ход %3 (оценка %4). Узлов %5, сгенерировано %6, кеш hits %7, misses %8, время %9 мс.")
                             .arg(seatLabel(r.info.seat))
                             .arg(sideLabel(who))
                             .arg(coordToHuman(r.info.move.row(), r.info.move.col()))
                             .arg(r.info.evalScore)
                             .arg(static_cast<qulonglong>(s.nodesVisited))
                             .arg(static_cast<qulonglong>(s.nodesGenerated))
                             .arg(static_cast<qulonglong>(s.cacheHits))
                             .arg(static_cast<qulonglong>(s.cacheMisses))
                             .arg(static_cast<long long>(s.timeMs));
        if (controller_.swapUsed()) {
            detailedStats_ += " В этой партии менялись стороны.";
        }

        
        setLastMove(r.info.move);
        updateAiMoveLabel(who, r.info.move, QString::number(r.info.evalScore));

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
        Board boardSnapshot = controller_.boardSnapshot();
        linesX = GameController::countLinesFor(boardSnapshot, CellState::X);
        linesO = GameController::countLinesFor(boardSnapshot, CellState::O);
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
