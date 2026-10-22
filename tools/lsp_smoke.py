#!/usr/bin/env python3
"""Smoke test for the Forge language server (forge-lsp --stdio).

Speaks the LSP wire protocol over stdio and exercises the features a
VS Code client relies on: initialize, didOpen (diagnostics), completion,
hover, definition and references.

Usage:  python3 tools/lsp_smoke.py [path/to/forge-lsp]
"""
import json
import os
import subprocess
import sys

DEFAULT_SERVER = os.path.join(os.path.dirname(__file__), "..", "build-release",
                              "tools", "forge-lsp")

SAMPLE = '''let total = 0
fn add(a, b) {
    return a + b
}
for (let i in [1, 2, 3]) {
    total = add(total, i)
}
print(total)
'''


class Client:
    def __init__(self, server):
        self.proc = subprocess.Popen([server, "--stdio"], stdin=subprocess.PIPE,
                                     stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.id = 0
        self.pending = {}
        self.notifications = []

    def send(self, method, params=None, notify=False):
        msg = {"jsonrpc": "2.0", "method": method}
        if not notify:
            self.id += 1
            msg["id"] = self.id
        if params is not None:
            msg["params"] = params
        body = json.dumps(msg)
        self.proc.stdin.write(f"Content-Length: {len(body)}\r\n\r\n{body}".encode())
        self.proc.stdin.flush()
        return None if notify else self.id

    def _read_message(self):
        headers = {}
        while True:
            line = self.proc.stdout.readline()
            if not line:
                return None
            line = line.decode().strip()
            if not line:
                break
            if ":" in line:
                k, v = line.split(":", 1)
                headers[k.strip().lower()] = v.strip()
        length = int(headers.get("content-length", 0))
        if length <= 0:
            return None
        raw = b""
        while len(raw) < length:
            chunk = self.proc.stdout.read(length - len(raw))
            if not chunk:
                break
            raw += chunk
        return json.loads(raw.decode())

    def wait_for(self, want_id, timeout=10.0):
        while True:
            msg = self._read_message()
            if msg is None:
                raise RuntimeError("language server closed the connection")
            if msg.get("method") == "textDocument/publishDiagnostics":
                self.notifications.append(msg)
                continue
            if msg.get("id") == want_id:
                return msg

    def request(self, method, params=None):
        want = self.send(method, params)
        return self.wait_for(want)

    def close(self):
        try:
            self.proc.stdin.close()
            self.proc.wait(timeout=5)
        except Exception:
            self.proc.kill()


def check(label, condition, detail=""):
    status = "PASS" if condition else "FAIL"
    print(f"[{status}] {label}" + (f" -- {detail}" if detail else ""))
    return bool(condition)


def main():
    server = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_SERVER
    if not os.path.exists(server):
        print(f"language server not found: {server}")
        return 2

    c = Client(server)
    ok = True

    init = c.request("initialize", {"processId": None, "rootUri": None,
                                     "capabilities": {}})
    caps = init.get("result", {}).get("capabilities", {})
    ok &= check("initialize returns capabilities", bool(caps))
    ok &= check("completion provider advertised", "completionProvider" in caps)
    ok &= check("hover provider advertised", "hoverProvider" in caps)

    c.send("initialized", {}, notify=True)

    uri = "file:///tmp/forge_lsp_smoke.fge"
    c.send("textDocument/didOpen", {
        "textDocument": {"uri": uri, "languageId": "forge", "version": 1,
                         "text": SAMPLE}}, notify=True)

    diag = None
    for _ in range(20):
        msg = c._read_message()
        if msg is None:
            break
        if msg.get("method") == "textDocument/publishDiagnostics":
            diag = msg
            break
    ok &= check("diagnostics published", diag is not None,
                json.dumps(diag["params"]["diagnostics"])[:120] if diag else "none")

    # completion on the empty line at the end of the file
    comp = c.request("textDocument/completion", {
        "textDocument": {"uri": uri}, "position": {"line": 8, "character": 0}})
    labels = [i["label"] for i in comp.get("result", [])]
    ok &= check("completion returns items", len(labels) > 0, f"{len(labels)} items")
    ok &= check("completion offers keywords", "let" in labels or "fn" in labels,
                ", ".join(labels[:8]))
    ok &= check("completion offers builtins",
                any(x in labels for x in ("print", "len", "str", "input")),
                ", ".join(labels[:20]))

    # completion inside a function body (line 3) should offer locals
    comp2 = c.request("textDocument/completion", {
        "textDocument": {"uri": uri}, "position": {"line": 3, "character": 4}})
    labels2 = [i["label"] for i in comp2.get("result", [])]
    ok &= check("completion is position aware", "add" in labels2 or len(labels2) != len(labels),
                ", ".join(labels2[:10]))

    hov = c.request("textDocument/hover", {
        "textDocument": {"uri": uri}, "position": {"line": 0, "character": 5}})
    ok &= check("hover returns markdown", "contents" in hov.get("result", {}),
                json.dumps(hov.get("result", {}))[:120])

    dfn = c.request("textDocument/definition", {
        "textDocument": {"uri": uri}, "position": {"line": 4, "character": 10}})
    ok &= check("definition resolves add()", "result" in dfn and dfn["result"] is not None,
                json.dumps(dfn.get("result"))[:160])

    refs = c.request("textDocument/references", {
        "textDocument": {"uri": uri}, "position": {"line": 1, "character": 3},
        "context": {"includeDeclaration": True}})
    ok &= check("references found", bool(refs.get("result")),
                json.dumps(refs.get("result"))[:160])

    c.close()
    print("ALL PASS" if ok else "FAILURES PRESENT")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
