"""Python binding for the Quilibrium C++ SDK stable C ABI."""

from __future__ import annotations

import ctypes
import ctypes.util
import json
import os
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Optional
from enum import IntEnum


class _Buffer(ctypes.Structure):
    _fields_ = [("data", ctypes.POINTER(ctypes.c_uint8)), ("size", ctypes.c_size_t)]


class _Response(ctypes.Structure):
    _fields_ = [("status_code", ctypes.c_int32), ("body", _Buffer)]


class _Error(ctypes.Structure):
    _fields_ = [
        ("domain", ctypes.c_int32),
        ("code", ctypes.c_int32),
        ("http_status", ctypes.c_int32),
        ("message", ctypes.c_char_p),
    ]


class _Config(ctypes.Structure):
    _fields_ = [
        ("hypersnap_endpoint", ctypes.c_char_p),
        ("qstorage_endpoint", ctypes.c_char_p),
        ("qkms_endpoint", ctypes.c_char_p),
        ("protocol_endpoint", ctypes.c_char_p),
        ("access_key_id", ctypes.c_char_p),
        ("secret_access_key", ctypes.c_char_p),
        ("session_token", ctypes.c_char_p),
    ]


@dataclass(frozen=True)
class Response:
    status_code: int
    body: bytes

    def json(self) -> Any:
        return json.loads(self.body.decode("utf-8"))




class NativeService(IntEnum):
    NODE = 0
    CONNECTIVITY = 1
    GLOBAL = 2
    APP_SHARD = 3
    HYPERGRAPH_COMPARISON = 4
    KEY_REGISTRY = 5
    DISPATCH = 6
    MIXNET = 7
    ONION = 8
    PUBSUB_PROXY = 9
    DATA_IPC = 10
    FERRET_PROXY = 11

class QuilibriumError(RuntimeError):
    def __init__(self, domain: int, code: int, message: str, http_status: int = 0):
        super().__init__(message)
        self.domain = domain
        self.code = code
        self.http_status = http_status


def _library_candidates() -> list[str]:
    override = os.environ.get("QUILIBRIUM_SDK_LIB")
    if override:
        return [override]
    names = {
        "darwin": ["libquilibrium.dylib"],
        "win32": ["quilibrium.dll"],
    }.get(sys.platform, ["libquilibrium.so"])
    found = ctypes.util.find_library("quilibrium")
    candidates = [found] if found else []
    candidates.extend(names)
    return [item for item in candidates if item]


def _load_library() -> ctypes.CDLL:
    errors: list[str] = []
    for candidate in _library_candidates():
        try:
            return ctypes.CDLL(candidate)
        except OSError as exc:
            errors.append(f"{candidate}: {exc}")
    raise OSError(
        "Unable to load Quilibrium SDK shared library. Set QUILIBRIUM_SDK_LIB. "
        + "; ".join(errors)
    )


_lib = _load_library()
_lib.ql_version.restype = ctypes.c_char_p
_lib.ql_sdk_create.argtypes = [ctypes.POINTER(_Config), ctypes.POINTER(_Error)]
_lib.ql_sdk_create.restype = ctypes.c_void_p
_lib.ql_sdk_destroy.argtypes = [ctypes.c_void_p]
_lib.ql_response_free.argtypes = [ctypes.POINTER(_Response)]
_lib.ql_error_free.argtypes = [ctypes.POINTER(_Error)]

for _name, _args in {
    "ql_hypersnap_user_by_fid": [ctypes.c_void_p, ctypes.c_uint64, ctypes.POINTER(_Response), ctypes.POINTER(_Error)],
    "ql_hypersnap_get": [ctypes.c_void_p, ctypes.c_char_p, ctypes.POINTER(_Response), ctypes.POINTER(_Error)],
    "ql_hypersnap_post": [ctypes.c_void_p, ctypes.c_char_p, ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t, ctypes.POINTER(_Response), ctypes.POINTER(_Error)],
    "ql_qkms_invoke": [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.POINTER(_Response), ctypes.POINTER(_Error)],
    "ql_qstorage_put": [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t, ctypes.c_char_p, ctypes.POINTER(_Response), ctypes.POINTER(_Error)],
    "ql_qstorage_get": [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.POINTER(_Response), ctypes.POINTER(_Error)],
    "ql_native_call": [ctypes.c_void_p, ctypes.c_int32, ctypes.c_char_p, ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t, ctypes.POINTER(_Response), ctypes.POINTER(_Error)],
}.items():
    fn = getattr(_lib, _name)
    fn.argtypes = _args
    fn.restype = ctypes.c_int


def version() -> str:
    raw = _lib.ql_version()
    return raw.decode("utf-8") if raw else "unknown"


def _cstr(value: Optional[str]) -> Optional[bytes]:
    return value.encode("utf-8") if value is not None else None


def _payload(value: bytes) -> tuple[Optional[ctypes.Array], ctypes.POINTER(ctypes.c_uint8)]:
    if not value:
        return None, ctypes.POINTER(ctypes.c_uint8)()
    array = (ctypes.c_uint8 * len(value)).from_buffer_copy(value)
    return array, ctypes.cast(array, ctypes.POINTER(ctypes.c_uint8))


class SDK:
    def __init__(
        self,
        *,
        hypersnap_endpoint: Optional[str] = None,
        qstorage_endpoint: Optional[str] = None,
        qkms_endpoint: Optional[str] = None,
        protocol_endpoint: Optional[str] = None,
        access_key_id: Optional[str] = None,
        secret_access_key: Optional[str] = None,
        session_token: Optional[str] = None,
    ) -> None:
        config = _Config(
            _cstr(hypersnap_endpoint),
            _cstr(qstorage_endpoint),
            _cstr(qkms_endpoint),
            _cstr(protocol_endpoint),
            _cstr(access_key_id),
            _cstr(secret_access_key),
            _cstr(session_token),
        )
        error = _Error()
        self._handle = _lib.ql_sdk_create(ctypes.byref(config), ctypes.byref(error))
        if not self._handle:
            self._raise(error)

    def close(self) -> None:
        if getattr(self, "_handle", None):
            _lib.ql_sdk_destroy(self._handle)
            self._handle = None

    def __enter__(self) -> "SDK":
        return self

    def __exit__(self, *_: object) -> None:
        self.close()

    def __del__(self) -> None:
        self.close()

    @staticmethod
    def _raise(error: _Error) -> None:
        try:
            message = error.message.decode("utf-8") if error.message else "Quilibrium SDK call failed"
            raise QuilibriumError(error.domain, error.code, message, error.http_status)
        finally:
            _lib.ql_error_free(ctypes.byref(error))

    @staticmethod
    def _finish(code: int, response: _Response, error: _Error) -> Response:
        if code != 0:
            SDK._raise(error)
        try:
            body = ctypes.string_at(response.body.data, response.body.size) if response.body.data else b""
            return Response(response.status_code, body)
        finally:
            _lib.ql_response_free(ctypes.byref(response))
            _lib.ql_error_free(ctypes.byref(error))

    def user_by_fid(self, fid: int) -> Response:
        response, error = _Response(), _Error()
        return self._finish(_lib.ql_hypersnap_user_by_fid(self._handle, fid, ctypes.byref(response), ctypes.byref(error)), response, error)

    def hypersnap_get(self, path: str) -> Response:
        response, error = _Response(), _Error()
        return self._finish(_lib.ql_hypersnap_get(self._handle, path.encode(), ctypes.byref(response), ctypes.byref(error)), response, error)

    def hypersnap_post(self, path: str, body: bytes) -> Response:
        backing, pointer = _payload(body)
        _ = backing
        response, error = _Response(), _Error()
        return self._finish(_lib.ql_hypersnap_post(self._handle, path.encode(), pointer, len(body), ctypes.byref(response), ctypes.byref(error)), response, error)

    def qkms_invoke(self, operation: str, payload: dict[str, Any] | str = "{}") -> Response:
        text = json.dumps(payload, separators=(",", ":")) if isinstance(payload, dict) else payload
        response, error = _Response(), _Error()
        return self._finish(_lib.ql_qkms_invoke(self._handle, operation.encode(), text.encode(), ctypes.byref(response), ctypes.byref(error)), response, error)

    def qstorage_put(self, bucket: str, key: str, body: bytes, content_type: str = "application/octet-stream") -> Response:
        backing, pointer = _payload(body)
        _ = backing
        response, error = _Response(), _Error()
        return self._finish(_lib.ql_qstorage_put(self._handle, bucket.encode(), key.encode(), pointer, len(body), content_type.encode(), ctypes.byref(response), ctypes.byref(error)), response, error)

    def qstorage_get(self, bucket: str, key: str) -> Response:
        response, error = _Response(), _Error()
        return self._finish(_lib.ql_qstorage_get(self._handle, bucket.encode(), key.encode(), ctypes.byref(response), ctypes.byref(error)), response, error)

    def native_call(self, service: int, method: str, protobuf_payload: bytes = b"") -> bytes:
        backing, pointer = _payload(protobuf_payload)
        _ = backing
        response, error = _Response(), _Error()
        result = self._finish(_lib.ql_native_call(self._handle, service, method.encode(), pointer, len(protobuf_payload), ctypes.byref(response), ctypes.byref(error)), response, error)
        return result.body


__all__ = ["SDK", "Response", "QuilibriumError", "version"]
