# libSceLibreSsl3

## remove `A-Hs-8mau+8`

## change [Additional exports]

- [Additional exports](../../../core/libs/prx/libSceLibreSsl3/AdditionalExports.cpp) declare 52 still-unimplemented callable exports: 49 named and 3 known only by NID. These entries throw. Parameterless `int` declarations are unknown-signature placeholders, not verified ABI contracts. Names/NIDs match the 13.60 export table. The fourteen libtls lifecycle/config exports are implemented in `LibTls.cpp`; title compatibility beyond preconnection behavior remains unverified.

## add Functional

- [`i2d_PUBKEY`](../../../core/libs/prx/libSceLibreSsl3/src/KeyDer.cpp) (libSceLibreSsl3) - nothing is checked on the console; the prototype comes from the LibreSSL name. EC groups without `OPENSSL_EC_NAMED_CURVE` (the libSceLibreSsl default), RSA-PSS, DSA, DH, X25519 and Ed25519 keys throw, and a faulted guest key or output pointer throws through `NotImplemented_nid_no_patch`. Otherwise it follows LibreSSL 4.0.0 `x_pubkey.c`, `tasn_enc.c`, `rsa_ameth.c` and `ec_ameth.c`: RSA and RSA2 keys encode `rsaEncryption` with NULL parameters, EC keys the curve OID and the point in the key's conversion form; a null key returns 0; an untyped key queues `X509_R_UNSUPPORTED_ALGORITHM`, HMAC and CMAC `X509_R_METHOD_NOT_SUPPORTED`, failed encodings `X509_R_PUBLIC_KEY_ENCODE_ERROR`, returning -1. A null `*pp` gets a guest-heap buffer; a set one is written and advanced. Closes when: compared with the console and the remaining key types and explicit EC parameters are encoded.
