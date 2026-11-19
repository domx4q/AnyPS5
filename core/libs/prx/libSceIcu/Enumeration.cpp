#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/HostEnumeration.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

constexpr std::int32_t UnsupportedError = 16;

UEnumeration Load(const UEnumeration* enumeration, const char* caller) {
    UEnumeration copy{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&copy, enumeration, sizeof(copy));
    errno = savedErrno;
    if (!copied) throw std::invalid_argument(std::string(caller) + ": unreadable enumeration");
    return copy;
}

bool Failed(const UEnumeration* enumeration, std::int32_t* status, const char* caller) {
    if (!enumeration) return true;
    if (!status) APS5_INVALID_ARG_EX;
    return Icu::ReadEnumerationStatus(status, caller) > 0;
}

void Unsupported(std::int32_t* status, const char* caller) {
    Icu::WriteEnumerationStatus(status, 0, UnsupportedError, caller);
}

}

extern "C" {

void APS5_VABI uenum_close_nid_postfix(UEnumeration* enumeration) {
    if (!enumeration) return;
    const auto copy = Load(enumeration, __func__);
    if (!copy.close) {
        free_nid_postfix(enumeration);
        return;
    }
    if (copy.baseContext) free_nid_postfix(copy.baseContext);
    copy.close(enumeration);
}

std::int32_t APS5_VABI uenum_count_nid_postfix(UEnumeration* enumeration, std::int32_t* status) {
    if (Failed(enumeration, status, __func__)) return -1;
    const auto copy = Load(enumeration, __func__);
    if (copy.count) return copy.count(enumeration, status);
    Unsupported(status, __func__);
    return -1;
}

const char16_t* APS5_VABI uenum_unext_nid_postfix(UEnumeration* enumeration, std::int32_t* resultLength, std::int32_t* status) {
    if (Failed(enumeration, status, __func__)) return nullptr;
    const auto copy = Load(enumeration, __func__);
    if (copy.uNext) return copy.uNext(enumeration, resultLength, status);
    Unsupported(status, __func__);
    return nullptr;
}

const char* APS5_VABI uenum_next_nid_postfix(UEnumeration* enumeration, std::int32_t* resultLength, std::int32_t* status) {
    if (Failed(enumeration, status, __func__)) return nullptr;
    const auto copy = Load(enumeration, __func__);
    if (!copy.next) {
        Unsupported(status, __func__);
        return nullptr;
    }
    std::int32_t ignoredLength = 0;
    return copy.next(enumeration, resultLength ? resultLength : &ignoredLength, status);
}

}
