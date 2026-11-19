# libkernel_sys

## change [pthread_get_specificarray_np]

- [pthread_get_specificarray_np](../../../core/libs/prx/libkernel_sys/Export.Discovery.cpp) (libkernel_sys) - returns EINVAL (22) for non-positive total, key below 1 or key range exceeding 512, EFAULT (14) for a null or unwritable output with `errno` kept, and otherwise collects the thread-specific data via scePthreadGetspecific and writes it with one `GuestCopyOut_nid_no_patch`. These argument checks, the 512-key bound and the EFAULT for an unwritable output are an unverified host model; console runtime behaviour was not independently measured.
