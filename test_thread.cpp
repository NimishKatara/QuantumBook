#include <windows.h>
#include <iostream>
#include <atomic>

std::atomic<bool> running(true);
std::atomic<int>  count(0);

DWORD WINAPI worker(LPVOID) {
    std::cout << "[thread] started\n" << std::flush;
    while (running.load()) {
        count.fetch_add(1);
        Sleep(1);
    }
    std::cout << "[thread] exiting, count=" << count.load() << "\n" << std::flush;
    return 0;
}

int main() {
    std::cout << "[main] launching thread\n" << std::flush;

    HANDLE h = CreateThread(nullptr, 0, worker, nullptr, 0, nullptr);
    if (h == nullptr) {
        std::cout << "[main] CreateThread FAILED: " << GetLastError() << "\n";
        return 1;
    }

    std::cout << "[main] thread launched, sleeping 100ms\n" << std::flush;
    Sleep(100);

    std::cout << "[main] signalling stop\n" << std::flush;
    running.store(false);

    WaitForSingleObject(h, INFINITE);
    CloseHandle(h);

    std::cout << "[main] done\n" << std::flush;
    return 0;
}