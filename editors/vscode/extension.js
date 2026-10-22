'use strict';
// Forge language support for VS Code.
//
// The extension owns a `forge-lsp` process (see lsp-client.js) and wires
// its capabilities into VS Code: diagnostics, completion, hover,
// go-to-definition and find-references. It also drives the forgevm CLI so a
// project can be run, built, exported as a shared library or scaffolded
// without leaving the editor.

const vscode = require('vscode');
const { LspClient, findLanguageServer, findForgeVm } = require('./lsp-client');
const { runForge, scaffoldNativeLibrary, scaffoldAndroidApp } = require('./commands');

const DOCUMENT_SELECTOR = [
    { language: 'forge', scheme: 'file' },
    { language: 'forge', scheme: 'untitled' },
];

const DIAGNOSTIC_COLLECTION_NAME = 'forge';

let client = null;
let diagnostics = null;
let output = null;

function log(message) {
    if (output) output.appendLine(message);
}

function serverPath() {
    const config = vscode.workspace.getConfiguration('forge');
    return findLanguageServer(config.get('languageServerPath'), vscode.extensions.getExtension('forge-lang.forge-lang').extensionPath);
}

function forgeVmPath() {
    const config = vscode.workspace.getConfiguration('forge');
    return findForgeVm(config.get('forgeVmPath'), vscode.extensions.getExtension('forge-lang.forge-lang').extensionPath);
}

function startServer() {
    const binary = serverPath();
    if (!binary) {
        vscode.window.showErrorMessage(
            'Forge: forge-lsp not found. Build it (cmake --build build-release --target forge-lsp) or set forge.languageServerPath.'
        );
        return;
    }

    client = new LspClient(binary, ['--stdio'], {
        onError: (err) => log(`[forge-lsp] ${err.message}`),
        onExit: (code) => log(`[forge-lsp] exited with code ${code}`),
    });

    client.onNotification('textDocument/publishDiagnostics', (params) => {
        if (!diagnostics) return;
        const uri = params.uri;
        const items = (params.diagnostics || []).map((d) => {
            const range = new vscode.Range(
                new vscode.Position(d.range.start.line, d.range.start.character),
                new vscode.Position(d.range.end.line, d.range.end.character)
            );
            const severity = d.severity === 1
                ? vscode.DiagnosticSeverity.Error
                : d.severity === 2
                    ? vscode.DiagnosticSeverity.Warning
                    : vscode.DiagnosticSeverity.Information;
            return new vscode.Diagnostic(range, d.message, severity);
        });
        diagnostics.set(uri, items);
    });

    client.start();

    client
        .request('initialize', {
            processId: process.pid,
            rootUri: vscode.workspace.workspaceFolders && vscode.workspace.workspaceFolders.length
                ? vscode.workspace.workspaceFolders[0].uri.toString()
                : null,
            capabilities: {
                textDocument: {
                    completion: { completionItem: { snippetSupport: false } },
                    hover: { contentFormat: ['markdown', 'plaintext'] },
                    definition: {},
                    references: {},
                },
                workspace: { workspaceFolders: true },
            },
            workspaceFolders: (vscode.workspace.workspaceFolders || []).map((f) => ({
                uri: f.uri.toString(),
                name: f.name,
            })),
        })
        .then((result) => {
            log(`[forge-lsp] initialized: ${JSON.stringify(result && result.capabilities ? Object.keys(result.capabilities) : {})}`);
            client.notify('initialized', {});
        })
        .catch((err) => {
            vscode.window.showErrorMessage(`Forge: language server failed to initialize: ${err.message}`);
        });
}

function stopServer() {
    if (client) {
        client.stop();
        client = null;
    }
}

function registerProviders(context) {
    diagnostics = vscode.languages.createDiagnosticCollection(DIAGNOSTIC_COLLECTION_NAME);
    context.subscriptions.push(diagnostics);

    context.subscriptions.push(
        vscode.languages.registerCompletionItemProvider(
            DOCUMENT_SELECTOR,
            {
                async provideCompletionItems(document, position) {
                    if (!client) return null;
                    try {
                        const result = await client.request('textDocument/completion', {
                            textDocument: { uri: document.uri.toString() },
                            position: { line: position.line, character: position.character },
                        });
                        const items = Array.isArray(result) ? result : (result && result.items) || [];
                        return items.map((item) => {
                            const completion = new vscode.CompletionItem(item.label, completionKind(item.kind));
                            if (item.detail) completion.detail = item.detail;
                            if (item.documentation) completion.documentation = item.documentation;
                            if (item.insertText) completion.insertText = item.insertText;
                            return completion;
                        });
                    } catch (err) {
                        log(`[forge-lsp] completion failed: ${err.message}`);
                        return null;
                    }
                },
            },
            '.',
            ':',
            '"'
        )
    );

    context.subscriptions.push(
        vscode.languages.registerHoverProvider(DOCUMENT_SELECTOR, {
            async provideHover(document, position) {
                if (!client) return null;
                try {
                    const result = await client.request('textDocument/hover', {
                        textDocument: { uri: document.uri.toString() },
                        position: { line: position.line, character: position.character },
                    });
                    if (!result || !result.contents) return null;
                    const value = result.contents.value || result.contents;
                    const range = result.range
                        ? new vscode.Range(
                            new vscode.Position(result.range.start.line, result.range.start.character),
                            new vscode.Position(result.range.end.line, result.range.end.character)
                        )
                        : undefined;
                    return new vscode.Hover(new vscode.MarkdownString(value), range);
                } catch (err) {
                    log(`[forge-lsp] hover failed: ${err.message}`);
                    return null;
                }
            },
        })
    );

    context.subscriptions.push(
        vscode.languages.registerDefinitionProvider(DOCUMENT_SELECTOR, {
            async provideDefinition(document, position) {
                if (!client) return null;
                try {
                    const result = await client.request('textDocument/definition', {
                        textDocument: { uri: document.uri.toString() },
                        position: { line: position.line, character: position.character },
                    });
                    return (result || []).map(toLocation);
                } catch (err) {
                    log(`[forge-lsp] definition failed: ${err.message}`);
                    return null;
                }
            },
        })
    );

    context.subscriptions.push(
        vscode.languages.registerReferenceProvider(DOCUMENT_SELECTOR, {
            async provideReferences(document, position) {
                if (!client) return null;
                try {
                    const result = await client.request('textDocument/references', {
                        textDocument: { uri: document.uri.toString() },
                        position: { line: position.line, character: position.character },
                        context: { includeDeclaration: true },
                    });
                    return (result || []).map(toLocation);
                } catch (err) {
                    log(`[forge-lsp] references failed: ${err.message}`);
                    return null;
                }
            },
        })
    );

    // Document synchronisation
    context.subscriptions.push(
        vscode.workspace.onDidOpenTextDocument((document) => {
            if (!client || !isForge(document)) return;
            client.notify('textDocument/didOpen', {
                textDocument: {
                    uri: document.uri.toString(),
                    languageId: 'forge',
                    version: document.version,
                    text: document.getText(),
                },
            });
        })
    );

    context.subscriptions.push(
        vscode.workspace.onDidChangeTextDocument((event) => {
            if (!client || !isForge(event.document)) return;
            client.notify('textDocument/didChange', {
                textDocument: { uri: event.document.uri.toString(), version: event.document.version },
                contentChanges: [{ text: event.document.getText() }],
            });
        })
    );

    context.subscriptions.push(
        vscode.workspace.onDidSaveTextDocument((document) => {
            if (!client || !isForge(document)) return;
            client.notify('textDocument/didSave', {
                textDocument: { uri: document.uri.toString() },
            });
        })
    );

    context.subscriptions.push(
        vscode.workspace.onDidCloseTextDocument((document) => {
            if (!client || !isForge(document)) return;
            client.notify('textDocument/didClose', {
                textDocument: { uri: document.uri.toString() },
            });
        })
    );
}

function isForge(document) {
    return document && document.languageId === 'forge';
}

function toLocation(item) {
    const range = item.range || item;
    const uri = vscode.Uri.parse(item.uri || item.targetUri);
    return new vscode.Location(
        uri,
        new vscode.Range(
            new vscode.Position(range.start.line, range.start.character),
            new vscode.Position(range.end.line, range.end.character)
        )
    );
}

function completionKind(kind) {
    switch (kind) {
        case 2: return vscode.CompletionItemKind.Method;
        case 3: return vscode.CompletionItemKind.Function;
        case 4: return vscode.CompletionItemKind.Constructor;
        case 5: return vscode.CompletionItemKind.Field;
        case 6: return vscode.CompletionItemKind.Variable;
        case 14: return vscode.CompletionItemKind.Keyword;
        case 21: return vscode.CompletionItemKind.Constant;
        default: return vscode.CompletionItemKind.Text;
    }
}

function workspaceRoot() {
    const folders = vscode.workspace.workspaceFolders;
    if (folders && folders.length) return folders[0].uri.fsPath;
    return vscode.workspace.workspaceFile ? vscode.workspace.workspaceFile.fsPath : process.cwd();
}

async function activeForgeFile() {
    const editor = vscode.window.activeTextEditor;
    if (editor && isForge(editor.document)) return editor.document;
    const documents = vscode.workspace.textDocuments.filter(isForge);
    if (documents.length) return documents[0];
    return null;
}

function activate(context) {
    output = vscode.window.createOutputChannel('Forge');
    context.subscriptions.push(output);

    registerProviders(context);
    startServer();

    const register = (command, handler) =>
        context.subscriptions.push(vscode.commands.registerCommand(command, handler));

    register('forge.runFile', async () => {
        const document = await activeForgeFile();
        if (!document) {
            vscode.window.showWarningMessage('Forge: open a .fge file first.');
            return;
        }
        runForge(forgeVmPath(), document.uri.fsPath, false, vscode.window.activeTerminal, output);
    });

    register('forge.runTrusted', async () => {
        const document = await activeForgeFile();
        if (!document) {
            vscode.window.showWarningMessage('Forge: open a .fge file first.');
            return;
        }
        runForge(forgeVmPath(), document.uri.fsPath, true, vscode.window.activeTerminal, output);
    });

    register('forge.buildProject', async () => {
        runForge(forgeVmPath(), 'build', false, vscode.window.activeTerminal, output, workspaceRoot());
    });

    register('forge.exportLibrary', async () => {
        runForge(forgeVmPath(), 'build --shared', false, vscode.window.activeTerminal, output, workspaceRoot());
    });

    register('forge.newNativeLibrary', async () => {
        const name = await vscode.window.showInputBox({
            prompt: 'Name of the native library (C ABI, C++ internals)',
            placeHolder: 'mathx',
        });
        if (!name) return;
        scaffoldNativeLibrary(forgeVmPath(), name, workspaceRoot(), output);
    });

    register('forge.newAndroidApp', async () => {
        const name = await vscode.window.showInputBox({
            prompt: 'Name of the Android app directory',
            placeHolder: 'myapp',
        });
        if (!name) return;
        scaffoldAndroidApp(forgeVmPath(), name, workspaceRoot(), output);
    });

    register('forge.restartServer', () => {
        stopServer();
        startServer();
    });
}

function deactivate() {
    stopServer();
}

module.exports = { activate, deactivate };
