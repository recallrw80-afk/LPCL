// 终端上下键选择器：termios 原始模式 + ANSI 转义序列，无外部依赖
#include "tui_select.h"

#include <cstdio>
#include <cstring>
#include <poll.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>

namespace {

struct TermiosGuard {
    termios saved{};
    bool active = false;
    ~TermiosGuard() { restore(); }
    void restore() {
        if (active) { tcsetattr(STDIN_FILENO, TCSANOW, &saved); active = false; }
    }
};

TermiosGuard g_guard;
void onSignal(int) {
    g_guard.restore();
    _exit(130);
}

// 写一行到终端（换行用 \r\n，原始模式下 \n 不会自动回车）
void tuiWrite(const QByteArray &s) {
    (void)::write(STDOUT_FILENO, s.constData(), s.size());
}

void drawMenu(const QString &title, const QStringList &items, int current, bool first) {
    if (!first) {
        // 光标移回菜单顶部（标题行 + items.size() 行）
        tuiWrite("\x1b[" + QByteArray::number(items.size() + 1) + "A");
    }
    // 标题行
    tuiWrite("\x1b[2K\x1b[1m" + title.toUtf8() + "\x1b[0m\r\n");
    for (int i = 0; i < items.size(); ++i) {
        tuiWrite("\x1b[2K");
        if (i == current)
            tuiWrite("\x1b[7m> " + items[i].toUtf8() + "\x1b[0m\r\n");
        else
            tuiWrite("  " + items[i].toUtf8() + "\r\n");
    }
    tuiWrite("\x1b[2K\x1b[90m(↑/↓ 选择, Enter 确认, q/ESC 取消)\x1b[0m");
    fflush(stdout);
}

// 读一个键：返回值 0=上 1=下 2=Home 3=End 4=确认 -1=取消 -2=其他
int readKey() {
    char c;
    if (::read(STDIN_FILENO, &c, 1) != 1) return -1;
    if (c == 'q' || c == 'Q' || c == 3) return -1;   // q 或 Ctrl+C
    if (c == '\r' || c == '\n') return 4;
    if (c == 27) {  // ESC：等 40ms 看是不是转义序列；单独 ESC 直接取消
        pollfd pfd{STDIN_FILENO, POLLIN, 0};
        if (poll(&pfd, 1, 40) <= 0) return -1;
        char seq[2] = {0, 0};
        if (::read(STDIN_FILENO, &seq[0], 1) != 1) return -1;
        if (seq[0] != '[') return -1;
        if (::read(STDIN_FILENO, &seq[1], 1) != 1) return -1;
        switch (seq[1]) {
        case 'A': return 0;  // ↑
        case 'B': return 1;  // ↓
        case 'H': return 2;  // Home
        case 'F': return 3;  // End
        default: return -2;
        }
    }
    return -2;
}

} // namespace

int tuiSelect(const QString &title, const QStringList &items, int initial) {
    if (items.isEmpty()) return -1;

    // 切原始模式（非 TTY 直接失败——调用方应已用 isatty 判断）
    if (!isatty(STDIN_FILENO)) return -1;
    if (tcgetattr(STDIN_FILENO, &g_guard.saved) != 0) return -1;
    termios raw = g_guard.saved;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) return -1;
    g_guard.active = true;
    signal(SIGINT, onSignal);
    signal(SIGTERM, onSignal);

    int current = (initial >= 0 && initial < items.size()) ? initial : 0;
    int result = -1;
    bool first = true;

    drawMenu(title, items, current, first);
    first = false;

    for (;;) {
        int key = readKey();
        if (key == -1) { result = -1; break; }
        if (key == 4) { result = current; break; }
        if (key == 0 && current > 0) current--;
        else if (key == 1 && current < items.size() - 1) current++;
        else if (key == 2) current = 0;
        else if (key == 3) current = items.size() - 1;
        else continue;
        drawMenu(title, items, current, first);
    }

    // 结束后：清理菜单区，留一行干净的输出位置。
    // 此时光标在尾行（提示行）行尾；菜单共 标题+N条目+尾行 三部分的 N+2 行。
    tuiWrite("\r\x1b[2K");  // 先清掉光标所在的尾行
    tuiWrite("\x1b[" + QByteArray::number(items.size() + 1) + "A");  // 回到标题行
    for (int i = 0; i <= items.size(); ++i)
        tuiWrite("\x1b[2K" + QByteArray(i < items.size() ? "\x1b[1B" : ""));
    // 光标在最后一个条目行（已清），换行离开菜单区，后续输出从干净行开始
    tuiWrite("\r\n");

    g_guard.restore();
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    return result;
}
