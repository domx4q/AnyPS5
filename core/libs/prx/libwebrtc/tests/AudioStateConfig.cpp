#include "prx/libwebrtc/libwebrtc.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc AudioState::Config check failed at line %u\n", location.line());
    std::abort();
}

void TestConstructorClearsEveryReference() {
    WebRtcAudioStateConfig config;
    std::memset(&config, 0xA5, sizeof(config));
    _ZN6webrtc10AudioState6ConfigC1Ev_nid_postfix(&config);
    Require(config.audioMixer == nullptr);
    Require(config.audioProcessing == nullptr);
    Require(config.audioDeviceModule == nullptr);
    Require(config.asyncAudioProcessingFactory == nullptr);
}

void TestNullConfigThrows() {
    try {
        _ZN6webrtc10AudioState6ConfigC1Ev_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false);
}

struct GuardedPage {
    void* base = nullptr;
    std::size_t size = 0;

    GuardedPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        base = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
        size = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        base = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (base == MAP_FAILED)
            base = nullptr;
#endif
        Require(base != nullptr);
    }

    ~GuardedPage() {
        if (!base) return;
#ifdef _WIN32
        VirtualFree(base, 0, MEM_RELEASE);
#else
        munmap(base, size);
#endif
    }

    template<typename T> T* As() const { return static_cast<T*>(base); }

    void SetReadOnly() const {
#ifdef _WIN32
        DWORD prev = 0;
        Require(VirtualProtect(base, size, PAGE_READONLY, &prev) != 0);
#else
        Require(mprotect(base, size, PROT_READ) == 0);
#endif
    }

    void SetInaccessible() const {
#ifdef _WIN32
        DWORD prev = 0;
        Require(VirtualProtect(base, size, PAGE_NOACCESS, &prev) != 0);
#else
        Require(mprotect(base, size, PROT_NONE) == 0);
#endif
    }
};

bool ThrowsInvalidArgument(WebRtcAudioStateConfig* config) {
    try {
        _ZN6webrtc10AudioState6ConfigC1Ev_nid_postfix(config);
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void TestGuardedConfigThrows() {
    {
        GuardedPage page;
        std::memset(page.base, 0xA5, sizeof(WebRtcAudioStateConfig));
        page.SetReadOnly();
        Require(ThrowsInvalidArgument(page.As<WebRtcAudioStateConfig>()));
        Require(page.As<unsigned char>()[0] == 0xA5);
    }
    {
        GuardedPage page;
        page.SetInaccessible();
        Require(ThrowsInvalidArgument(page.As<WebRtcAudioStateConfig>()));
    }
}

}

int main() {
    TestConstructorClearsEveryReference();
    TestNullConfigThrows();
    TestGuardedConfigThrows();
    return 0;
}
