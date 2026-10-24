'use strict';
// Headless end-to-end test of the VS Code extension's LSP wiring.
// Spawns forge-lsp --stdio and exercises the same protocol the extension uses.

const assert = require('assert');
const path = require('path');
const fs = require('fs');
const { LspClient } = require('../lsp-client');

const SERVER = path.resolve(__dirname, '../../../build-release/tools/forge-lsp');

const SAMPLE = `let total = 0
fn add(a, b) {
    return a + b
}
for (let i in [1, 2, 3]) {
    total = add(total, i)
}
print(total)
`;

async function waitForNotifications(client, method, timeoutMs) {
    const start = Date.now();
    while (Date.now() - start < timeoutMs) {
        for (const m of client.notifications || []) {
            if (m.method === method) return m;
        }
        await new Promise((r) => setTimeout(r, 10));
    }
    return null;
}

async function main() {
    assert(fs.existsSync(SERVER), `forge-lsp not found at ${SERVER}`);

    const client = new LspClient(SERVER, ['--stdio']);
    const diags = [];
    client.onNotification('textDocument/publishDiagnostics', (p) => diags.push(p));
    client.start();

    const init = await client.request('initialize', {
        processId: process.pid,
        rootUri: null,
        capabilities: {},
    });
    assert(init.capabilities.completionProvider, 'completion provider missing');
    assert(init.capabilities.hoverProvider, 'hover provider missing');
    assert(init.capabilities.definitionProvider, 'definition provider missing');

    client.notify('initialized', {});

    const uri = 'file:///tmp/forge-vscode-test.fge';
    client.notify('textDocument/didOpen', {
        textDocument: { uri, languageId: 'forge', version: 1, text: SAMPLE },
    });

    await new Promise((r) => setTimeout(r, 200));
    assert(diags.length > 0, 'no diagnostics received');

    const comp = await client.request('textDocument/completion', {
        textDocument: { uri },
        position: { line: 8, character: 0 },
    });
    const labels = (Array.isArray(comp) ? comp : comp.items || []).map((i) => i.label);
    assert(labels.includes('let') || labels.includes('fn'), `completion missing keywords: ${labels}`);

    const hover = await client.request('textDocument/hover', {
        textDocument: { uri },
        position: { line: 0, character: 5 },
    });
    assert(hover && hover.contents, 'hover missing');

    // "add" is used on line 5, definition is at line 1
    const def = await client.request('textDocument/definition', {
        textDocument: { uri },
        position: { line: 5, character: 12 },
    });
    // LSP allows null/array/object depending on shape
    assert(def !== null && def !== undefined, 'definition missing');

    const refs = await client.request('textDocument/references', {
        textDocument: { uri },
        position: { line: 1, character: 3 },
        context: { includeDeclaration: true },
    });
    assert(Array.isArray(refs) && refs.length > 0, 'references missing');

    client.stop();
    console.log('VS Code LSP client test: ALL PASS');
}

main().catch((err) => {
    console.error(err);
    process.exit(1);
});
