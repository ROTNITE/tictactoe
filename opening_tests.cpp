#include "GameController.hpp"
#include <iostream>

namespace {
int failures = 0;

void reportFailure(const char* testName, int line) {
    std::cerr << "FAIL: " << testName << " at line " << line << "\n";
    ++failures;
}

#define CHECK(testName, expr) \
    do { \
        if (!(expr)) { \
            reportFailure(testName, __LINE__); \
            return; \
        } \
    } while (0)

void testSwap2TakeX() {
    const char* name = "testSwap2TakeX";
    GameController gc(5, 5, 4, GameMode::LinesScore, OpeningRule::Swap2);
    CHECK(name, gc.openingPhase() == OpeningPhase::Swap2_A_Place1_X);
    CHECK(name, gc.seatToMove() == Seat::A);

    CHECK(name, gc.applyMove(0, 0) == MoveStatus::Ok);
    CHECK(name, gc.openingPhase() == OpeningPhase::Swap2_A_Place2_O);
    CHECK(name, gc.applyMove(0, 1) == MoveStatus::Ok);
    CHECK(name, gc.openingPhase() == OpeningPhase::Swap2_A_Place3_X);
    CHECK(name, gc.applyMove(0, 2) == MoveStatus::Ok);
    CHECK(name, gc.openingPhase() == OpeningPhase::Swap2_B_ChooseOption);
    CHECK(name, gc.isOpeningChoiceRequired());
    CHECK(name, gc.availableSwap2Options().size() == 3);

    CHECK(name, gc.chooseSwap2Option(Swap2Option::TakeX));
    CHECK(name, gc.openingPhase() == OpeningPhase::Normal);
    CHECK(name, gc.currentPlayer() == Player::O);
    CHECK(name, gc.seatToMove() == gc.seatForSide(Player::O));
    CHECK(name, gc.seatForSide(Player::X) == Seat::B);
}

void testSwap2ExtraO() {
    const char* name = "testSwap2ExtraO";
    GameController gc(5, 5, 4, GameMode::LinesScore, OpeningRule::Swap2);
    CHECK(name, gc.applyMove(0, 0) == MoveStatus::Ok);
    CHECK(name, gc.applyMove(0, 1) == MoveStatus::Ok);
    CHECK(name, gc.applyMove(0, 2) == MoveStatus::Ok);
    CHECK(name, gc.openingPhase() == OpeningPhase::Swap2_B_ChooseOption);

    CHECK(name, gc.chooseSwap2Option(Swap2Option::TakeO_AndPlaceExtraO));
    CHECK(name, gc.openingPhase() == OpeningPhase::Swap2_B_PlaceExtraO);
    CHECK(name, gc.currentPlayer() == Player::O);
    CHECK(name, gc.canPlaceStoneNow());

    CHECK(name, gc.applyMove(1, 0) == MoveStatus::Ok);
    CHECK(name, gc.openingPhase() == OpeningPhase::Normal);
    CHECK(name, gc.currentPlayer() == Player::X);
    CHECK(name, gc.seatToMove() == gc.seatForSide(Player::X));
}

void testSwap2PlaceTwoAndChoose() {
    const char* name = "testSwap2PlaceTwoAndChoose";
    GameController gc(5, 5, 4, GameMode::LinesScore, OpeningRule::Swap2);
    CHECK(name, gc.applyMove(0, 0) == MoveStatus::Ok);
    CHECK(name, gc.applyMove(0, 1) == MoveStatus::Ok);
    CHECK(name, gc.applyMove(0, 2) == MoveStatus::Ok);
    CHECK(name, gc.openingPhase() == OpeningPhase::Swap2_B_ChooseOption);

    CHECK(name, gc.chooseSwap2Option(Swap2Option::PlaceTwoAndGiveChoice));
    CHECK(name, gc.openingPhase() == OpeningPhase::Swap2_B_Place4_O);
    CHECK(name, gc.applyMove(1, 0) == MoveStatus::Ok);
    CHECK(name, gc.openingPhase() == OpeningPhase::Swap2_B_Place5_X);
    CHECK(name, gc.applyMove(1, 1) == MoveStatus::Ok);
    CHECK(name, gc.openingPhase() == OpeningPhase::Swap2_A_FinalChooseSide);
    CHECK(name, gc.availableSideChoices().size() == 2);
    CHECK(name, gc.chooseSwap2FinalSide(Player::X));
    CHECK(name, gc.openingPhase() == OpeningPhase::Normal);
    CHECK(name, gc.currentPlayer() == Player::O);
}

void testSwap2PlusFlow() {
    const char* name = "testSwap2PlusFlow";
    GameController gc(5, 5, 4, GameMode::LinesScore, OpeningRule::Swap2Plus);
    CHECK(name, gc.openingPhase() == OpeningPhase::Swap2P_A_Place1_X);
    CHECK(name, gc.applyMove(0, 0) == MoveStatus::Ok);
    CHECK(name, gc.openingPhase() == OpeningPhase::Swap2P_A_Place2_O);
    CHECK(name, gc.applyMove(0, 1) == MoveStatus::Ok);
    CHECK(name, gc.openingPhase() == OpeningPhase::Swap2P_B_ChooseSide);
    CHECK(name, gc.availableSideChoices().size() == 2);
    CHECK(name, gc.chooseSwap2PlusSideB(Player::X));
    CHECK(name, gc.openingPhase() == OpeningPhase::Swap2P_B_Place3_SelectedSide);
    CHECK(name, gc.applyMove(0, 2) == MoveStatus::Ok);
    CHECK(name, gc.openingPhase() == OpeningPhase::Swap2P_A_FinalChooseSide);
    CHECK(name, gc.chooseSwap2PlusFinalSideA(Player::O));
    CHECK(name, gc.openingPhase() == OpeningPhase::Normal);
    CHECK(name, gc.currentPlayer() == Player::O);
}
}

int main() {
    testSwap2TakeX();
    testSwap2ExtraO();
    testSwap2PlaceTwoAndChoose();
    testSwap2PlusFlow();

    if (failures == 0) {
        std::cout << "All opening tests passed.\n";
        return 0;
    }
    std::cerr << failures << " opening test(s) failed.\n";
    return 1;
}
