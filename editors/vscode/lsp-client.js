'use strict';
// Minimal, dependency-free LSP client over stdio.
//
// Shared by the VS Code extension and the headless test
// (editors/vscode/test/client.test.js), so both talk to the language
// server exactly the same way.

const { spawn } = require('child_process');
const path = require('path');
const fs = require('fs');
const os = require('os');

function encodeMessage(message) {
    const body = JSON.stringify(message);
    return `Content-Length: ${Buffer.byteLength(body, 'utf8')}\r\n\r\n${body}`;
}

class LspClient {
    constructor(command, args = [], options = {}) {
        this.command = command;
        this.args = args;
        this.options = options;
        this.process = null;
        this.nextId = 1;
        this.pending = new Map();
        this.notificationHandlers = new Map();
        this.buffer = Buffer.alloc(0);
        this.onError = options.onError || (() => {});
        this.onExit = options.onExit || (() => {});
    }

    start() {
        this.process = spawn(this.command, this.args, {
            stdio: ['pipe', 'pipe', 'pipe'],
            cwd: this.options.cwd,
        });
        this.process.stdout.on('data', (chunk) => this._onData(chunk));
        this.process.stderr.on('data', (chunk) => {
            const text = chunk.toString('utf8').trim();
            if (text) this.onError(new Error(text));
        });
        this.process.on('exit', (code) => this.onExit(code));
        return this;
    }

    _onData(chunk) {
        this.buffer = Buffer.concat([this.buffer, chunk]);
        for (;;) {
            const headerEnd = this.buffer.indexOf('\r\n\r\n');
            if (headerEnd < 0) return;
            const header = this.buffer.slice(0, headerEnd).toString('utf8');
            const match = /content-length:\s*(\d+)/i.exec(header);
            if (!match) {
                this.buffer = this.buffer.slice(headerEnd + 4);
                continue;
            }
            const length = parseInt(match[1], 10);
            const start = headerEnd + 4;
            if (this.buffer.length < start + length) return;
            const body = this.buffer.slice(start, start + length).toString('utf8');
            this.buffer = this.buffer.slice(start + length);
            let message;
            try {
                message = JSON.parse(body);
            } catch (err) {
                continue;
            }
            this._dispatch(message);
        }
    }

    _dispatch(message) {
        if (message.id !== undefined && this.pending.has(message.id)) {
            const { resolve, reject } = this.pending.get(message.id);
            this.pending.delete(message.id);
            if (message.error) reject(new Error(message.error.message || 'LSP error'));
            else resolve(message.result);
            return;
        }
        if (message.method) {
            const handler = this.notificationHandlers.get(message.method);
            if (handler) handler(message.params);
        }
    }

    onNotification(method, handler) {
        this.notificationHandlers.set(method, handler);
    }

    notify(method, params) {
        if (!this.process) throw new Error('language server is not running');
        this.process.stdin.write(encodeMessage({ jsonrpc: '2.0', method, params }));
    }

    request(method, params) {
        if (!this.process) throw new Error('language server is not running');
        const id = this.nextId++;
        const promise = new Promise((resolve, reject) => {
            this.pending.set(id, { resolve, reject });
            setTimeout(() => {
                if (this.pending.has(id)) {
                    this.pending.delete(id);
                    reject(new Error(`LSP request timeout: ${method}`));
                }
            }, 10000);
        });
        this.process.stdin.write(encodeMessage({ jsonrpc: '2.0', id, method, params }));
        return promise;
    }

    stop() {
        if (!this.process) return;
        try {
            this.process.kill();
        } catch (err) {
            /* ignore */
        }
        this.process = null;
    }
}

// Locates the forge-lsp binary: explicit setting, PATH, then the usual
// build/install locations relative to the repository or the extension.
function findLanguageServer(configuredPath, extensionPath) {
    const candidates = [];

    if (configuredPath) candidates.push(configuredPath);

    const pathDirs = (process.env.PATH || '').split(path.delimiter);
    for (const dir of pathDirs) {
        if (dir) candidates.push(path.join(dir, 'forge-lsp'));
    }

    const roots = [];
    if (extensionPath) {
        // <repo>/editors/vscode -> <repo>
        roots.push(path.resolve(extensionPath, '..', '..'));
        roots.push(path.resolve(extensionPath, '..', '..', '..'));
    }
    roots.push(process.cwd());
    roots.push(path.join(os.homedir(), '.local', 'bin'));
    roots.push('/usr/local/bin');
    roots.push('/usr/bin');

    for (const root of roots) {
        candidates.push(path.join(root, 'build-release', 'tools', 'forge-lsp'));
        candidates.push(path.join(root, 'build', 'tools', 'forge-lsp'));
        candidates.push(path.join(root, 'build-release', 'forge-lsp'));
        candidates.push(path.join(root, 'bin', 'forge-lsp'));
    }

    for (const candidate of candidates) {
        try {
            if (fs.existsSync(candidate) && fs.statSync(candidate).isFile()) return candidate;
        } catch (err) {
            /* ignore */
        }
    }
    return null;
}

// Locates the forgevm binary (used by the run/build commands).
function findForgeVm(configuredPath, extensionPath) {
    const candidates = [];
    if (configuredPath) candidates.push(configuredPath);

    const pathDirs = (process.env.PATH || '').split(path.delimiter);
    for (const dir of pathDirs) {
        if (dir) candidates.push(path.join(dir, 'forgevm'));
    }

    const roots = [];
    if (extensionPath) {
        roots.push(path.resolve(extensionPath, '..', '..'));
        roots.push(path.resolve(extensionPath, '..', '..', '..'));
    }
    roots.push(process.cwd());
    roots.push(path.join(os.homedir(), '.local', 'bin'));
    roots.push('/usr/local/bin');

    for (const root of roots) {
        candidates.push(path.join(root, 'build-release', 'forgevm'));
        candidates.push(path.join(root, 'build', 'forgevm'));
        candidates.push(path.join(root, 'bin', 'forgevm'));
    }

    for (const candidate of candidates) {
        try {
            if (fs.existsSync(candidate) && fs.statSync(candidate).isFile()) return candidate;
        } catch (err) {
            /* ignore */
        }
    }
    return null;
}

module.exports = { LspClient, findLanguageServer, findForgeVm, encodeMessage };
