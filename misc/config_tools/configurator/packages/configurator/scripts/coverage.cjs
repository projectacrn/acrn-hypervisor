#!/usr/bin/env node
/**
 * Merge Vitest and native WebDriver coverage into a single Istanbul report.
 *
 * Why the remap step exists: Vitest instruments modules through Vite's SSR
 * transform, so its coverage already uses original source coordinates. The
 * native e2e suite collects window.__coverage__ from the bundled application,
 * whose positions refer to the client build output. Merging those two maps
 * directly double counts statements and lowers the reported coverage. The e2e
 * entries carry inputSourceMap, so they are remapped back to original sources
 * before merging.
 *
 * Usage: node scripts/coverage.cjs [check]
 *   COVERAGE_MIN   line/function/branch/statement threshold, default 95
 *
 * Inputs (produced by test:unit:coverage and test:e2e:coverage):
 *   build/coverage/vitest/coverage-final.json
 *   build/e2e-artifacts/coverage/coverage.json
 * Outputs:
 *   build/coverage-merged/*.json   normalized inputs, one file per suite
 *   build/coverage-report/         text, html and json-summary reports
 */
const fs = require('fs');
const path = require('path');
const {spawnSync} = require('child_process');

const packageRoot = path.resolve(__dirname, '..');
const workspaceNodeModules = path.resolve(packageRoot, '..', '..', 'node_modules');
const buildDir = path.join(packageRoot, 'build');
const vitestCoverage = path.join(buildDir, 'coverage', 'vitest', 'coverage-final.json');
const e2eCoverage = path.join(buildDir, 'e2e-artifacts', 'coverage', 'coverage.json');
const mergedDir = path.join(buildDir, 'coverage-merged');
const reportDir = path.join(buildDir, 'coverage-report');
const nycBin = path.join(workspaceNodeModules, 'nyc', 'bin', 'nyc.js');

function loadWorkspace(moduleName) {
    return require(path.join(workspaceNodeModules, moduleName));
}

async function remapToSources(inputPath, outputPath) {
    const {createCoverageMap} = loadWorkspace('istanbul-lib-coverage');
    const {createSourceMapStore} = loadWorkspace('istanbul-lib-source-maps');
    const raw = JSON.parse(fs.readFileSync(inputPath, 'utf8'));
    const store = createSourceMapStore();
    const remapped = await store.transformCoverage(createCoverageMap(raw));
    const json = typeof remapped.toJSON === 'function' ? remapped.toJSON() : remapped;
    fs.writeFileSync(outputPath, JSON.stringify(json));
}

function runNyc(args) {
    const result = spawnSync(process.execPath, [nycBin, ...args], {
        cwd: packageRoot,
        stdio: 'inherit',
    });
    if (result.error) {
        throw result.error;
    }
    return result.status === 0;
}

async function main() {
    const minimum = String(Number(process.env.COVERAGE_MIN || 95));
    const check = process.argv[2] !== 'report';
    const missing = [vitestCoverage, e2eCoverage].filter(file => !fs.existsSync(file));
    if (missing.length > 0) {
        console.error('Missing coverage input(s):\n  ' + missing.join('\n  '));
        console.error('Run test:unit:coverage and test:e2e:coverage first.');
        process.exitCode = 1;
        return;
    }

    fs.rmSync(mergedDir, {recursive: true, force: true});
    fs.mkdirSync(mergedDir, {recursive: true});
    fs.copyFileSync(vitestCoverage, path.join(mergedDir, 'vitest.json'));
    await remapToSources(e2eCoverage, path.join(mergedDir, 'e2e.json'));

    const reported = runNyc([
        'report',
        '--temp-dir', mergedDir,
        '--report-dir', reportDir,
        '--reporter=text',
        '--reporter=html',
        '--reporter=json-summary',
    ]);

    if (!reported) {
        console.error('Failed to generate the merged coverage report.');
        process.exitCode = 1;
        return;
    }
    if (!check) {
        return;
    }
    const passed = runNyc([
        'check-coverage',
        '--temp-dir', mergedDir,
        '--lines', minimum,
        '--functions', minimum,
        '--branches', minimum,
        '--statements', minimum,
    ]);
    if (!passed) {
        console.error(`Merged coverage is below ${minimum}%. See ${path.relative(packageRoot, reportDir)}/index.html`);
        process.exitCode = 1;
    }
}

main().catch(error => {
    console.error(error);
    process.exitCode = 1;
});
