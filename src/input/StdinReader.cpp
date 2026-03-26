#include "StdinReader.h"

#ifdef _WIN32
#include <conio.h>
#else
#include <cstdio>
#include <unistd.h>
#include <sys/select.h>
#endif

#include <iostream>

namespace bbfx {

std::optional<std::string> StdinReader::poll() {
#ifdef _WIN32
    while (_kbhit()) {
        int ch = _getch();

        if (ch == '\r' || ch == '\n') {
            // Enter pressed — return the line
            std::cout << std::endl;
            std::string line = mBuffer;
            mBuffer.clear();
            return line;
        }
        else if (ch == 8 || ch == 127) {
            // Backspace
            if (!mBuffer.empty()) {
                mBuffer.pop_back();
                // Erase character on terminal
                std::cout << "\b \b" << std::flush;
            }
        }
        else if (ch == 0 || ch == 0xE0) {
            // Extended key (arrow keys, function keys) — read and discard
            _getch();
        }
        else if (ch >= 32) {
            // Printable character
            mBuffer.push_back(static_cast<char>(ch));
            std::cout << static_cast<char>(ch) << std::flush;
        }
    }
    return std::nullopt;
#else
    // POSIX: use select() on stdin with zero timeout
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    struct timeval tv = {0, 0};

    while (select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) > 0) {
        int ch = fgetc(stdin);
        if (ch == EOF) return std::nullopt;

        if (ch == '\n') {
            std::string line = mBuffer;
            mBuffer.clear();
            return line;
        }
        else if (ch == 127 || ch == 8) {
            if (!mBuffer.empty()) mBuffer.pop_back();
        }
        else if (ch >= 32) {
            mBuffer.push_back(static_cast<char>(ch));
        }

        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        tv = {0, 0};
    }
    return std::nullopt;
#endif
}

} // namespace bbfx
