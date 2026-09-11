# bridge.py
# Python <-> DLL communication

import ctypes
import ctypes.wintypes as wintypes
import time
import threading

GENERIC_READ = 0x80000000
GENERIC_WRITE = 0x40000000
OPEN_EXISTING = 3
PIPE_NAME = r"\\.\pipe\roblox_executor"

kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)

class NamedPipe:
    def __init__(self, name=PIPE_NAME):
        self.name = name
        self.handle = None
    
    def connect(self, timeout=5.0):
        start = time.time()
        while time.time() - start < timeout:
            self.handle = kernel32.CreateFileA(
                self.name.encode(),
                GENERIC_READ | GENERIC_WRITE,
                0, None, OPEN_EXISTING, 0, None
            )
            if self.handle != -1:
                return True
            time.sleep(0.1)
        return False
    
    def write(self, data: str) -> bool:
        if not self.handle:
            return False
        data_bytes = data.encode('utf-8')
        written = wintypes.DWORD(0)
        return bool(kernel32.WriteFile(
            self.handle, data_bytes, len(data_bytes),
            ctypes.byref(written), None
        ))
    
    def read(self, size=65536) -> str:
        if not self.handle:
            return ""
        buffer = ctypes.create_string_buffer(size)
        read = wintypes.DWORD(0)
        if kernel32.ReadFile(self.handle, buffer, size, ctypes.byref(read), None):
            return buffer.raw[:read.value].decode('utf-8', errors='ignore')
        return ""
    
    def close(self):
        if self.handle and self.handle != -1:
            kernel32.CloseHandle(self.handle)
            self.handle = None

class ExecutorBridge:
    def __init__(self):
        self.pipe = NamedPipe()
        self.connected = False
        self.responses = []
        self.lock = threading.Lock()
    
    def connect(self) -> bool:
        self.connected = self.pipe.connect()
        if self.connected:
            threading.Thread(target=self._read_loop, daemon=True).start()
        return self.connected
    
    def _read_loop(self):
        while self.connected:
            data = self.pipe.read()
            if data:
                with self.lock:
                    self.responses.append(data)
            time.sleep(0.01)
    
    def execute(self, code: str) -> bool:
        if not self.connected:
            return False
        return self.pipe.write(code)
    
    def get_output(self) -> list:
        with self.lock:
            out = self.responses[:]
            self.responses.clear()
            return out
    
    def disconnect(self):
        self.connected = False
        self.pipe.close()
