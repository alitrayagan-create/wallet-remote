#!/usr/bin/env python3
"""Wallet Remote Bluetooth clipboard helper.

The board is a BLE keyboard; it cannot read the Windows clipboard by itself.
This process stays paired over HID (vendor report ID 4) and sends clipboard
text when the screen asks. USB data is not used — power the CYD however you like.
"""
from __future__ import annotations

import ctypes
import sys
import time
from ctypes import wintypes

VID = 0x303A
PID = 0x4011
REPORT_ID = 4
USAGE_PAGE = 0xFF00
GETCLIP = 0xF1
FLAG_FIRST = 0x01
FLAG_LAST = 0x02
MAX_CHARS = 240

GENERIC_READ = 0x80000000
GENERIC_WRITE = 0x40000000
FILE_SHARE_READ = 1
FILE_SHARE_WRITE = 2
OPEN_EXISTING = 3
FILE_FLAG_OVERLAPPED = 0x40000000
DIGCF_PRESENT = 2
DIGCF_DEVICEINTERFACE = 0x10
INVALID_HANDLE_VALUE = ctypes.c_void_p(-1).value
ERROR_NO_MORE_ITEMS = 259
ERROR_INSUFFICIENT_BUFFER = 122
HIDP_STATUS_SUCCESS = 0x00110000

kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
setupapi = ctypes.WinDLL("setupapi", use_last_error=True)
hid = ctypes.WinDLL("hid", use_last_error=True)
user32 = ctypes.WinDLL("user32", use_last_error=True)


class GUID(ctypes.Structure):
    _fields_ = [
        ("Data1", wintypes.DWORD),
        ("Data2", wintypes.WORD),
        ("Data3", wintypes.WORD),
        ("Data4", ctypes.c_ubyte * 8),
    ]


class SP_DEVICE_INTERFACE_DATA(ctypes.Structure):
    _fields_ = [
        ("cbSize", wintypes.DWORD),
        ("InterfaceClassGuid", GUID),
        ("Flags", wintypes.DWORD),
        ("Reserved", ctypes.POINTER(ctypes.c_ulong)),
    ]


class HIDD_ATTRIBUTES(ctypes.Structure):
    _fields_ = [
        ("Size", wintypes.ULONG),
        ("VendorID", wintypes.USHORT),
        ("ProductID", wintypes.USHORT),
        ("VersionNumber", wintypes.USHORT),
    ]


class HIDP_CAPS(ctypes.Structure):
    _fields_ = [
        ("Usage", wintypes.USHORT),
        ("UsagePage", wintypes.USHORT),
        ("InputReportByteLength", wintypes.USHORT),
        ("OutputReportByteLength", wintypes.USHORT),
        ("FeatureReportByteLength", wintypes.USHORT),
        ("Reserved", wintypes.USHORT * 17),
        ("NumberLinkCollectionNodes", wintypes.USHORT),
        ("NumberInputButtonCaps", wintypes.USHORT),
        ("NumberInputValueCaps", wintypes.USHORT),
        ("NumberInputDataIndices", wintypes.USHORT),
        ("NumberOutputButtonCaps", wintypes.USHORT),
        ("NumberOutputValueCaps", wintypes.USHORT),
        ("NumberOutputDataIndices", wintypes.USHORT),
        ("NumberFeatureButtonCaps", wintypes.USHORT),
        ("NumberFeatureValueCaps", wintypes.USHORT),
        ("NumberFeatureDataIndices", wintypes.USHORT),
    ]


HidD_GetHidGuid = hid.HidD_GetHidGuid
HidD_GetHidGuid.argtypes = [ctypes.POINTER(GUID)]

HidD_GetAttributes = hid.HidD_GetAttributes
HidD_GetAttributes.argtypes = [wintypes.HANDLE, ctypes.POINTER(HIDD_ATTRIBUTES)]
HidD_GetAttributes.restype = wintypes.BOOL

HidD_GetPreparsedData = hid.HidD_GetPreparsedData
HidD_GetPreparsedData.argtypes = [wintypes.HANDLE, ctypes.POINTER(ctypes.c_void_p)]
HidD_GetPreparsedData.restype = wintypes.BOOL

HidD_FreePreparsedData = hid.HidD_FreePreparsedData
HidD_FreePreparsedData.argtypes = [ctypes.c_void_p]
HidD_FreePreparsedData.restype = wintypes.BOOL

HidP_GetCaps = hid.HidP_GetCaps
HidP_GetCaps.argtypes = [ctypes.c_void_p, ctypes.POINTER(HIDP_CAPS)]
HidP_GetCaps.restype = ctypes.c_ulong

HidD_SetFeature = hid.HidD_SetFeature
HidD_SetFeature.argtypes = [wintypes.HANDLE, ctypes.c_void_p, wintypes.ULONG]
HidD_SetFeature.restype = wintypes.BOOL

HidD_GetFeature = hid.HidD_GetFeature
HidD_GetFeature.argtypes = [wintypes.HANDLE, ctypes.c_void_p, wintypes.ULONG]
HidD_GetFeature.restype = wintypes.BOOL

HidD_SetOutputReport = hid.HidD_SetOutputReport
HidD_SetOutputReport.argtypes = [wintypes.HANDLE, ctypes.c_void_p, wintypes.ULONG]
HidD_SetOutputReport.restype = wintypes.BOOL

SetupDiGetClassDevs = setupapi.SetupDiGetClassDevsW
SetupDiGetClassDevs.argtypes = [ctypes.POINTER(GUID), wintypes.LPCWSTR, wintypes.HWND, wintypes.DWORD]
SetupDiGetClassDevs.restype = wintypes.HANDLE

SetupDiEnumDeviceInterfaces = setupapi.SetupDiEnumDeviceInterfaces
SetupDiEnumDeviceInterfaces.argtypes = [
    wintypes.HANDLE,
    ctypes.c_void_p,
    ctypes.POINTER(GUID),
    wintypes.DWORD,
    ctypes.POINTER(SP_DEVICE_INTERFACE_DATA),
]
SetupDiEnumDeviceInterfaces.restype = wintypes.BOOL

SetupDiGetDeviceInterfaceDetail = setupapi.SetupDiGetDeviceInterfaceDetailW
SetupDiGetDeviceInterfaceDetail.argtypes = [
    wintypes.HANDLE,
    ctypes.POINTER(SP_DEVICE_INTERFACE_DATA),
    ctypes.c_void_p,
    wintypes.DWORD,
    ctypes.POINTER(wintypes.DWORD),
    ctypes.c_void_p,
]
SetupDiGetDeviceInterfaceDetail.restype = wintypes.BOOL

SetupDiDestroyDeviceInfoList = setupapi.SetupDiDestroyDeviceInfoList
SetupDiDestroyDeviceInfoList.argtypes = [wintypes.HANDLE]
SetupDiDestroyDeviceInfoList.restype = wintypes.BOOL

CreateFile = kernel32.CreateFileW
CreateFile.argtypes = [
    wintypes.LPCWSTR,
    wintypes.DWORD,
    wintypes.DWORD,
    ctypes.c_void_p,
    wintypes.DWORD,
    wintypes.DWORD,
    wintypes.HANDLE,
]
CreateFile.restype = wintypes.HANDLE

CloseHandle = kernel32.CloseHandle
CloseHandle.argtypes = [wintypes.HANDLE]
CloseHandle.restype = wintypes.BOOL

user32.OpenClipboard.argtypes = [wintypes.HWND]
user32.OpenClipboard.restype = wintypes.BOOL
user32.CloseClipboard.argtypes = []
user32.CloseClipboard.restype = wintypes.BOOL
user32.IsClipboardFormatAvailable.argtypes = [wintypes.UINT]
user32.IsClipboardFormatAvailable.restype = wintypes.BOOL
user32.GetClipboardData.argtypes = [wintypes.UINT]
user32.GetClipboardData.restype = ctypes.c_void_p
kernel32.GlobalLock.argtypes = [ctypes.c_void_p]
kernel32.GlobalLock.restype = ctypes.c_void_p
kernel32.GlobalUnlock.argtypes = [ctypes.c_void_p]
kernel32.GlobalUnlock.restype = wintypes.BOOL
kernel32.GlobalSize.argtypes = [ctypes.c_void_p]
kernel32.GlobalSize.restype = ctypes.c_size_t
ole32 = ctypes.WinDLL("ole32", use_last_error=True)


def _hid_paths():
    guid = GUID()
    HidD_GetHidGuid(ctypes.byref(guid))
    devs = SetupDiGetClassDevs(ctypes.byref(guid), None, None, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE)
    if devs == INVALID_HANDLE_VALUE:
        return
    try:
        index = 0
        while True:
            iface = SP_DEVICE_INTERFACE_DATA()
            iface.cbSize = ctypes.sizeof(SP_DEVICE_INTERFACE_DATA)
            if not SetupDiEnumDeviceInterfaces(devs, None, ctypes.byref(guid), index, ctypes.byref(iface)):
                err = ctypes.get_last_error()
                if err in (ERROR_NO_MORE_ITEMS, 0):
                    break
                index += 1
                continue
            needed = wintypes.DWORD(0)
            SetupDiGetDeviceInterfaceDetail(devs, ctypes.byref(iface), None, 0, ctypes.byref(needed), None)
            buf = ctypes.create_string_buffer(needed.value)
            # cbSize: 8 on 64-bit Windows, 5 on 32-bit (SP_DEVICE_INTERFACE_DETAIL_DATA)
            ctypes.cast(buf, ctypes.POINTER(wintypes.DWORD))[0] = 8 if ctypes.sizeof(ctypes.c_void_p) == 8 else 6
            if SetupDiGetDeviceInterfaceDetail(
                devs, ctypes.byref(iface), buf, needed.value, None, None
            ):
                path = ctypes.wstring_at(ctypes.addressof(buf) + ctypes.sizeof(wintypes.DWORD))
                yield path
            index += 1
    finally:
        SetupDiDestroyDeviceInfoList(devs)


def _open_path(path):
    # Feature reports fail more often on overlapped handles.
    handle = CreateFile(
        path,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        None,
        OPEN_EXISTING,
        0,
        None,
    )
    return handle


def _caps(handle):
    prep = ctypes.c_void_p()
    if not HidD_GetPreparsedData(handle, ctypes.byref(prep)):
        return None
    try:
        caps = HIDP_CAPS()
        if HidP_GetCaps(prep, ctypes.byref(caps)) != HIDP_STATUS_SUCCESS:
            return None
        return caps
    finally:
        HidD_FreePreparsedData(prep)


def find_wallet_remote():
    matches = []
    for path in _hid_paths():
        handle = _open_path(path)
        if handle == INVALID_HANDLE_VALUE:
            continue
        try:
            attrs = HIDD_ATTRIBUTES()
            attrs.Size = ctypes.sizeof(HIDD_ATTRIBUTES)
            if not HidD_GetAttributes(handle, ctypes.byref(attrs)):
                continue
            if attrs.VendorID != VID or attrs.ProductID != PID:
                continue
            caps = _caps(handle)
            up = caps.UsagePage if caps else 0
            feat = caps.FeatureReportByteLength if caps else 0
            matches.append((handle, path, up, feat, caps))
        except Exception:
            CloseHandle(handle)
    if not matches:
        return None
    matches.sort(key=lambda m: (0 if m[2] == USAGE_PAGE else 1, 0 if m[3] >= 33 else 1))
    best = matches[0]
    for handle, path, up, feat, caps in matches[1:]:
        CloseHandle(handle)
        del path, up, feat, caps
    return best[0], best[1], best[4]


def _read_open_clipboard():
    CF_UNICODETEXT = 13
    CF_TEXT = 1
    text = ""
    if user32.IsClipboardFormatAvailable(CF_UNICODETEXT):
        h = user32.GetClipboardData(CF_UNICODETEXT)
        if h:
            p = kernel32.GlobalLock(h)
            if p:
                try:
                    text = ctypes.wstring_at(p)
                finally:
                    kernel32.GlobalUnlock(h)
    elif user32.IsClipboardFormatAvailable(CF_TEXT):
        h = user32.GetClipboardData(CF_TEXT)
        if h:
            p = kernel32.GlobalLock(h)
            if p:
                try:
                    n = kernel32.GlobalSize(h)
                    raw = ctypes.string_at(p, n)
                    text = raw.split(b"\0", 1)[0].decode("utf-8", "replace")
                finally:
                    kernel32.GlobalUnlock(h)
    return " ".join((text or "").split())


def _clipboard_win32(wait_s=1.0):
    deadline = time.time() + wait_s
    while True:
        opened = False
        for _ in range(20):
            if user32.OpenClipboard(None):
                opened = True
                break
            time.sleep(0.025)
        if opened:
            try:
                text = _read_open_clipboard()
            finally:
                user32.CloseClipboard()
            if text:
                return text[:MAX_CHARS]
        if time.time() >= deadline:
            return ""
        time.sleep(0.05)


_tk = None


def _clipboard_tk():
    global _tk
    try:
        import tkinter as tk

        if _tk is None:
            _tk = tk.Tk()
            _tk.withdraw()
            _tk.update_idletasks()
        _tk.update()
        text = _tk.clipboard_get()
        return " ".join((text or "").split())[:MAX_CHARS]
    except Exception:
        return ""


def _clipboard_powershell():
    import subprocess

    try:
        r = subprocess.run(
            [
                "powershell",
                "-STA",
                "-NoProfile",
                "-Command",
                "Add-Type -AssemblyName System.Windows.Forms; "
                "[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false); "
                "[System.Windows.Forms.Clipboard]::GetText()",
            ],
            capture_output=True,
            timeout=6,
        )
        out = r.stdout.decode("utf-8", "replace") if r.stdout else ""
        return " ".join(out.split())[:MAX_CHARS]
    except Exception:
        return ""


def clipboard_text(wait_s=1.2):
    text = _clipboard_win32(wait_s)
    if text:
        return text
    text = _clipboard_tk()
    if text:
        return text
    return _clipboard_powershell()


def send_packets(handle, feat_len, payload: bytes):
    body = 32
    pkt_len = max(feat_len, 1 + body)
    offset = 0
    first = True
    if not payload:
        buf = (ctypes.c_ubyte * pkt_len)()
        buf[0] = REPORT_ID
        buf[1] = FLAG_FIRST | FLAG_LAST
        buf[2] = 0
        if not HidD_SetFeature(handle, buf, pkt_len):
            HidD_SetOutputReport(handle, buf, pkt_len)
        return
    while True:
        chunk = payload[offset : offset + 30]
        last = offset + len(chunk) >= len(payload)
        flags = 0
        if first:
            flags |= FLAG_FIRST
        if last:
            flags |= FLAG_LAST
        buf = (ctypes.c_ubyte * pkt_len)()
        buf[0] = REPORT_ID
        buf[1] = flags
        buf[2] = len(chunk)
        for i, b in enumerate(chunk):
            buf[3 + i] = b
        ok = HidD_SetFeature(handle, buf, pkt_len)
        if not ok:
            ok = HidD_SetOutputReport(handle, buf, pkt_len)
        if not ok:
            raise OSError("HID SetFeature/SetOutput failed (is Wallet Remote still paired?)")
        if last:
            return
        first = False
        offset += len(chunk)
        time.sleep(0.02)


def poll_getclip(handle, feat_len):
    pkt_len = max(feat_len or 0, 33)
    buf = (ctypes.c_ubyte * pkt_len)()
    buf[0] = REPORT_ID
    if not HidD_GetFeature(handle, buf, pkt_len):
        return None
    if buf[1] != GETCLIP:
        return None
    return buf[2]  # sequence number (0 on older firmware)


def main():
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("--quit-after", type=int, default=0)
    args = parser.parse_args()
    quit_after = max(0, args.quit_after)

    try:
        ole32.OleInitialize(None)
    except Exception:
        pass
    try:
        kernel32.SetConsoleTitleW.argtypes = [wintypes.LPCWSTR]
        kernel32.SetConsoleTitleW("WalletRemoteHelper")
    except Exception:
        pass

    print("Wallet Remote helper")
    if quit_after:
        print("Auto mode: quit after %d clipboard send(s)" % quit_after)
    else:
        print("Click the website field so it stays focused.")
        print("Do not click this window while capturing.")
        print("Ctrl+C to quit.")
    print("")

    now = clipboard_text(wait_s=0.2)
    print("Clipboard check: %d chars" % len(now))

    handle = None
    feat_len = 33
    deadline = time.time() + (8 if quit_after else 1e9)
    while handle is None:
        found = find_wallet_remote()
        if found:
            handle, path, caps = found
            feat_len = caps.FeatureReportByteLength if caps else 33
            print("Connected: %s" % path)
            break
        if time.time() >= deadline:
            sys.exit("Wallet Remote HID not found")
        print("Waiting for Wallet Remote...")
        time.sleep(0.4)

    last_seq = None
    sent = 0
    t0 = time.time()
    try:
        while True:
            seq = None
            try:
                seq = poll_getclip(handle, feat_len)
            except OSError:
                seq = None
            if seq is not None and seq != last_seq:
                last_seq = seq
                text = clipboard_text(wait_s=1.2)
                data = text.encode("utf-8", errors="replace")[:MAX_CHARS]
                send_packets(handle, feat_len, data)
                sent += 1
                if data:
                    print("sent %d chars" % len(data))
                else:
                    print("sent 0 chars - click the website box, then try again")
                if quit_after and sent >= quit_after:
                    break
            if quit_after and time.time() - t0 > 25:
                break
            time.sleep(0.08)
    except KeyboardInterrupt:
        if not quit_after:
            print("\nbye")
    finally:
        if handle and handle != INVALID_HANDLE_VALUE:
            CloseHandle(handle)


if __name__ == "__main__":
    if sys.platform != "win32":
        sys.exit("This helper is for Windows.")
    main()
