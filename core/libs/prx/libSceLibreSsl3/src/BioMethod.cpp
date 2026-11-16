#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <cerrno>
#include <cstddef>
#include <stdexcept>

namespace {

template <typename T>
int SetCallback(LibreSsl::BioMethod* method, std::size_t offset, T callback, const char* function) {
    const int saved = errno;
    const bool ok = GuestCopyOut_nid_no_patch(reinterpret_cast<unsigned char*>(method) + offset, &callback, sizeof(callback));
    errno = saved;
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return 1;
}

}

extern "C" {

LibreSsl::BioMethod* APS5_VABI BIO_meth_new_nid_postfix(int type, const char* name) {
    auto* method = static_cast<LibreSsl::BioMethod*>(calloc_nid_postfix(1, sizeof(LibreSsl::BioMethod)));
    if (!method) return nullptr;
    method->type = type;
    method->name = name;
    return method;
}

void APS5_VABI BIO_meth_free_nid_postfix(LibreSsl::BioMethod* method) {
    free_nid_postfix(method);
}

int APS5_VABI BIO_meth_set_write_nid_postfix(LibreSsl::BioMethod* method, decltype(LibreSsl::BioMethod::write) write) {
    if (!method) throw std::invalid_argument("BIO_meth_set_write requires a method");
    return SetCallback(method, offsetof(LibreSsl::BioMethod, write), write, __func__);
}

int APS5_VABI BIO_meth_set_read_nid_postfix(LibreSsl::BioMethod* method, decltype(LibreSsl::BioMethod::read) read) {
    if (!method) throw std::invalid_argument("BIO_meth_set_read requires a method");
    return SetCallback(method, offsetof(LibreSsl::BioMethod, read), read, __func__);
}

int APS5_VABI BIO_meth_set_ctrl_nid_postfix(LibreSsl::BioMethod* method, decltype(LibreSsl::BioMethod::control) ctrl) {
    if (!method) throw std::invalid_argument("BIO_meth_set_ctrl requires a method");
    return SetCallback(method, offsetof(LibreSsl::BioMethod, control), ctrl, __func__);
}

int APS5_VABI BIO_meth_set_create_nid_postfix(LibreSsl::BioMethod* method, decltype(LibreSsl::BioMethod::create) create) {
    if (!method) throw std::invalid_argument("BIO_meth_set_create requires a method");
    return SetCallback(method, offsetof(LibreSsl::BioMethod, create), create, __func__);
}

int APS5_VABI BIO_meth_set_destroy_nid_postfix(LibreSsl::BioMethod* method, decltype(LibreSsl::BioMethod::destroy) destroy) {
    if (!method) throw std::invalid_argument("BIO_meth_set_destroy requires a method");
    return SetCallback(method, offsetof(LibreSsl::BioMethod, destroy), destroy, __func__);
}

}
