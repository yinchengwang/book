#include <gtest/gtest.h>

extern "C" {
#include <g2048_core.h>
}

namespace {

/** 设置固定棋盘（绕过随机） */
void set_board(G2048Game *g, const int board[4][4]) {
    memset(g, 0, sizeof(*g));
    for (int r = 0; r < G2048_SIZE; r++)
        for (int c = 0; c < G2048_SIZE; c++)
            g->board[r][c] = board[r][c];
}

}  // namespace

// 1. CreateInitializesEmptyBoard — g2048_create 后棋盘非空（spawn_tile 生成了方块）
TEST(G2048CoreTest, CreateInitializesEmptyBoard) {
    G2048Game g;
    g2048_create(&g, 42);

    bool has_nonzero = false;
    for (int r = 0; r < G2048_SIZE; r++)
        for (int c = 0; c < G2048_SIZE; c++)
            if (g.board[r][c] != 0) has_nonzero = true;

    EXPECT_TRUE(has_nonzero);
}

// 2. SlideRowMergesTwoIdentical — [2,2,0,0] 向左得 [4,0,0,0], moved=true
TEST(G2048CoreTest, SlideRowMergesTwoIdentical) {
    G2048Game g;
    set_board(&g, (int[4][4]){{2, 2, 0, 0},
                             {0, 0, 0, 0},
                             {0, 0, 0, 0},
                             {0, 0, 0, 0}});

    g2048_move(&g, G2048_LEFT);

    EXPECT_EQ(g.board[0][0], 4);
    EXPECT_EQ(g.board[0][1], 0);
    EXPECT_EQ(g.board[0][2], 0);
    EXPECT_EQ(g.board[0][3], 0);
    EXPECT_TRUE(g.moved);
}

// 3. SlideRowWithThreeSame — [2,2,2,2] 向左得 [4,4,0,0]
TEST(G2048CoreTest, SlideRowWithThreeSame) {
    G2048Game g;
    set_board(&g, (int[4][4]){{2, 2, 2, 2},
                             {0, 0, 0, 0},
                             {0, 0, 0, 0},
                             {0, 0, 0, 0}});

    g2048_move(&g, G2048_LEFT);

    EXPECT_EQ(g.board[0][0], 4);
    EXPECT_EQ(g.board[0][1], 4);
    EXPECT_EQ(g.board[0][2], 0);
    EXPECT_EQ(g.board[0][3], 0);
    EXPECT_TRUE(g.moved);
}

// 4. SlideRowNoChangeOnZeros — [4,8,0,0] 向左无变化, moved=false
TEST(G2048CoreTest, SlideRowNoChangeOnZeros) {
    G2048Game g;
    set_board(&g, (int[4][4]){{4, 8, 0, 0},
                             {0, 0, 0, 0},
                             {0, 0, 0, 0},
                             {0, 0, 0, 0}});

    g2048_move(&g, G2048_LEFT);

    EXPECT_EQ(g.board[0][0], 4);
    EXPECT_EQ(g.board[0][1], 8);
    EXPECT_EQ(g.board[0][2], 0);
    EXPECT_EQ(g.board[0][3], 0);
    EXPECT_FALSE(g.moved);
}

// 5. MoveUpRotatesAndSlides — 列 [2,2,0,0] 向上合并到顶
TEST(G2048CoreTest, MoveUpRotatesAndSlides) {
    G2048Game g;
    set_board(&g, (int[4][4]){{2, 0, 0, 0},
                             {2, 0, 0, 0},
                             {0, 0, 0, 0},
                             {0, 0, 0, 0}});

    g2048_move(&g, G2048_UP);

    EXPECT_EQ(g.board[0][0], 4);
    EXPECT_EQ(g.board[0][1], 0);
    EXPECT_EQ(g.board[0][2], 0);
    EXPECT_EQ(g.board[0][3], 0);
    EXPECT_TRUE(g.moved);
}

// 6. CannotMoveWhenFullAndNoAdjacent — 满棋盘无相邻相同，can_move 返回 false
TEST(G2048CoreTest, CannotMoveWhenFullAndNoAdjacent) {
    G2048Game g;
    // 典型无空位且无相邻相同的满棋盘
    set_board(&g, (int[4][4]){{ 2,  4,  8, 16},
                             {16,  8,  4,  2},
                             { 2,  4,  8, 16},
                             {16,  8,  4,  2}});

    EXPECT_FALSE(g2048_can_move(&g));
}

// 7. HasWonTrueWhen2048Present — 棋盘含 2048, has_won 返回 true
TEST(G2048CoreTest, HasWonTrueWhen2048Present) {
    G2048Game g;
    set_board(&g, (int[4][4]){{0, 0, 0, 0},
                             {0, 0, 0, 0},
                             {0, 0, 0, 0},
                             {0, 0, 2048, 0}});

    EXPECT_TRUE(g2048_has_won(&g));
}

// 8. SnapshotCopiesAllFields — snapshot 复制 board/score/game_over/won 与原结构一致
TEST(G2048CoreTest, SnapshotCopiesAllFields) {
    G2048Game g;
    set_board(&g, (int[4][4]){{2, 4, 0, 0},
                             {0, 0, 0, 0},
                             {0, 0, 0, 0},
                             {0, 0, 0, 0}});
    g.score = 100;
    g.game_over = true;
    g.won = true;
    g.keep_going = true;
    g.moved = true;

    int snap_board[G2048_SIZE][G2048_SIZE];
    int snap_score = -1;
    bool snap_game_over = false;
    bool snap_won = false;
    g2048_snapshot(&g, snap_board, &snap_score, &snap_game_over, &snap_won);

    for (int r = 0; r < G2048_SIZE; r++)
        for (int c = 0; c < G2048_SIZE; c++)
            EXPECT_EQ(snap_board[r][c], g.board[r][c]);

    EXPECT_EQ(snap_score, g.score);
    EXPECT_EQ(snap_game_over, g.game_over);
    EXPECT_EQ(snap_won, g.won);
}
