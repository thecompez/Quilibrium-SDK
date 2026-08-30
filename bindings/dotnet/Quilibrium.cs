using System;
using System.Runtime.InteropServices;
using System.Text;

namespace Quilibrium;

public enum NativeService
{
    Node = 0, Connectivity = 1, Global = 2, AppShard = 3,
    HypergraphComparison = 4, KeyRegistry = 5, Dispatch = 6,
    Mixnet = 7, Onion = 8, PubSubProxy = 9, DataIPC = 10, FerretProxy = 11
}

public sealed class QuilibriumException : Exception
{
    public int Domain { get; }
    public int Code { get; }
    public int HttpStatus { get; }
    internal QuilibriumException(int domain, int code, int httpStatus, string message) : base(message)
        => (Domain, Code, HttpStatus) = (domain, code, httpStatus);
}

public readonly record struct Response(int StatusCode, byte[] Body)
{
    public string Utf8Text => Encoding.UTF8.GetString(Body);
}

public sealed class Sdk : IDisposable
{
    [StructLayout(LayoutKind.Sequential)] private struct Buffer { public IntPtr data; public UIntPtr size; }
    [StructLayout(LayoutKind.Sequential)] private struct NativeResponse { public int status_code; public Buffer body; }
    [StructLayout(LayoutKind.Sequential)] private struct NativeError { public int domain; public int code; public int http_status; public IntPtr message; }
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)] private struct Config
    {
        public string? hypersnap_endpoint, qstorage_endpoint, qkms_endpoint, protocol_endpoint,
                       access_key_id, secret_access_key, session_token;
    }

    private const string Library = "quilibrium";
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] private static extern IntPtr ql_sdk_create(ref Config config, out NativeError error);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] private static extern void ql_sdk_destroy(IntPtr sdk);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] private static extern int ql_hypersnap_user_by_fid(IntPtr sdk, ulong fid, out NativeResponse response, out NativeError error);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)] private static extern int ql_hypersnap_get(IntPtr sdk, string path, out NativeResponse response, out NativeError error);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)] private static extern int ql_hypersnap_post(IntPtr sdk, string path, byte[] data, UIntPtr size, out NativeResponse response, out NativeError error);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)] private static extern int ql_qkms_invoke(IntPtr sdk, string operation, string jsonPayload, out NativeResponse response, out NativeError error);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)] private static extern int ql_qstorage_put(IntPtr sdk, string bucket, string key, byte[] data, UIntPtr size, string contentType, out NativeResponse response, out NativeError error);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)] private static extern int ql_qstorage_get(IntPtr sdk, string bucket, string key, out NativeResponse response, out NativeError error);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)] private static extern int ql_native_call(IntPtr sdk, int service, string method, byte[] data, UIntPtr size, out NativeResponse response, out NativeError error);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] private static extern void ql_response_free(ref NativeResponse response);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl)] private static extern void ql_error_free(ref NativeError error);

    private IntPtr _handle;

    public Sdk(string? hypersnapEndpoint = null, string? qstorageEndpoint = null, string? qkmsEndpoint = null,
               string? protocolEndpoint = null, string? accessKeyId = null, string? secretAccessKey = null,
               string? sessionToken = null)
    {
        var config = new Config {
            hypersnap_endpoint = hypersnapEndpoint,
            qstorage_endpoint = qstorageEndpoint,
            qkms_endpoint = qkmsEndpoint,
            protocol_endpoint = protocolEndpoint,
            access_key_id = accessKeyId,
            secret_access_key = secretAccessKey,
            session_token = sessionToken
        };
        _handle = ql_sdk_create(ref config, out var error);
        if (_handle == IntPtr.Zero) Throw(ref error);
    }

    public Response UserByFid(ulong fid)
        => Finish(ql_hypersnap_user_by_fid(_handle, fid, out var response, out var error), ref response, ref error);

    public Response HypersnapGet(string path)
        => Finish(ql_hypersnap_get(_handle, path, out var response, out var error), ref response, ref error);

    public Response HypersnapPost(string path, byte[] data)
        => Finish(ql_hypersnap_post(_handle, path, data, (UIntPtr)data.Length, out var response, out var error), ref response, ref error);

    public Response QkmsInvoke(string operation, string jsonPayload = "{}")
        => Finish(ql_qkms_invoke(_handle, operation, jsonPayload, out var response, out var error), ref response, ref error);

    public Response QstoragePut(string bucket, string key, byte[] data, string contentType = "application/octet-stream")
        => Finish(ql_qstorage_put(_handle, bucket, key, data, (UIntPtr)data.Length, contentType, out var response, out var error), ref response, ref error);

    public Response QstorageGet(string bucket, string key)
        => Finish(ql_qstorage_get(_handle, bucket, key, out var response, out var error), ref response, ref error);

    public byte[] NativeCall(NativeService service, string method, byte[]? protobufPayload = null)
    {
        var data = protobufPayload ?? Array.Empty<byte>();
        return Finish(ql_native_call(_handle, (int)service, method, data, (UIntPtr)data.Length, out var response, out var error), ref response, ref error).Body;
    }

    private static Response Finish(int code, ref NativeResponse response, ref NativeError error)
    {
        if (code != 0) Throw(ref error);
        try {
            int length = checked((int)response.body.size.ToUInt64());
            var bytes = new byte[length];
            if (length > 0) Marshal.Copy(response.body.data, bytes, 0, length);
            return new Response(response.status_code, bytes);
        } finally {
            ql_response_free(ref response);
            ql_error_free(ref error);
        }
    }

    private static void Throw(ref NativeError error)
    {
        try {
            throw new QuilibriumException(error.domain, error.code, error.http_status,
                Marshal.PtrToStringUTF8(error.message) ?? "Quilibrium SDK call failed");
        } finally {
            ql_error_free(ref error);
        }
    }

    public void Dispose()
    {
        if (_handle != IntPtr.Zero) {
            ql_sdk_destroy(_handle);
            _handle = IntPtr.Zero;
            GC.SuppressFinalize(this);
        }
    }

    ~Sdk() => Dispose();
}
