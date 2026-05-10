"""
VioSpice UI Proxy — Python client for the Qt GUI command server.

Usage:
    from vspice.ui import UIProxy

    ui = UIProxy()
    ui.connect()  # Connect to localhost:18790

    # Show a message in the GUI
    ui.show_message("Hello", "This is a message from Python!")

    # Add a menu item
    ui.add_menu_item("Tools", "Run My Script", code="print('Hello from menu!')")

    # Get schematic context
    ctx = ui.get_schematic_context()
    print(ctx)

    # Run Python code in the GUI context
    result = ui.run_python_code("import vspice; print(vspice.__version__)")

    # Listen for menu item triggers
    @ui.on_menu_item_triggered
    def handler(event):
        print(f"Menu item triggered: {event['label']}")
        # Execute the code if provided
        if event.get('code'):
            exec(event['code'])

    # Keep the client alive to receive events
    ui.run_forever()
"""

import json
import socket
import threading
import time
from typing import Any, Callable, Dict, List, Optional

try:
    import websocket
except ImportError:
    websocket = None


class UIProxy:
    """WebSocket client for the VioSpice UI Command Server."""

    def __init__(self, host: str = "localhost", port: int = 18790):
        self.host = host
        self.port = port
        self.url = f"ws://{host}:{port}"
        self.ws = None
        self._connected = False
        self._callbacks: List[Callable[[Dict[str, Any]], None]] = []
        self._menu_item_callbacks: List[Callable[[Dict[str, Any]], None]] = []
        self._lock = threading.Lock()
        self._response_queue: Dict[str, Dict[str, Any]] = {}
        self._request_id = 0

    def connect(self, timeout: float = 5.0) -> bool:
        """Connect to the UI command server (WebSocket with TCP fallback)."""
        if self._connected:
            return True

        # Try WebSocket first if available
        if websocket is not None:
            try:
                self.ws = websocket.create_connection(
                    self.url, timeout=timeout, enable_multithread=True
                )
                self._connected = True
                self._recv_thread = threading.Thread(target=self._recv_loop, daemon=True)
                self._recv_thread.start()
                return True
            except Exception:
                pass

        # Fallback to Raw TCP
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.settimeout(timeout)
            self.sock.connect((self.host, self.port))
            self._connected = True
            self.ws = None # Ensure WS is marked as none
            self._recv_thread = threading.Thread(target=self._recv_loop_tcp, daemon=True)
            self._recv_thread.start()
            return True
        except Exception as e:
            self._connected = False
            raise ConnectionError(f"Failed to connect to {self.host}:{self.port} (WS and TCP): {e}")

    def disconnect(self):
        """Disconnect from the server."""
        self._connected = False
        if self.ws:
            try: self.ws.close()
            except: pass
            self.ws = None
        if hasattr(self, 'sock') and self.sock:
            try: self.sock.close()
            except: pass
            self.sock = None

    def is_connected(self) -> bool:
        """Check if connected to the server."""
        return self._connected

    def _send_request(self, cmd: str, params: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        """Send a command and wait for response."""
        if not self._connected:
            raise ConnectionError("Not connected to UI server. Call connect() first.")

        with self._lock:
            self._request_id += 1
            req_id = self._request_id

        request = {"id": req_id, "cmd": cmd, "params": params or {}}
        data = json.dumps(request)
        
        if self.ws:
            self.ws.send(data)
        elif hasattr(self, 'sock') and self.sock:
            self.sock.sendall(data.encode('utf-8'))

        # Wait for response with timeout
        start = time.time()
        while time.time() - start < 10.0:
            with self._lock:
                if req_id in self._response_queue:
                    return self._response_queue.pop(req_id)
            time.sleep(0.05)

        raise TimeoutError(f"No response for command '{cmd}' within 10 seconds")

    def _recv_loop_tcp(self):
        """Background thread to receive messages over TCP."""
        while self._connected and hasattr(self, 'sock') and self.sock:
            try:
                msg = self.sock.recv(16384).decode('utf-8')
                if not msg: break
                self._handle_message(msg)
            except Exception:
                break
        self._connected = False

    def _recv_loop(self):
        """Background thread to receive messages over WebSocket."""
        while self._connected and self.ws:
            try:
                msg = self.ws.recv()
                if not msg: break
                self._handle_message(msg)
            except Exception:
                break
        self._connected = False

    def _handle_message(self, msg: str):
        """Process a message received from the server."""
        try:
            data = json.loads(msg)
            req_id = data.get("id")
            if req_id is not None:
                with self._lock:
                    self._response_queue[req_id] = data
            else:
                msg_type = data.get("type", "")
                if msg_type == "menu_item_triggered":
                    for cb in self._menu_item_callbacks:
                        try: cb(data)
                        except: pass
                else:
                    for cb in self._callbacks:
                        try: cb(data)
                        except: pass
        except Exception:
            pass

    def on_broadcast(self, callback: Callable[[Dict[str, Any]], None]):
        """Register a callback for all broadcast messages."""
        self._callbacks.append(callback)

    def on_menu_item_triggered(self, callback: Callable[[Dict[str, Any]], None]):
        """Register a callback for menu item triggers."""
        self._menu_item_callbacks.append(callback)

    # -----------------------------------------------------------------------
    # UI Commands
    # -----------------------------------------------------------------------

    def show_message(self, title: str, text: str, type: str = "info") -> Dict[str, Any]:
        """Show a message dialog in the GUI."""
        return self._send_request("show_message", {
            "title": title,
            "text": text,
            "type": type,  # "info", "warning", "error"
        })

    def add_menu_item(
        self,
        menu: str,
        label: str,
        code: str = "",
        menu_id: Optional[str] = None,
    ) -> Dict[str, Any]:
        """Add a menu item to the GUI's menu bar.

        When clicked, the item triggers a broadcast to all connected Python
        clients with the provided code.
        """
        params = {
            "menu": menu,
            "label": label,
            "code": code,
        }
        if menu_id:
            params["id"] = menu_id
        return self._send_request("add_menu_item", params)

    def remove_menu_item(self, menu_id: str) -> Dict[str, Any]:
        """Remove a previously added menu item."""
        return self._send_request("remove_menu_item", {"id": menu_id})

    def run_python_code(self, code: str) -> Dict[str, Any]:
        """Execute Python code in the GUI's Python context."""
        return self._send_request("run_python_code", {"code": code})

    def open_schematic(self, path: str) -> Dict[str, Any]:
        """Open a schematic or netlist file in the GUI."""
        return self._send_request("open_schematic", {"path": path})

    def get_schematic_context(self) -> Dict[str, Any]:
        """Get information about the current schematic."""
        return self._send_request("get_schematic_context")

    def ping(self) -> Dict[str, Any]:
        """Ping the server to check connectivity."""
        return self._send_request("ping")

    def run_forever(self):
        """Block forever to keep the client alive for receiving events."""
        try:
            while self._connected:
                time.sleep(0.1)
        except KeyboardInterrupt:
            pass
        finally:
            self.disconnect()


# Module-level convenience
_default_proxy: Optional[UIProxy] = None


def get_proxy(host: str = "localhost", port: int = 18790) -> UIProxy:
    """Get or create the default UI proxy."""
    global _default_proxy
    if _default_proxy is None:
        _default_proxy = UIProxy(host, port)
    return _default_proxy


def connect(host: str = "localhost", port: int = 18790, **kwargs) -> UIProxy:
    """Connect to the UI command server."""
    proxy = get_proxy(host, port)
    if not proxy.is_connected():
        proxy.connect(**kwargs)
    return proxy


def show_message(title: str, text: str, **kwargs):
    """Show a message in the GUI (auto-connects if needed)."""
    return connect().show_message(title, text, **kwargs)


def add_menu_item(menu: str, label: str, **kwargs):
    """Add a menu item to the GUI."""
    return connect().add_menu_item(menu, label, **kwargs)


def run_python_code(code: str):
    """Execute Python code in the GUI context."""
    return connect().run_python_code(code)
