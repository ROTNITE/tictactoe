#pragma once
#include "DynamicArray.hpp"
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <limits>

enum class CellState : char {
    Empty = ' ',
    X = 'X',
    O = 'O'
};


enum class GameMode {
    Classic,    
    LinesScore  
};

class Coord {
public:
    Coord() : row_(-1), col_(-1) {}
    Coord(int r, int c) : row_(r), col_(c) {}

    int row() const { return row_; }
    int col() const { return col_; }
    void setRow(int r) { row_ = r; }
    void setCol(int c) { col_ = c; }

    bool operator==(const Coord& other) const {
        return row_ == other.row_ && col_ == other.col_;
    }

    bool operator!=(const Coord& other) const {
        return !(*this == other);
    }

private:
    int row_;
    int col_;
};

class Board {
private:
    DynamicArray<CellState> cells_;
    int rows_;
    int cols_;
    int winLength_;
    int emptyCount_ = 0;

public:
    Board(int size = 3, int winLength = 3)
        : Board(size, size, winLength)
    {}

    Board(int rows, int cols, int winLength)
        : rows_(rows), cols_(cols), winLength_(winLength)
    {
        if (rows_ <= 0 || cols_ <= 0) {
            throw std::invalid_argument("Board dimensions must be positive");
        }
        if (winLength_ < 3 || winLength_ > std::min(rows_, cols_)) {
            throw std::invalid_argument("Invalid win length");
        }

        cells_.reserve(rows_ * cols_);
        for (int i = 0; i < rows_ * cols_; ++i) {
            cells_.push_back(CellState::Empty);
        }
        emptyCount_ = rows_ * cols_;
    }

    
    int getRows() const { return rows_; }
    int getCols() const { return cols_; }
    int getWinLength() const { return winLength_; }

    
    CellState get(int row, int col) const {
        if (row < 0 || row >= rows_ || col < 0 || col >= cols_) {
            throw std::out_of_range("Invalid coordinates");
        }
        return cells_[row * cols_ + col];
    }

    CellState get(const Coord& coord) const {
        return get(coord.row(), coord.col());
    }

    CellState getNoCheck(int row, int col) const {
        return cells_.unchecked(static_cast<size_t>(row * cols_ + col));
    }

    void set(int row, int col, CellState state) {
        if (row < 0 || row >= rows_ || col < 0 || col >= cols_) {
            throw std::out_of_range("Invalid coordinates");
        }
        int idx = row * cols_ + col;
        CellState prev = cells_.unchecked(static_cast<size_t>(idx));
        if (prev == state) {
            return;
        }
        if (prev == CellState::Empty && state != CellState::Empty) {
            --emptyCount_;
        } else if (prev != CellState::Empty && state == CellState::Empty) {
            ++emptyCount_;
        }
        cells_.unchecked(static_cast<size_t>(idx)) = state;
    }

    void set(const Coord& coord, CellState state) {
        set(coord.row(), coord.col(), state);
    }

    bool isEmpty(int row, int col) const {
        return get(row, col) == CellState::Empty;
    }

    bool isEmpty(const Coord& coord) const {
        return isEmpty(coord.row(), coord.col());
    }

    bool isFull() const {
        return emptyCount_ == 0;
    }

    DynamicArray<Coord> getEmptyCells() const {
        DynamicArray<Coord> result;
        for (int row = 0; row < rows_; ++row) {
            for (int col = 0; col < cols_; ++col) {
                if (cells_.unchecked(static_cast<size_t>(row * cols_ + col)) == CellState::Empty) {
                    result.push_back(Coord(row, col));
                }
            }
        }
        return result;
    }

    DynamicArray<Coord> getCandidateMoves(int radius = 1) const {
        DynamicArray<Coord> frontier;
        frontier.reserve(rows_ * cols_);
        static thread_local DynamicArray<int> mark;
        static thread_local int markStamp = 1;
        int total = rows_ * cols_;
        if (mark.size() != static_cast<size_t>(total)) {
            mark.clear();
            mark.reserve(static_cast<size_t>(total));
            for (int i = 0; i < total; ++i) mark.push_back(0);
            markStamp = 1;
        } else if (markStamp == std::numeric_limits<int>::max()) {
            markStamp = 1;
            for (size_t i = 0; i < mark.size(); ++i) {
                mark.unchecked(i) = 0;
            }
        } else {
            ++markStamp;
        }
        bool anyStone = false;
        for (int row = 0; row < rows_; ++row) {
            for (int col = 0; col < cols_; ++col) {
                if (cells_.unchecked(static_cast<size_t>(row * cols_ + col)) == CellState::Empty) continue;
                anyStone = true;
                for (int dr = -radius; dr <= radius; ++dr) {
                    for (int dc = -radius; dc <= radius; ++dc) {
                        if (dr == 0 && dc == 0) continue;
                        int rr = row + dr;
                        int cc = col + dc;
                        if (rr < 0 || rr >= rows_ || cc < 0 || cc >= cols_) continue;
                        if (cells_.unchecked(static_cast<size_t>(rr * cols_ + cc)) != CellState::Empty) continue;
                        int idx = rr * cols_ + cc;
                        size_t sidx = static_cast<size_t>(idx);
                        if (mark.unchecked(sidx) != markStamp) {
                            mark.unchecked(sidx) = markStamp;
                            frontier.push_back(Coord(rr, cc));
                        }
                    }
                }
            }
        }

        if (!anyStone) {
            return getEmptyCells();
        }
        if (frontier.size() == 0) {
            return getEmptyCells();
        }
        return frontier;
    }

    
    bool checkWin(CellState player) const {
        if (player == CellState::Empty) return false;

        
        for (int row = 0; row < rows_; ++row) {
            for (int col = 0; col <= cols_ - winLength_; ++col) {
                bool win = true;
                for (int k = 0; k < winLength_; ++k) {
                    if (getNoCheck(row, col + k) != player) {
                        win = false;
                        break;
                    }
                }
                if (win) return true;
            }
        }

        
        for (int col = 0; col < cols_; ++col) {
            for (int row = 0; row <= rows_ - winLength_; ++row) {
                bool win = true;
                for (int k = 0; k < winLength_; ++k) {
                    if (getNoCheck(row + k, col) != player) {
                        win = false;
                        break;
                    }
                }
                if (win) return true;
            }
        }

        
        for (int row = 0; row <= rows_ - winLength_; ++row) {
            for (int col = 0; col <= cols_ - winLength_; ++col) {
                bool win = true;
                for (int k = 0; k < winLength_; ++k) {
                    if (getNoCheck(row + k, col + k) != player) {
                        win = false;
                        break;
                    }
                }
                if (win) return true;
            }
        }

        
        for (int row = 0; row <= rows_ - winLength_; ++row) {
            for (int col = winLength_ - 1; col < cols_; ++col) {
                bool win = true;
                for (int k = 0; k < winLength_; ++k) {
                    if (getNoCheck(row + k, col - k) != player) {
                        win = false;
                        break;
                    }
                }
                if (win) return true;
            }
        }

        return false;
    }

    
    bool checkWinFromMove(const Coord& mv, CellState player) const {
        if (player == CellState::Empty) return false;
        if (mv.row() < 0 || mv.col() < 0) return false;
        int dirs[4][2] = { {1,0},{0,1},{1,1},{1,-1} };
        for (auto& d : dirs) {
            int dr = d[0], dc = d[1];
            int cnt = 1;
            int r = mv.row() + dr, c = mv.col() + dc;
            while (r >= 0 && r < rows_ && c >= 0 && c < cols_ && getNoCheck(r, c) == player) {
                ++cnt; r += dr; c += dc;
            }
            r = mv.row() - dr; c = mv.col() - dc;
            while (r >= 0 && r < rows_ && c >= 0 && c < cols_ && getNoCheck(r, c) == player) {
                ++cnt; r -= dr; c -= dc;
            }
            if (cnt >= winLength_) return true;
        }
        return false;
    }

    
    int countLinesFromMove(const Coord& mv, CellState who) const {
        if (who == CellState::Empty || mv.row() < 0 || mv.col() < 0) return 0;
        int total = 0;
        int dirs[4][2] = { {1,0},{0,1},{1,1},{1,-1} };
        for (auto& d : dirs) {
            int dr = d[0], dc = d[1];
            int count = 1;
            int r = mv.row() + dr, c = mv.col() + dc;
            while (r >= 0 && r < rows_ && c >= 0 && c < cols_ && getNoCheck(r, c) == who) {
                ++count; r += dr; c += dc;
            }
            r = mv.row() - dr; c = mv.col() - dc;
            while (r >= 0 && r < rows_ && c >= 0 && c < cols_ && getNoCheck(r, c) == who) {
                ++count; r -= dr; c -= dc;
            }
            if (count < winLength_) continue;
            int lineLen = count;
            int startR = mv.row(), startC = mv.col();
            int mvIndex = 0;
            while (startR - dr >= 0 && startR - dr < rows_ &&
                   startC - dc >= 0 && startC - dc < cols_ &&
                   getNoCheck(startR - dr, startC - dc) == who) {
                startR -= dr;
                startC -= dc;
                ++mvIndex;
            }
            int maxStart = lineLen - winLength_;
            int minS = std::max(0, mvIndex - (winLength_ - 1));
            int maxS = std::min(mvIndex, maxStart);
            if (maxS >= minS) {
                total += (maxS - minS + 1);
            }
        }
        return total;
    }


    void print() const {
        std::cout << "  ";
        for (int col = 0; col < cols_; ++col) {
            std::cout << col << " ";
        }
        std::cout << std::endl;

        for (int row = 0; row < rows_; ++row) {
            std::cout << row << " ";
            for (int col = 0; col < cols_; ++col) {
                std::cout << static_cast<char>(get(row, col));
                if (col < cols_ - 1) std::cout << "|";
            }
            std::cout << std::endl;

            if (row < rows_ - 1) {
                std::cout << "  ";
                for (int col = 0; col < cols_; ++col) {
                    std::cout << "-";
                    if (col < cols_ - 1) std::cout << "+";
                }
                std::cout << std::endl;
            }
        }
    }
};
