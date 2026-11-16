#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

static_assert(sizeof(LibreSslUiMethod) == 56);
static_assert(sizeof(LibreSslUi) == 40);
static_assert(offsetof(LibreSslUi, flags) == 32);
static_assert(sizeof(LibreSslUiString) == 64);
static_assert(offsetof(LibreSslUiString, resultBuffer) == 24);
static_assert(offsetof(LibreSslUiString, data) == 32);
static_assert(offsetof(LibreSslUiString, flags) == 56);

namespace {
constexpr int UiLibrary = 40;
constexpr int AnyFunction = 0xfff;
constexpr int ResultTooLarge = 100;
constexpr int ResultTooSmall = 101;
constexpr int NoResultBuffer = 105;
constexpr std::int32_t Redoable = 0x0001;
constexpr std::int32_t PromptString = 1;
constexpr std::int32_t VerifyString = 2;
constexpr std::int32_t BooleanString = 3;

void PutError(int reason) {
    LibreSslPutError_nid_no_patch(UiLibrary, AnyFunction, reason);
}

bool CopyGuestInPreservingErrno(void* destination, const void* source, std::size_t size) {
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(destination, source, size);
    errno = savedErrno;
    return ok;
}

bool CopyGuestOutPreservingErrno(void* destination, const void* source, std::size_t size) {
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(destination, source, size);
    errno = savedErrno;
    return ok;
}

bool CopyGuestStringPreservingErrno(const char* source, std::string& destination) {
    constexpr std::uintptr_t Chunk = 4096;
    char bytes[Chunk];
    destination.clear();
    for (auto address = reinterpret_cast<std::uintptr_t>(source);;) {
        const std::size_t count = Chunk - address % Chunk;
        if (!CopyGuestInPreservingErrno(bytes, reinterpret_cast<const void*>(address), count)) return false;
        const auto* end = static_cast<const char*>(std::memchr(bytes, 0, count));
        destination.append(bytes, end ? static_cast<std::size_t>(end - bytes) : count);
        if (end) return true;
        address += count;
    }
}

const std::string& Characters(const char* characters, std::string& copy, bool& copied, const char* function) {
    if (!characters) throw std::invalid_argument(std::string(function) + ": null boolean characters");
    if (!copied && !CopyGuestStringPreservingErrno(characters, copy)) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    copied = true;
    return copy;
}

void SetFlags(LibreSslUi* ui, std::int32_t flags, const char* function) {
    if (!CopyGuestOutPreservingErrno(reinterpret_cast<std::byte*>(ui) + offsetof(LibreSslUi, flags), &flags, sizeof(flags)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}
}

extern "C" {
LibreSslUiMethod* APS5_VABI UI_create_method_nid_postfix(char* name) {
    std::string copy;
    if (name && !CopyGuestStringPreservingErrno(name, copy)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    auto* method = static_cast<LibreSslUiMethod*>(malloc_nid_postfix(sizeof(LibreSslUiMethod)));
    if (!method) return nullptr;
    std::memset(method, 0, sizeof(LibreSslUiMethod));
    if (name) {
        method->name = static_cast<char*>(malloc_nid_postfix(copy.size() + 1));
        if (method->name) std::memcpy(method->name, copy.c_str(), copy.size() + 1);
    }
    return method;
}

void APS5_VABI UI_destroy_method_nid_postfix(LibreSslUiMethod* method) {
    if (!method) throw std::invalid_argument(std::string(__func__) + ": null method");
    free_nid_postfix(method->name);
    method->name = nullptr;
    free_nid_postfix(method);
}

int APS5_VABI UI_method_set_opener_nid_postfix(LibreSslUiMethod* method, UiSessionFunction opener) {
    if (!method) return -1;
    method->openSession = opener;
    return 0;
}

UiSessionFunction APS5_VABI UI_method_get_opener_nid_postfix(const LibreSslUiMethod* method) {
    return method ? method->openSession : nullptr;
}

int APS5_VABI UI_method_set_writer_nid_postfix(LibreSslUiMethod* method, UiStringFunction writer) {
    if (!method) return -1;
    method->writeString = writer;
    return 0;
}

UiStringFunction APS5_VABI UI_method_get_writer_nid_postfix(const LibreSslUiMethod* method) {
    return method ? method->writeString : nullptr;
}

int APS5_VABI UI_method_set_reader_nid_postfix(LibreSslUiMethod* method, UiStringFunction reader) {
    if (!method) return -1;
    method->readString = reader;
    return 0;
}

UiStringFunction APS5_VABI UI_method_get_reader_nid_postfix(const LibreSslUiMethod* method) {
    return method ? method->readString : nullptr;
}

int APS5_VABI UI_method_set_closer_nid_postfix(LibreSslUiMethod* method, UiSessionFunction closer) {
    if (!method) return -1;
    method->closeSession = closer;
    return 0;
}

UiSessionFunction APS5_VABI UI_method_get_closer_nid_postfix(const LibreSslUiMethod* method) {
    return method ? method->closeSession : nullptr;
}

int APS5_VABI UI_set_result_nid_postfix(LibreSslUi* ui, LibreSslUiString* string, const char* result) {
    if (!result) throw std::invalid_argument(std::string(__func__) + ": null result");
    if (!ui) throw std::invalid_argument(std::string(__func__) + ": null UI");
    std::string text;
    if (!CopyGuestStringPreservingErrno(result, text)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    const auto length = static_cast<int>(text.size());
    std::int32_t flags = 0;
    if (!CopyGuestInPreservingErrno(&flags, reinterpret_cast<const std::byte*>(ui) + offsetof(LibreSslUi, flags), sizeof(flags)))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    flags &= ~Redoable;
    SetFlags(ui, flags, __func__);
    if (!string) return -1;
    LibreSslUiString copy{};
    if (!CopyGuestInPreservingErrno(&copy, string, sizeof(copy))) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    switch (copy.type) {
    case PromptString:
    case VerifyString:
        if (length < copy.data.stringData.resultMinimum) {
            SetFlags(ui, flags | Redoable, __func__);
            PutError(ResultTooSmall);
            return -1;
        }
        if (length > copy.data.stringData.resultMaximum) {
            SetFlags(ui, flags | Redoable, __func__);
            PutError(ResultTooLarge);
            return -1;
        }
        if (!copy.resultBuffer) {
            PutError(NoResultBuffer);
            return -1;
        }
        if (!CopyGuestOutPreservingErrno(copy.resultBuffer, text.c_str(), static_cast<std::size_t>(length) + 1)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        break;
    case BooleanString: {
        if (!copy.resultBuffer) {
            PutError(NoResultBuffer);
            return -1;
        }
        char answer = '\0';
        if (!CopyGuestOutPreservingErrno(copy.resultBuffer, &answer, 1)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        std::string acceptedCopy;
        std::string cancelledCopy;
        bool acceptedCopied = false;
        bool cancelledCopied = false;
        for (const char character : text) {
            const auto& accepted = Characters(copy.data.booleanData.okCharacters, acceptedCopy, acceptedCopied, __func__);
            if (accepted.find(character) != std::string::npos) {
                answer = accepted[0];
                break;
            }
            const auto& cancelled = Characters(copy.data.booleanData.cancelCharacters, cancelledCopy, cancelledCopied, __func__);
            if (cancelled.find(character) != std::string::npos) {
                answer = cancelled[0];
                break;
            }
        }
        if (answer != '\0' && !CopyGuestOutPreservingErrno(copy.resultBuffer, &answer, 1)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        break;
    }
    default:
        break;
    }
    return 0;
}

}
