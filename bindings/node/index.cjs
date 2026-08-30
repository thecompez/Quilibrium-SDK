'use strict';

const koffi = require('koffi');

function defaultLibrary() {
  if (process.env.QUILIBRIUM_SDK_LIB) return process.env.QUILIBRIUM_SDK_LIB;
  if (process.platform === 'win32') return 'quilibrium.dll';
  if (process.platform === 'darwin') return 'libquilibrium.dylib';
  return 'libquilibrium.so';
}

const lib = koffi.load(defaultLibrary());
const ql_sdk = koffi.opaque('ql_sdk');
const ql_sdk_p = koffi.pointer(ql_sdk);
const ql_buffer = koffi.struct('ql_buffer', { data: 'uint8_t *', size: 'size_t' });
const ql_response = koffi.struct('ql_response', { status_code: 'int32_t', body: ql_buffer });
const ql_error = koffi.struct('ql_error', { domain: 'int32_t', code: 'int32_t', http_status: 'int32_t', message: 'char *' });
const ql_config = koffi.struct('ql_sdk_config', {
  hypersnap_endpoint: 'char *',
  qstorage_endpoint: 'char *',
  qkms_endpoint: 'char *',
  protocol_endpoint: 'char *',
  access_key_id: 'char *',
  secret_access_key: 'char *',
  session_token: 'char *'
});

const versionFn = lib.func('const char * ql_version(void)');
const createFn = lib.func('ql_sdk * ql_sdk_create(const ql_sdk_config *config, _Out_ ql_error *error)');
const destroyFn = lib.func('void ql_sdk_destroy(ql_sdk *sdk)');
const responseFreeFn = lib.func('void ql_response_free(ql_response *response)');
const errorFreeFn = lib.func('void ql_error_free(ql_error *error)');
const userFn = lib.func('int ql_hypersnap_user_by_fid(ql_sdk *sdk, uint64_t fid, _Out_ ql_response *response, _Out_ ql_error *error)');
const getFn = lib.func('int ql_hypersnap_get(ql_sdk *sdk, const char *path, _Out_ ql_response *response, _Out_ ql_error *error)');
const postFn = lib.func('int ql_hypersnap_post(ql_sdk *sdk, const char *path, const uint8_t *data, size_t size, _Out_ ql_response *response, _Out_ ql_error *error)');
const kmsFn = lib.func('int ql_qkms_invoke(ql_sdk *sdk, const char *operation, const char *json_payload, _Out_ ql_response *response, _Out_ ql_error *error)');
const storagePutFn = lib.func('int ql_qstorage_put(ql_sdk *sdk, const char *bucket, const char *key, const uint8_t *data, size_t size, const char *content_type, _Out_ ql_response *response, _Out_ ql_error *error)');
const storageGetFn = lib.func('int ql_qstorage_get(ql_sdk *sdk, const char *bucket, const char *key, _Out_ ql_response *response, _Out_ ql_error *error)');
const nativeFn = lib.func('int ql_native_call(ql_sdk *sdk, int32_t service, const char *method, const uint8_t *data, size_t size, _Out_ ql_response *response, _Out_ ql_error *error)');

const NativeService = Object.freeze({
  Node: 0, Connectivity: 1, Global: 2, AppShard: 3,
  HypergraphComparison: 4, KeyRegistry: 5, Dispatch: 6,
  Mixnet: 7, Onion: 8, PubSubProxy: 9, DataIPC: 10, FerretProxy: 11
});

function decodeBody(response) {
  if (!response.body.data || Number(response.body.size) === 0) return Buffer.alloc(0);
  return Buffer.from(koffi.decode(response.body.data, 'uint8_t', Number(response.body.size)));
}

function finish(code, response, error) {
  try {
    if (code !== 0) {
      const message = error.message ? koffi.decode(error.message, 'char', -1) : 'Quilibrium SDK call failed';
      const e = new Error(message);
      e.domain = error.domain;
      e.code = error.code;
      e.httpStatus = error.http_status;
      throw e;
    }
    return { statusCode: response.status_code, body: decodeBody(response) };
  } finally {
    responseFreeFn(response);
    errorFreeFn(error);
  }
}

function bytes(value) {
  if (Buffer.isBuffer(value)) return value;
  if (value instanceof Uint8Array) return Buffer.from(value);
  if (typeof value === 'string') return Buffer.from(value);
  return Buffer.alloc(0);
}

class SDK {
  constructor(options = {}) {
    const config = {
      hypersnap_endpoint: options.hypersnapEndpoint ?? null,
      qstorage_endpoint: options.qstorageEndpoint ?? null,
      qkms_endpoint: options.qkmsEndpoint ?? null,
      protocol_endpoint: options.protocolEndpoint ?? null,
      access_key_id: options.accessKeyId ?? null,
      secret_access_key: options.secretAccessKey ?? null,
      session_token: options.sessionToken ?? null
    };
    const error = {};
    this._handle = createFn(config, error);
    if (!this._handle) {
      const message = error.message ? koffi.decode(error.message, 'char', -1) : 'Unable to create Quilibrium SDK';
      errorFreeFn(error);
      throw new Error(message);
    }
  }

  close() {
    if (this._handle) {
      destroyFn(this._handle);
      this._handle = null;
    }
  }

  userByFid(fid) {
    const r = {}, e = {};
    return finish(userFn(this._handle, BigInt(fid), r, e), r, e);
  }

  hypersnapGet(pathname) {
    const r = {}, e = {};
    return finish(getFn(this._handle, pathname, r, e), r, e);
  }

  hypersnapPost(pathname, payload = Buffer.alloc(0)) {
    const data = bytes(payload), r = {}, e = {};
    return finish(postFn(this._handle, pathname, data, data.length, r, e), r, e);
  }

  qkmsInvoke(operation, payload = {}) {
    const r = {}, e = {};
    const json = typeof payload === 'string' ? payload : JSON.stringify(payload);
    return finish(kmsFn(this._handle, operation, json, r, e), r, e);
  }

  qstoragePut(bucket, key, payload, contentType = 'application/octet-stream') {
    const data = bytes(payload), r = {}, e = {};
    return finish(storagePutFn(this._handle, bucket, key, data, data.length, contentType, r, e), r, e);
  }

  qstorageGet(bucket, key) {
    const r = {}, e = {};
    return finish(storageGetFn(this._handle, bucket, key, r, e), r, e);
  }

  nativeCall(service, method, payload = Buffer.alloc(0)) {
    const data = bytes(payload), r = {}, e = {};
    const response = finish(nativeFn(this._handle, service, method, data, data.length, r, e), r, e);
    return response.body;
  }
}

module.exports = { SDK, NativeService, version: () => koffi.decode(versionFn(), 'char', -1) };
