/**
 * @file terminal.c
 * @brief 跨平台终端适配层实现
 */

#include "terminal.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/time.h>
#endif

/* 静态变量保存终端状态 */
#ifdef _WIN32
static HANDLE hConsoleIn = INVALID_HANDLE_VALUE;
static HANDLE hConsoleOut = INVALID_HANDLE_VALUE;
static DWORD oldInMode;
static DWORD oldOutMode;
#else
static struct termios oldTermios;
static int termiosSaved = 0;
#endif

/* 终端是否已初始化 */
static int terminalInitialized = 0;

TermResult terminal_init(void) {
#ifdef _WIN32
    /* Windows: 设置控制台代码页 */
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);

    /* 获取控制台句柄 */
    hConsoleIn = GetStdHandle(STD_INPUT_HANDLE);
    hConsoleOut = GetStdHandle(STD_OUTPUT_HANDLE);

    if (hConsoleIn == INVALID_HANDLE_VALUE || hConsoleOut == INVALID_HANDLE_VALUE) {
        return TERM_ERR_INIT_FAILED;
    }

    /* 保存并设置原始输入模式 */
    GetConsoleMode(hConsoleIn, &oldInMode);
    SetConsoleMode(hConsoleIn, oldInMode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT));

    /* 启用虚拟终端序列（ANSI 颜色） */
    GetConsoleMode(hConsoleOut, &oldOutMode);
    DWORD dwMode = oldOutMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hConsoleOut, dwMode);

#else
    /* POSIX: 设置 locale */
    setlocale(LC_ALL, "en_US.UTF-8");

    /* 保存终端属性并设置原始模式 */
    if (tcgetattr(STDIN_FILENO, &oldTermios) != 0) {
        return TERM_ERR_INIT_FAILED;
    }
    termiosSaved = 1;
#endif

    terminalInitialized = 1;
    return TERM_OK;
}

TermResult terminal_restore(void) {
    if (!terminalInitialized) {
        return TERM_OK;
    }

#ifdef _WIN32
    if (hConsoleIn != INVALID_HANDLE_VALUE) {
        SetConsoleMode(hConsoleIn, oldInMode);
    }
    if (hConsoleOut != INVALID_HANDLE_VALUE) {
        SetConsoleMode(hConsoleOut, oldOutMode);
    }
#else
    if (termiosSaved) {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldTermios);
    }
#endif

    terminalInitialized = 0;
    return TERM_OK;
}

void terminal_set_utf8(void) {
#ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#else
    setlocale(LC_ALL, "en_US.UTF-8");
#endif
}

void terminal_hide_cursor(void) {
    printf("\033[?25l");
    fflush(stdout);
}

void terminal_show_cursor(void) {
    printf("\033[?25h");
    fflush(stdout);
}

void terminal_clear(void) {
    printf("\033[2J\033[H");
    fflush(stdout);
}

void terminal_move_cursor(int row, int col) {
    printf("\033[%d;%dH", row, col);
    fflush(stdout);
}

void terminal_sleep_ms(int ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    usleep((unsigned int)ms * 1000);
#endif
}

int terminal_kbhit(void) {
#ifdef _WIN32
    return _kbhit();
#else
    struct timeval tv = {0, 0};
    fd_set readset;
    FD_ZERO(&readset);
    FD_SET(STDIN_FILENO, &readset);
    return select(STDIN_FILENO + 1, &readset, NULL, NULL, &tv) > 0;
#endif
}

int terminal_getch(void) {
#ifdef _WIN32
    return _getch();
#else
    /* POSIX: 使用 termios 原始模式 */
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    int c = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return c;
#endif
}
