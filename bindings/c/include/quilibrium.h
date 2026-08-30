#ifndef QUILIBRIUM_C_SDK_H
#define QUILIBRIUM_C_SDK_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#  if defined(quilibrium_c_EXPORTS)
#    define QL_API __declspec(dllexport)
#  else
#    define QL_API __declspec(dllimport)
#  endif
#else
#  define QL_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ql_sdk ql_sdk;

/** Native Quilibrium protocol service selector used by ql_native_call. */
typedef enum ql_native_service {
    QL_NATIVE_NODE = 0,
    QL_NATIVE_CONNECTIVITY = 1,
    QL_NATIVE_GLOBAL = 2,
    QL_NATIVE_APP_SHARD = 3,
    QL_NATIVE_HYPERGRAPH_COMPARISON = 4,
    QL_NATIVE_KEY_REGISTRY = 5,
    QL_NATIVE_DISPATCH = 6,
    QL_NATIVE_MIXNET = 7,
    QL_NATIVE_ONION = 8,
    QL_NATIVE_PUBSUB_PROXY = 9,
    QL_NATIVE_DATA_IPC = 10,
    QL_NATIVE_FERRET_PROXY = 11
} ql_native_service;

/** Optional SDK configuration. Null fields use SDK defaults where available. */
typedef struct ql_sdk_config {
    const char* hypersnap_endpoint;
    const char* qstorage_endpoint;
    const char* qkms_endpoint;
    const char* protocol_endpoint;
    const char* access_key_id;
    const char* secret_access_key;
    const char* session_token;
} ql_sdk_config;

typedef struct ql_buffer {
    uint8_t* data;
    size_t size;
} ql_buffer;

typedef struct ql_response {
    int32_t status_code;
    ql_buffer body;
} ql_response;

typedef struct ql_error {
    int32_t domain;
    int32_t code;
    int32_t http_status;
    char* message;
} ql_error;

QL_API const char* ql_version(void);
QL_API ql_sdk* ql_sdk_create(const ql_sdk_config* config, ql_error* error);
QL_API void ql_sdk_destroy(ql_sdk* sdk);

QL_API int ql_hypersnap_user_by_fid(ql_sdk* sdk, uint64_t fid, ql_response* response, ql_error* error);
QL_API int ql_hypersnap_get(ql_sdk* sdk, const char* path, ql_response* response, ql_error* error);
QL_API int ql_hypersnap_post(ql_sdk* sdk, const char* path, const uint8_t* data, size_t size, ql_response* response, ql_error* error);
QL_API int ql_qkms_invoke(ql_sdk* sdk, const char* operation, const char* json_payload, ql_response* response, ql_error* error);
QL_API int ql_qstorage_put(ql_sdk* sdk, const char* bucket, const char* key, const uint8_t* data, size_t size, const char* content_type, ql_response* response, ql_error* error);
QL_API int ql_qstorage_get(ql_sdk* sdk, const char* bucket, const char* key, ql_response* response, ql_error* error);
QL_API int ql_native_call(ql_sdk* sdk, int32_t service, const char* method, const uint8_t* data, size_t size, ql_response* response, ql_error* error);

/** Releases buffers owned by a ql_response and resets it to zero. */
QL_API void ql_response_free(ql_response* response);
/** Releases strings owned by a ql_error and resets it to zero. */
QL_API void ql_error_free(ql_error* error);

#ifdef __cplusplus
}
#endif
#endif
