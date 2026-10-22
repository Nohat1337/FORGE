'use strict';
// Helpers that drive the forgevm CLI from the editor.

const { spawn } = require('child_process');
const path = require('path');

// Runs a forgevm command. When a terminal is available the command runs in
// the user's terminal (so interactive programs keep working); otherwise the
// output is streamed to the Forge output channel.
function runForge(forgeVm, args, trusted, terminal, output, cwd) {
    const argv = typeof args === 'string' ? args.split(' ').filter(Boolean) : args;
    const full = trusted ? ['--trusted', ...argv] : argv;

    if (!forgeVm) {
        if (output) output.appendLine('[forge] forgevm not found - build it or set forge.forgeVmPath');
        return;
    }

    if (terminal) {
        const quote = (value) => (/[\s"]/.test(value) ? `"${value.replace(/"/g, '\\"')}"` : value);
        const command = [quote(forgeVm), ...full.map(quote)].join(' ');
        terminal.sendText(command);
        return;
    }

    const child = spawn(forgeVm, full, { cwd: cwd || process.cwd() });
    child.stdout.on('data', (chunk) => output && output.append(chunk.toString()));
    child.stderr.on('data', (chunk) => output && output.append(chunk.toString()));
    child.on('close', (code) => output && output.appendLine(`[forge] exited with code ${code}`));
}

function scaffoldNativeLibrary(forgeVm, name, cwd, output) {
    if (!forgeVm) {
        if (output) output.appendLine('[forge] forgevm not found');
        return;
    }
    const child = spawn(forgeVm, ['lib', 'new', name, '--cpp'], { cwd });
    child.stdout.on('data', (chunk) => output && output.append(chunk.toString()));
    child.stderr.on('data', (chunk) => output && output.append(chunk.toString()));
    child.on('close', (code) => {
        if (output) output.appendLine(`[forge] lib new exited with code ${code}`);
        if (code === 0) {
            output && output.appendLine(`[forge] next: cd ${name} && sh build.sh && forgevm --trusted main.fge`);
        }
    });
}

function scaffoldAndroidApp(forgeVm, name, cwd, output) {
    if (!forgeVm) {
        if (output) output.appendLine('[forge] forgevm not found');
        return;
    }
    const child = spawn(forgeVm, ['android', 'new', path.resolve(cwd, name)], { cwd });
    child.stdout.on('data', (chunk) => output && output.append(chunk.toString()));
    child.stderr.on('data', (chunk) => output && output.append(chunk.toString()));
    child.on('close', (code) => {
        if (output) output.appendLine(`[forge] android new exited with code ${code}`);
    });
}

module.exports = { runForge, scaffoldNativeLibrary, scaffoldAndroidApp };
