use std::ffi::{c_char, c_int, c_void, CStr, CString};
use std::ptr;

#[repr(C)]
struct QlConfig {
    hypersnap_endpoint: *const c_char,
    qstorage_endpoint: *const c_char,
    qkms_endpoint: *const c_char,
    protocol_endpoint: *const c_char,
    access_key_id: *const c_char,
    secret_access_key: *const c_char,
    session_token: *const c_char,
}

#[repr(C)]
struct QlBuffer { data: *mut u8, size: usize }
#[repr(C)]
struct QlResponse { status_code: i32, body: QlBuffer }
#[repr(C)]
struct QlError { domain: i32, code: i32, http_status: i32, message: *mut c_char }

enum QlSdkOpaque {}
type QlSdk = QlSdkOpaque;

unsafe extern "C" {
    fn ql_version() -> *const c_char;
    fn ql_sdk_create(config: *const QlConfig, error: *mut QlError) -> *mut QlSdk;
    fn ql_sdk_destroy(sdk: *mut QlSdk);
    fn ql_hypersnap_user_by_fid(sdk: *mut QlSdk, fid: u64, response: *mut QlResponse, error: *mut QlError) -> c_int;
    fn ql_hypersnap_get(sdk: *mut QlSdk, path: *const c_char, response: *mut QlResponse, error: *mut QlError) -> c_int;
    fn ql_qkms_invoke(sdk: *mut QlSdk, operation: *const c_char, json_payload: *const c_char, response: *mut QlResponse, error: *mut QlError) -> c_int;
    fn ql_qstorage_put(sdk: *mut QlSdk, bucket: *const c_char, key: *const c_char, data: *const u8, size: usize, content_type: *const c_char, response: *mut QlResponse, error: *mut QlError) -> c_int;
    fn ql_qstorage_get(sdk: *mut QlSdk, bucket: *const c_char, key: *const c_char, response: *mut QlResponse, error: *mut QlError) -> c_int;
    fn ql_native_call(sdk: *mut QlSdk, service: i32, method: *const c_char, data: *const u8, size: usize, response: *mut QlResponse, error: *mut QlError) -> c_int;
    fn ql_response_free(response: *mut QlResponse);
    fn ql_error_free(error: *mut QlError);
}

#[derive(Debug, Clone)]
pub struct Error { pub domain: i32, pub code: i32, pub http_status: i32, pub message: String }

#[derive(Debug, Clone)]
pub struct Response { pub status_code: i32, pub body: Vec<u8> }

#[repr(i32)]
#[derive(Debug, Clone, Copy)]
pub enum NativeService {
    Node=0, Connectivity=1, Global=2, AppShard=3, HypergraphComparison=4, KeyRegistry=5,
    Dispatch=6, Mixnet=7, Onion=8, PubSubProxy=9, DataIpc=10, FerretProxy=11,
}


#[derive(Default)]
pub struct Config {
    pub hypersnap_endpoint: Option<String>,
    pub qstorage_endpoint: Option<String>,
    pub qkms_endpoint: Option<String>,
    pub protocol_endpoint: Option<String>,
    pub access_key_id: Option<String>,
    pub secret_access_key: Option<String>,
    pub session_token: Option<String>,
}

pub struct Sdk { raw: *mut QlSdk }
unsafe impl Send for Sdk {}

fn cstring(value: &Option<String>) -> Option<CString> { value.as_ref().map(|v| CString::new(v.as_str()).expect("NUL byte in SDK configuration")) }

fn take_error(error: &mut QlError) -> Error {
    let message = if error.message.is_null() { "Quilibrium SDK call failed".into() } else { unsafe { CStr::from_ptr(error.message) }.to_string_lossy().into_owned() };
    let out=Error { domain:error.domain, code:error.code, http_status:error.http_status, message };
    unsafe { ql_error_free(error) };
    out
}

fn finish(code: c_int, response: &mut QlResponse, error: &mut QlError) -> Result<Response, Error> {
    if code != 0 { return Err(take_error(error)); }
    let body = if response.body.data.is_null() || response.body.size==0 { Vec::new() } else { unsafe { std::slice::from_raw_parts(response.body.data,response.body.size) }.to_vec() };
    let result=Response { status_code:response.status_code, body };
    unsafe { ql_response_free(response); ql_error_free(error); }
    Ok(result)
}

impl Sdk {
    pub fn version() -> String { unsafe { CStr::from_ptr(ql_version()) }.to_string_lossy().into_owned() }

    pub fn new(config: Config) -> Result<Self, Error> {
        let h=cstring(&config.hypersnap_endpoint); let s=cstring(&config.qstorage_endpoint); let k=cstring(&config.qkms_endpoint); let p=cstring(&config.protocol_endpoint);
        let a=cstring(&config.access_key_id); let secret=cstring(&config.secret_access_key); let token=cstring(&config.session_token);
        let raw_config=QlConfig {
            hypersnap_endpoint:h.as_ref().map_or(ptr::null(),|v|v.as_ptr()),
            qstorage_endpoint:s.as_ref().map_or(ptr::null(),|v|v.as_ptr()),
            qkms_endpoint:k.as_ref().map_or(ptr::null(),|v|v.as_ptr()),
            protocol_endpoint:p.as_ref().map_or(ptr::null(),|v|v.as_ptr()),
            access_key_id:a.as_ref().map_or(ptr::null(),|v|v.as_ptr()),
            secret_access_key:secret.as_ref().map_or(ptr::null(),|v|v.as_ptr()),
            session_token:token.as_ref().map_or(ptr::null(),|v|v.as_ptr()),
        };
        let mut error=QlError{domain:0,code:0,http_status:0,message:ptr::null_mut()};
        let raw=unsafe { ql_sdk_create(&raw_config,&mut error) };
        if raw.is_null() { Err(take_error(&mut error)) } else { Ok(Self{raw}) }
    }

    pub fn user_by_fid(&self,fid:u64)->Result<Response,Error>{
        self.call_response(|response,error| unsafe { ql_hypersnap_user_by_fid(self.raw,fid,response,error) })
    }

    pub fn hypersnap_get(&self,path:&str)->Result<Response,Error>{
        let path=CString::new(path).expect("NUL byte in path");
        self.call_response(|response,error| unsafe { ql_hypersnap_get(self.raw,path.as_ptr(),response,error) })
    }

    pub fn qkms_invoke(&self,operation:&str,json_payload:&str)->Result<Response,Error>{
        let operation=CString::new(operation).expect("NUL byte in operation");
        let payload=CString::new(json_payload).expect("NUL byte in payload");
        self.call_response(|response,error| unsafe { ql_qkms_invoke(self.raw,operation.as_ptr(),payload.as_ptr(),response,error) })
    }

    pub fn qstorage_put(&self,bucket:&str,key:&str,data:&[u8],content_type:&str)->Result<Response,Error>{
        let bucket=CString::new(bucket).unwrap(); let key=CString::new(key).unwrap(); let content_type=CString::new(content_type).unwrap();
        self.call_response(|response,error| unsafe { ql_qstorage_put(self.raw,bucket.as_ptr(),key.as_ptr(),data.as_ptr(),data.len(),content_type.as_ptr(),response,error) })
    }

    pub fn qstorage_get(&self,bucket:&str,key:&str)->Result<Response,Error>{
        let bucket=CString::new(bucket).unwrap(); let key=CString::new(key).unwrap();
        self.call_response(|response,error| unsafe { ql_qstorage_get(self.raw,bucket.as_ptr(),key.as_ptr(),response,error) })
    }

    pub fn native_call(&self,service:NativeService,method:&str,payload:&[u8])->Result<Vec<u8>,Error>{
        let method=CString::new(method).unwrap();
        self.call_response(|response,error| unsafe { ql_native_call(self.raw,service as i32,method.as_ptr(),payload.as_ptr(),payload.len(),response,error) }).map(|r|r.body)
    }

    fn call_response<F>(&self,call:F)->Result<Response,Error> where F:FnOnce(*mut QlResponse,*mut QlError)->c_int {
        let mut response=QlResponse{status_code:0,body:QlBuffer{data:ptr::null_mut(),size:0}};
        let mut error=QlError{domain:0,code:0,http_status:0,message:ptr::null_mut()};
        let code=call(&mut response,&mut error);
        finish(code,&mut response,&mut error)
    }
}

impl Drop for Sdk { fn drop(&mut self){ if !self.raw.is_null(){ unsafe{ql_sdk_destroy(self.raw)}; self.raw=ptr::null_mut(); } } }
