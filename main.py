# main.py - Updated with bridge
# Roblox Executor - Complete GUI

import ctypes
import ctypes.wintypes as wintypes
import os
import sys
import time
import threading
import tkinter as tk
from tkinter import scrolledtext, messagebox, filedialog

from bridge import ExecutorBridge

# Native bindings
kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)

PROCESS_ALL_ACCESS = 0x1F0FFF
TH32CS_SNAPPROCESS = 0x00000002

class PROCESSENTRY32(ctypes.Structure):
    _fields_ = [
        ("dwSize", wintypes.DWORD),
        ("cntUsage", wintypes.DWORD),
        ("th32ProcessID", wintypes.DWORD),
        ("th32DefaultHeapID", ctypes.POINTER(ctypes.c_ulong)),
        ("th32ModuleID", wintypes.DWORD),
        ("cntThreads", wintypes.DWORD),
        ("th32ParentProcessID", wintypes.DWORD),
        ("pcPriClassBase", ctypes.c_long),
        ("dwFlags", wintypes.DWORD),
        ("szExeFile", ctypes.c_char * 260),
    ]

def find_roblox_process():
    snapshot = kernel32.CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)
    if snapshot == -1:
        return None
    entry = PROCESSENTRY32()
    entry.dwSize = ctypes.sizeof(PROCESSENTRY32)
    pid = None
    if kernel32.Process32First(snapshot, ctypes.byref(entry)):
        while True:
            name = entry.szExeFile.decode('utf-8', errors='ignore')
            if 'RobloxPlayerBeta' in name:
                pid = entry.th32ProcessID
                break
            if not kernel32.Process32Next(snapshot, ctypes.byref(entry)):
                break
    kernel32.CloseHandle(snapshot)
    return pid

class Memory:
    def __init__(self, pid):
        self.pid = pid
        self.handle = kernel32.OpenProcess(PROCESS_ALL_ACCESS, False, pid)
        if not self.handle:
            raise RuntimeError("OpenProcess failed")
    def close(self):
        if self.handle:
            kernel32.CloseHandle(self.handle)

class Injector:
    def __init__(self, memory):
        self.mem = memory
    def inject_dll(self, dll_path):
        dll_path = os.path.abspath(dll_path)
        if not os.path.exists(dll_path):
            raise FileNotFoundError(dll_path)
        dll_bytes = dll_path.encode('utf-8') + b'\x00'
        addr = kernel32.VirtualAllocEx(self.mem.handle, None, len(dll_bytes), 0x3000, 0x40)
        if not addr:
            return False
        written = ctypes.c_size_t(0)
        kernel32.WriteProcessMemory(self.mem.handle, ctypes.c_void_p(addr), dll_bytes, len(dll_bytes), ctypes.byref(written))
        load_lib = kernel32.GetProcAddress(kernel32.GetModuleHandleA(b'kernel32.dll'), b'LoadLibraryA')
        thread_id = wintypes.DWORD(0)
        thread = kernel32.CreateRemoteThread(self.mem.handle, None, 0, ctypes.c_void_p(load_lib), ctypes.c_void_p(addr), 0, ctypes.byref(thread_id))
        if not thread:
            kernel32.VirtualFreeEx(self.mem.handle, ctypes.c_void_p(addr), 0, 0x8000)
            return False
        kernel32.WaitForSingleObject(thread, 10000)
        kernel32.CloseHandle(thread)
        kernel32.VirtualFreeEx(self.mem.handle, ctypes.c_void_p(addr), 0, 0x8000)
        return True

class ExecutorGUI:
    def __init__(self):
        self.root = tk.Tk()
        self.root.title("Roblox Executor")
        self.root.geometry("800x600")
        self.root.configure(bg="#1e1e1e")
        self.pid = None
        self.memory = None
        self.injector = None
        self.bridge = ExecutorBridge()
        self._build_ui()
        self._start_output_poll()
    
    def _build_ui(self):
        self.status = tk.Label(self.root, text="Not attached", bg="#2d2d2d", fg="white", anchor="w", padx=10)
        self.status.pack(fill=tk.X)
        btn_frame = tk.Frame(self.root, bg="#1e1e1e")
        btn_frame.pack(fill=tk.X, padx=10, pady=5)
        tk.Button(btn_frame, text="Attach", command=self.attach, bg="#0078d4", fg="white").pack(side=tk.LEFT, padx=5)
        tk.Button(btn_frame, text="Inject", command=self.inject, bg="#4CAF50", fg="white").pack(side=tk.LEFT, padx=5)
        tk.Button(btn_frame, text="Execute", command=self.execute, bg="#FF9800", fg="white").pack(side=tk.LEFT, padx=5)
        tk.Button(btn_frame, text="Open Script", command=self.open_script, bg="#9C27B0", fg="white").pack(side=tk.LEFT, padx=5)
        tk.Button(btn_frame, text="Clear", command=self.clear, bg="#607D8B", fg="white").pack(side=tk.LEFT, padx=5)
        self.editor = scrolledtext.ScrolledText(self.root, bg="#0d0d0d", fg="#d4d4d4", insertbackground="white", font=("Consolas", 11))
        self.editor.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)
        self.console = scrolledtext.ScrolledText(self.root, bg="#000000", fg="#00ff00", font=("Consolas", 10), height=8)
        self.console.pack(fill=tk.X, padx=10, pady=5)
        self.console.config(state=tk.DISABLED)
    
    def log(self, msg):
        self.console.config(state=tk.NORMAL)
        self.console.insert(tk.END, msg + "\n")
        self.console.see(tk.END)
        self.console.config(state=tk.DISABLED)
    
    def _start_output_poll(self):
        def poll():
            while True:
                for line in self.bridge.get_output():
                    self.log(line)
                time.sleep(0.1)
        threading.Thread(target=poll, daemon=True).start()
    
    def attach(self):
        self.pid = find_roblox_process()
        if not self.pid:
            self.log("[!] Roblox not found")
            return
        try:
            self.memory = Memory(self.pid)
            self.injector = Injector(self.memory)
            self.status.config(text=f"Attached to PID {self.pid}")
            self.log(f"[+] Attached to PID {self.pid}")
        except Exception as e:
            self.log(f"[!] Attach failed: {e}")
    
    def inject(self):
        if not self.injector:
            self.log("[!] Not attached")
            return
        dll = os.path.join(os.path.dirname(__file__), "native", "injector.dll")
        if not os.path.exists(dll):
            self.log(f"[!] DLL not found: {dll}")
            return
        if self.injector.inject_dll(dll):
            if self.bridge.connect():
                self.log("[+] Injection successful")
                self.log("[+] Bridge connected")
            else:
                self.log("[!] Bridge connection failed")
        else:
            self.log("[!] Injection failed")
    
    def execute(self):
        code = self.editor.get("1.0", tk.END).strip()
        if not code:
            return
        if not self.bridge.connected:
            self.log("[!] Not injected")
            return
        if self.bridge.execute(code):
            self.log(f"[+] Executed: {code[:60]}...")
        else:
            self.log("[!] Execution failed")
    
    def open_script(self):
        path = filedialog.askopenfilename(filetypes=[("Lua files", "*.lua"), ("All", "*.*")])
        if path:
            with open(path, 'r', encoding='utf-8') as f:
                self.editor.delete("1.0", tk.END)
                self.editor.insert("1.0", f.read())
            self.log(f"[+] Loaded: {os.path.basename(path)}")
    
    def clear(self):
        self.editor.delete("1.0", tk.END)
        self.console.config(state=tk.NORMAL)
        self.console.delete("1.0", tk.END)
        self.console.config(state=tk.DISABLED)
    
    def run(self):
        self.root.mainloop()

def main():
    if os.name != 'nt':
        print("[!] Windows required")
        return
    app = ExecutorGUI()
    app.run()

if __name__ == "__main__":
    main()
