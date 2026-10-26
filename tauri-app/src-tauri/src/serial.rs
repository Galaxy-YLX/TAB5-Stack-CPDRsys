/// Windows 串口通信 — 纯 Win32 FFI (零外部依赖)
use std::ffi::CString;
use std::ptr::null_mut;
pub type PortHandle = *mut std::ffi::c_void;

#[link(name = "kernel32")]
extern "system" {
    fn CreateFileA(fn_: *const u8, dwD: u32, dwS: u32, lpS: *mut std::ffi::c_void,
                   dwC: u32, dwF: u32, hT: *mut std::ffi::c_void) -> *mut std::ffi::c_void;
    fn CloseHandle(h: *mut std::ffi::c_void) -> i32;
    fn WriteFile(h: *mut std::ffi::c_void, buf: *const u8, n: u32, wr: *mut u32,
                 ov: *mut std::ffi::c_void) -> i32;
    fn ReadFile(h: *mut std::ffi::c_void, buf: *mut u8, n: u32, rd: *mut u32,
                ov: *mut std::ffi::c_void) -> i32;
    fn GetCommState(h: *mut std::ffi::c_void, dcb: *mut u8) -> i32;
    fn SetCommState(h: *mut std::ffi::c_void, dcb: *const u8) -> i32;
    fn SetCommTimeouts(h: *mut std::ffi::c_void, to: *const u8) -> i32;
    fn SetupComm(h: *mut std::ffi::c_void, dwIn: u32, dwOut: u32) -> i32;
    fn PurgeComm(h: *mut std::ffi::c_void, dwF: u32) -> i32;
    fn ClearCommError(h: *mut std::ffi::c_void, err: *mut u32, stat: *mut u8) -> i32;
    fn GetLastError() -> u32;
}

const GENERIC_RW: u32 = 0xC0000000 | 0x40000000;
const OPEN_EXISTING: u32 = 3;
const FILE_ATTRIBUTE_NORMAL: u32 = 128;
const PURGE_RXCLEAR: u32 = 8;
const PURGE_TXCLEAR: u32 = 4;
const INVALID_HANDLE: isize = -1;

pub fn open(port: &str) -> Result<PortHandle, String> {
    let name = format!("\\\\.\\{}", port);
    let cname = CString::new(name).map_err(|e| e.to_string())?;
    let h = unsafe {
        CreateFileA(cname.as_ptr() as *const u8, GENERIC_RW, 0, null_mut(),
                     OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, null_mut())
    };
    if h as isize == INVALID_HANDLE || h.is_null() {
        return Err(format!("can't open {}", port));
    }

    // DCB: use byte buffer, fill with GetCommState, modify only BaudRate (offset 4)
    let mut dcb = [0u8; 40];
    unsafe {
        // Set DCBlength field at offset 0
        dcb[0] = 0x1C; // sizeof(DCB) on x64 = 28
        GetCommState(h, dcb.as_mut_ptr());
        // BaudRate at offset 4 (u32 LE)
        let baud: u32 = 115200;
        dcb[4] = (baud & 0xFF) as u8;
        dcb[5] = ((baud >> 8) & 0xFF) as u8;
        dcb[6] = ((baud >> 16) & 0xFF) as u8;
        dcb[7] = ((baud >> 24) & 0xFF) as u8;
        // fBinary=1 at offset 8, bit 0
        dcb[8] |= 0x01;
        // ByteSize=8 at offset 17
        dcb[17] = 8;
        // Parity=0 at offset 18
        dcb[18] = 0;
        // StopBits=0 (ONESTOPBIT) at offset 19
        dcb[19] = 0;
        SetCommState(h, dcb.as_ptr());

        // COMMTIMEOUTS: RI=5ms, RTM=1ms, RTC=20ms, WTM=0, WTC=0
        let to: [u32; 5] = [5, 1, 20, 0, 0];
        SetCommTimeouts(h, to.as_ptr() as *const u8);

        SetupComm(h, 512, 512);
        PurgeComm(h, PURGE_RXCLEAR | PURGE_TXCLEAR);
    }
    println!("[SERIAL] {} open 115200bps", port);
    Ok(h)
}

pub fn close(h: PortHandle) {
    if !h.is_null() {
        unsafe { PurgeComm(h, PURGE_RXCLEAR | PURGE_TXCLEAR); CloseHandle(h); }
    }
}

pub fn send(h: PortHandle, data: &[u8]) -> i32 {
    if h.is_null() || data.is_empty() { return -1; }
    let mut n: u32 = 0;
    let r = unsafe { WriteFile(h, data.as_ptr(), data.len() as u32, &mut n, null_mut()) };
    if r == 0 { -1 } else { n as i32 }
}

pub fn recv(h: PortHandle, buf: &mut [u8], timeout_ms: u32) -> i32 {
    if h.is_null() { return -1; }
    let to: [u32; 5] = [5, 1, timeout_ms, 0, 0];
    unsafe { SetCommTimeouts(h, to.as_ptr() as *const u8); }
    let mut n: u32 = 0;
    let r = unsafe { ReadFile(h, buf.as_mut_ptr(), buf.len() as u32, &mut n, null_mut()) };
    if r == 0 { -1 } else { n as i32 }
}

pub fn is_open(h: PortHandle) -> bool {
    if h.is_null() { return false; }
    let mut e: u32 = 0;
    let mut s = [0u8; 32];
    unsafe { ClearCommError(h, &mut e, s.as_mut_ptr()) != 0 }
}

pub fn list_ports() -> Vec<String> {
    let mut v = Vec::new();
    for i in 1..=16 {
        let name = format!("COM{}", i);
        if let Ok(h) = open(&name) { v.push(name); close(h); }
    }
    v
}
