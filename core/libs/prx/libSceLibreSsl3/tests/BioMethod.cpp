#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 BIO_METHOD check failed at line %u\n", location.line());
    std::abort();
}

const char Name[] = "test method";
int liveBlocks = 0;
int zeroedCallsBeforeFailure = -1;
int written = 0;
int destroyed = 0;
int payload = 0;

void* APS5_VABI Allocate(std::size_t bytes) {
    ++liveBlocks;
    return std::malloc(bytes ? bytes : 1);
}

void APS5_VABI Release(void* block) {
    if (block) --liveBlocks;
    std::free(block);
}

void* APS5_VABI AllocateZeroed(std::size_t count, std::size_t bytes) {
    if (zeroedCallsBeforeFailure == 0) {
        zeroedCallsBeforeFailure = -1;
        return nullptr;
    }
    if (zeroedCallsBeforeFailure > 0) --zeroedCallsBeforeFailure;
    ++liveBlocks;
    return std::calloc(count ? count : 1, bytes ? bytes : 1);
}

void* APS5_VABI Reallocate(void*, std::size_t) { return nullptr; }
void* APS5_VABI Align(std::size_t, std::size_t) { return nullptr; }
void* APS5_VABI Realign(void*, std::size_t, std::size_t) { return nullptr; }
int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { return 12; }

void InitializeHeap() {
    void* const api[10] {
        reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Release), reinterpret_cast<void*>(&AllocateZeroed),
        reinterpret_cast<void*>(&Reallocate), reinterpret_cast<void*>(&Align), reinterpret_cast<void*>(&Realign),
        reinterpret_cast<void*>(&PosixAlign),
    };
    ApplicationHeapRegister_nid_no_patch(api);
}

int APS5_VABI Write(LibreSsl::Bio*, const char*, int size) {
    written += size;
    return size;
}

int APS5_VABI Read(LibreSsl::Bio*, char*, int) { return 0; }

std::int64_t APS5_VABI Control(LibreSsl::Bio*, int command, std::int64_t argument, void*) { return command + argument; }

int APS5_VABI Create(LibreSsl::Bio* bio) {
    bio->initialized = 1;
    bio->data = &payload;
    return 1;
}

int APS5_VABI Destroy(LibreSsl::Bio*) {
    ++destroyed;
    return 1;
}

class ReadOnlyPage {
public:
    ReadOnlyPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        bytes = info.dwPageSize;
        data = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Require(data != nullptr);
#else
        const long page = sysconf(_SC_PAGESIZE);
        Require(page > 0);
        bytes = static_cast<std::size_t>(page);
        data = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(data != nullptr && data != MAP_FAILED);
#endif
    }

    ~ReadOnlyPage() {
#ifdef _WIN32
        VirtualFree(data, 0, MEM_RELEASE);
#else
        munmap(data, bytes);
#endif
    }

    LibreSsl::BioMethod* Set(const LibreSsl::BioMethod& value) {
        Require(sizeof(value) <= bytes);
        std::memcpy(data, &value, sizeof(value));
        return static_cast<LibreSsl::BioMethod*>(data);
    }

    void Protect() {
#ifdef _WIN32
        DWORD previous{};
        Require(VirtualProtect(data, bytes, PAGE_READONLY, &previous) != 0);
#else
        Require(mprotect(data, bytes, PROT_READ) == 0);
#endif
    }

private:
    void* data = nullptr;
    std::size_t bytes = 0;
};

template <typename TCall>
void Rejects(TCall call, std::source_location location = std::source_location::current()) {
    errno = EINVAL;
    bool threw = false;
    try { call(); } catch (const std::runtime_error&) { threw = true; }
    Require(threw && errno == EINVAL, location);
}

void GuestPointers() {
    auto* unmapped = reinterpret_cast<LibreSsl::BioMethod*>(0x10);
    Rejects([&] { BIO_meth_set_write_nid_postfix(unmapped, &Write); });
    Rejects([&] { BIO_meth_set_read_nid_postfix(unmapped, &Read); });
    Rejects([&] { BIO_meth_set_ctrl_nid_postfix(unmapped, &Control); });
    Rejects([&] { BIO_meth_set_create_nid_postfix(unmapped, &Create); });
    Rejects([&] { BIO_meth_set_destroy_nid_postfix(unmapped, &Destroy); });

    LibreSsl::BioMethod initial{};
    initial.type = 8;
    initial.name = Name;
    ReadOnlyPage page;
    LibreSsl::BioMethod* readOnly = page.Set(initial);
    page.Protect();
    Rejects([&] { BIO_meth_set_write_nid_postfix(readOnly, &Write); });
    Rejects([&] { BIO_meth_set_read_nid_postfix(readOnly, &Read); });
    Rejects([&] { BIO_meth_set_ctrl_nid_postfix(readOnly, &Control); });
    Rejects([&] { BIO_meth_set_create_nid_postfix(readOnly, &Create); });
    Rejects([&] { BIO_meth_set_destroy_nid_postfix(readOnly, &Destroy); });
    Require(std::memcmp(readOnly, &initial, sizeof(initial)) == 0);

    auto* method = BIO_meth_new_nid_postfix(9, Name);
    errno = EINVAL;
    Require(BIO_meth_set_write_nid_postfix(method, &Write) == 1 && method->write == &Write && errno == EINVAL);
    Require(method->type == 9 && method->name == Name && !method->read && !method->control && !method->create && !method->destroy);
    BIO_meth_free_nid_postfix(method);
}

}

int main() {
    InitializeHeap();
    {
        auto* method = BIO_meth_new_nid_postfix(0x400 | 25, Name);
        Require(method && method->type == (0x400 | 25) && method->name == Name);
        Require(!method->write && !method->read && !method->puts && !method->gets && !method->control);
        Require(!method->create && !method->destroy && !method->callbackControl);
        Require(liveBlocks == 1);
        free_nid_postfix(method);
        zeroedCallsBeforeFailure = 0;
        Require(BIO_meth_new_nid_postfix(1, Name) == nullptr && liveBlocks == 0);
    }
    {
        auto* method = BIO_meth_new_nid_postfix(2, nullptr);
        Require(method && liveBlocks == 1);
        BIO_meth_free_nid_postfix(method);
        Require(liveBlocks == 0);
        BIO_meth_free_nid_postfix(nullptr);
        Require(liveBlocks == 0);
    }
    {
        auto* method = BIO_meth_new_nid_postfix(3, Name);
        Require(BIO_meth_set_write_nid_postfix(method, &Write) == 1 && method->write == &Write);
        auto* bio = BIO_new_nid_postfix(method);
        Require(bio && bio->method == method);
        bio->initialized = 1;
        Require(BIO_write_nid_postfix(bio, "abc", 3) == 3 && written == 3 && bio->bytesWritten == 3);
        Require(BIO_free_nid_postfix(bio) == 1);
        Require(BIO_meth_set_write_nid_postfix(method, nullptr) == 1 && method->write == nullptr);
        BIO_meth_free_nid_postfix(method);
        Require(liveBlocks == 0);
    }
    {
        auto* method = BIO_meth_new_nid_postfix(4, Name);
        Require(BIO_meth_set_read_nid_postfix(method, &Read) == 1 && method->read == &Read && !method->write);
        BIO_meth_free_nid_postfix(method);
    }
    {
        auto* method = BIO_meth_new_nid_postfix(5, Name);
        Require(BIO_meth_set_ctrl_nid_postfix(method, &Control) == 1 && method->control == &Control);
        auto* bio = BIO_new_nid_postfix(method);
        Require(bio && BIO_ctrl_nid_postfix(bio, 7, 9, nullptr) == 16);
        Require(BIO_free_nid_postfix(bio) == 1);
        BIO_meth_free_nid_postfix(method);
    }
    {
        auto* method = BIO_meth_new_nid_postfix(6, Name);
        Require(BIO_meth_set_create_nid_postfix(method, &Create) == 1 && method->create == &Create);
        auto* bio = BIO_new_nid_postfix(method);
        Require(bio && bio->initialized == 1 && bio->data == &payload);
        Require(BIO_free_nid_postfix(bio) == 1);
        BIO_meth_free_nid_postfix(method);
    }
    {
        auto* method = BIO_meth_new_nid_postfix(7, Name);
        Require(BIO_meth_set_destroy_nid_postfix(method, &Destroy) == 1 && method->destroy == &Destroy);
        auto* bio = BIO_new_nid_postfix(method);
        Require(bio && destroyed == 0);
        Require(BIO_free_nid_postfix(bio) == 1 && destroyed == 1);
        BIO_meth_free_nid_postfix(method);
        Require(liveBlocks == 0);
    }
    GuestPointers();
    Require(liveBlocks == 0);
}
