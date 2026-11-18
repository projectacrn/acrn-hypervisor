import assert from 'node:assert/strict'
import {spawn, spawnSync} from 'node:child_process'
import {createWriteStream} from 'node:fs'
import fs from 'node:fs/promises'
import net from 'node:net'
import os from 'node:os'
import path from 'node:path'
import {after, before, describe, test} from 'node:test'
import {fileURLToPath} from 'node:url'
import {waitUntil, WebDriverClient} from './webdriver.js'

const packageRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..')
const repositoryRoot = path.resolve(packageRoot, '../../../../..')
const application = process.env.TAURI_E2E_BINARY ||
    path.join(packageRoot, 'src-tauri', 'target', 'release', 'acrn-configurator')
const artifacts = path.join(packageRoot, 'build', 'e2e-artifacts')

async function freePort() {
    const server = net.createServer()
    await new Promise((resolve, reject) => {
        server.once('error', reject)
        server.listen(0, '127.0.0.1', resolve)
    })
    const {port} = server.address()
    await new Promise(resolve => server.close(resolve))
    return port
}

async function clickByText(driver, selector, text) {
    return waitUntil(async () => {
        const elements = await driver.findElements(selector)
        for (const element of elements) {
            if (await driver.displayed(element) &&
                (await driver.text(element)).toLowerCase().includes(text.toLowerCase())) {
                await driver.click(element)
                return element
            }
        }
        return false
    }, {timeout: 10000, message: `${selector} containing ${JSON.stringify(text)} to be clickable`})
}

async function clickByTextWithin(driver, root, selector, text) {
    return waitUntil(async () => {
        const elements = await driver.findElementsWithin(root, selector)
        for (const element of elements) {
            if (await driver.displayed(element) &&
                (await driver.text(element)).toLowerCase().includes(text.toLowerCase())) {
                await driver.click(element)
                return element
            }
        }
        return false
    }, {
        timeout: 10000,
        message: `${selector} containing ${JSON.stringify(text)} within ${root} to be clickable`
    })
}

async function welcomeForm(driver, title) {
    for (const form of await driver.findElements('form')) {
        if ((await driver.text(form)).includes(title)) {
            return form
        }
    }
    throw new Error(`Could not find Welcome form ${JSON.stringify(title)}`)
}

async function expandAccordion(driver, title) {
    const buttons = await driver.findElements('.accordion-button')
    for (const button of buttons) {
        if ((await driver.text(button)).includes(title)) {
            if (await driver.attribute(button, 'aria-expanded') !== 'true') {
                await driver.click(button)
                await waitUntil(async () =>
                    await driver.attribute(button, 'aria-expanded') === 'true', {
                    message: `${title} accordion to expand`
                })
            }
            return
        }
    }
    throw new Error(`Could not find accordion ${JSON.stringify(title)}`)
}

async function selectVMTab(driver, name) {
    await clickByText(driver, '.TabBox .Tab', name)
    await waitUntil(async () => {
        const tabs = await driver.findElements('.TabBox .Tab.Active')
        return tabs.length === 1 && (await driver.text(tabs[0])).includes(name)
    }, {message: `${name} VM tab to become active`})
}

async function setFormControlValue(driver, selector, value, occurrence = -1) {
    const updated = await driver.executeScript(`
        const controls = document.querySelectorAll(arguments[0]);
        const control = arguments[2] < 0 ? controls[controls.length - 1] : controls[arguments[2]];
        if (!control) return 'missing';
        if (control.disabled) return 'disabled';
        const prototype = control instanceof HTMLSelectElement ? HTMLSelectElement.prototype :
            control instanceof HTMLTextAreaElement ? HTMLTextAreaElement.prototype : HTMLInputElement.prototype;
        Object.getOwnPropertyDescriptor(prototype, 'value').set.call(control, String(arguments[1]));
        control.dispatchEvent(new Event(control instanceof HTMLSelectElement ? 'change' : 'input', {bubbles: true}));
        control.dispatchEvent(new Event('change', {bubbles: true}));
        return control.value;
    `, [selector, value, occurrence])
    assert.equal(updated, String(value), `Could not update ${selector}`)
    await waitUntil(async () => {
        const data = await currentScenarioData(driver)
        return data !== null
    }, {message: 'scenario state after form input'})
}

async function setNamedFormValue(driver, label, value) {
    const result = await driver.executeScript(`
        const expected = arguments[0].toLowerCase();
        const labels = Array.from(document.querySelectorAll('.tabContent label'));
        const label = labels.find(item => item.textContent.trim().toLowerCase().startsWith(expected));
        if (!label) return 'label missing';
        const container = label.closest('.n-form-item') || label.closest('.row') || label.parentElement;
        const control = container && container.querySelector('input:not([type="checkbox"]), textarea, select');
        if (!control) return 'control missing';
        const prototype = control instanceof HTMLSelectElement ? HTMLSelectElement.prototype :
            control instanceof HTMLTextAreaElement ? HTMLTextAreaElement.prototype : HTMLInputElement.prototype;
        Object.getOwnPropertyDescriptor(prototype, 'value').set.call(control, String(arguments[1]));
        control.dispatchEvent(new Event(control instanceof HTMLSelectElement ? 'change' : 'input', {bubbles: true}));
        control.dispatchEvent(new Event('change', {bubbles: true}));
        return control.value;
    `, [label, value])
    assert.equal(result, String(value), `Could not update form field ${label}`)
}

async function selectNaiveOption(driver, label, option) {
    const markSelector = async () => driver.executeScript(`
        const expected = arguments[0].toLowerCase();
        const labels = Array.from(document.querySelectorAll('.tabContent label'));
        const label = labels.find(item => item.textContent.trim().toLowerCase().startsWith(expected));
        const container = label && (label.closest('.n-form-item') || label.closest('.row') || label.parentElement);
        const select = container && container.querySelector('.n-select');
        if (!select) return false;
        const target = select.querySelector('input') || select;
        target.setAttribute('data-e2e-select-target', 'true');
        return true;
    `, [label])
    assert.ok(await markSelector(), `Could not open the ${label} selector`)
    const chooseVisibleOption = async () => driver.executeScript(`
        const option = Array.from(document.querySelectorAll('.n-base-select-option'))
            .find(item => item.offsetParent !== null && item.textContent.trim() === arguments[0]);
        if (!option) return false;
        option.click();
        document.querySelector('[data-e2e-select-target="true"]')
            ?.removeAttribute('data-e2e-select-target');
        return true;
    `, [option])
    await waitUntil(async () => {
        if (await chooseVisibleOption()) return true
        if ((await driver.findElements('[data-e2e-select-target="true"]')).length === 0) {
            if (!await markSelector()) return false
        }
        const target = await driver.findElement('[data-e2e-select-target="true"]')
        await driver.click(target)
        return false
    }, {message: `${option} dropdown option to render`})
    if (option === 'Release') {
        await waitUntil(async () =>
            (await currentScenarioData(driver))?.hv?.BUILD_TYPE?.toLowerCase() === 'release', {
            message: 'Release build type to be applied'
        })
    }
}

async function setDialogResponses(driver, responses) {
    await driver.executeScript('window.__e2eDialogResponses = arguments[0]', [responses])
}

async function clickWidgetTool(driver, selector, tool = 1, occurrence = 0) {
    await waitUntil(async () => {
        const roots = await driver.findElements(selector)
        if (roots.length <= occurrence) return false
        const controls = await driver.findElementsWithin(roots[occurrence], ':scope > .ToolSet > div')
        if (controls.length <= tool || !await driver.displayed(controls[tool])) return false
        await driver.click(controls[tool])
        return true
    }, {message: `${selector} tool ${tool} to be clickable`})
}

async function setFirstAvailableOption(driver, selector) {
    const value = await driver.executeScript(`
        const controls = document.querySelectorAll(arguments[0]);
        const control = controls[controls.length - 1];
        if (!control) return null;
        const option = Array.from(control.options).find(item => item.value !== '');
        if (!option) return null;
        Object.getOwnPropertyDescriptor(HTMLSelectElement.prototype, 'value').set.call(control, option.value);
        control.dispatchEvent(new Event('change', {bubbles: true}));
        return option.value;
    `, [selector])
    assert.ok(value !== null, `No available option for ${selector}`)
    return value
}

async function clickToolInSet(driver, selector, tool = 1, occurrence = 0) {
    await waitUntil(async () => {
        const sets = await driver.findElements(selector)
        if (sets.length <= occurrence) return false
        const controls = await driver.findElementsWithin(sets[occurrence], ':scope > div')
        if (controls.length <= tool || !await driver.displayed(controls[tool])) return false
        await driver.click(controls[tool])
        return true
    }, {message: `${selector} tool ${tool} to be clickable`})
}

async function clickAddControlByTitle(driver, title) {
    const marked = await driver.executeScript(`
        const titleElement = Array.from(document.querySelectorAll('.tabContent b'))
            .find(item => item.textContent.trim() === arguments[0]);
        if (!titleElement) return false;
        let container = titleElement.parentElement;
        let tool = container.querySelector('.ToolSet > div');
        if (!tool && container.parentElement) {
            container = container.parentElement;
            tool = container.querySelector('.ToolSet > div');
        }
        if (!tool) return false;
        tool.setAttribute('data-e2e-add-control', 'true');
        return true;
    `, [title])
    assert.ok(marked, `Could not locate the ${title} add control`)
    await driver.click(await driver.findElement('[data-e2e-add-control="true"]'))
    await driver.executeScript(`
        document.querySelector('[data-e2e-add-control="true"]')?.removeAttribute('data-e2e-add-control');
    `)
}

async function clickIVSHMEMMinusForVM(driver, name) {
    const marked = await driver.executeScript(`
        const rows = Array.from(document.querySelectorAll('.IVSH_REGION_CONTENT .row'));
        const row = rows.find(item => item.querySelector('select')?.value === arguments[0]);
        const minus = row && row.querySelector('.ToolSet > div');
        if (!minus) return false;
        minus.setAttribute('data-e2e-ivshmem-minus', 'true');
        return true;
    `, [name])
    assert.ok(marked, `Could not locate IVSHMEM VM ${JSON.stringify(name)}`)
    await driver.click(await driver.findElement('[data-e2e-ivshmem-minus="true"]'))
    await driver.executeScript(`
        document.querySelector('[data-e2e-ivshmem-minus="true"]')
            ?.removeAttribute('data-e2e-ivshmem-minus');
    `)
}

async function clickCheckboxByText(driver, text) {
    const marked = await driver.executeScript(`
        const elements = Array.from(document.querySelectorAll('.tabContent label, .tabContent .form-check'));
        const element = elements.find(item => item.textContent.includes(arguments[0]));
        const container = element && (element.closest('.form-check') || element.closest('.row') || element.parentElement);
        const input = container && container.querySelector('input[type="checkbox"]');
        if (!input) return false;
        input.setAttribute('data-e2e-checkbox', 'true');
        return true;
    `, [text])
    assert.ok(marked, `Could not find checkbox ${text}`)
    const checkbox = await driver.findElement('[data-e2e-checkbox="true"]')
    await driver.click(checkbox)
    await driver.executeScript(`
        document.querySelector('[data-e2e-checkbox="true"]')?.removeAttribute('data-e2e-checkbox');
    `)
}

async function checkboxCheckedByText(driver, text) {
    const checked = await driver.executeScript(`
        const elements = Array.from(document.querySelectorAll('.tabContent label, .tabContent .form-check'));
        const element = elements.find(item => item.textContent.includes(arguments[0]));
        const container = element && (element.closest('.form-check') || element.closest('.row') || element.parentElement);
        const input = container && container.querySelector('input[type="checkbox"]');
        if (!input) throw new Error('Could not find checkbox ' + arguments[0]);
        return input.checked;
    `, [text])
    assert.equal(typeof checked, 'boolean')
    return checked
}

async function closeVisibleModal(driver, action = '.btn-primary') {
    const modal = await driver.findElement('.modal.show')
    const id = await driver.attribute(modal, 'id')
    await driver.click(await driver.findElementWithin(modal, `.modal-footer ${action}`))
    await waitUntil(async () =>
        (await driver.findElements(id ? `#${id}.show` : '.modal.show')).length === 0, {
        timeout: 10000,
        message: `${id || 'modal'} to close`
    })
}

async function discoverBundledFixtures() {
    const dataRoot = path.join(repositoryRoot, 'misc', 'config_tools', 'data')
    const directories = await fs.readdir(dataRoot, {withFileTypes: true})
    const fixtures = []
    for (const directory of directories.filter(entry => entry.isDirectory())) {
        const fixtureDirectory = path.join(dataRoot, directory.name)
        const files = (await fs.readdir(fixtureDirectory))
            .filter(file => file.endsWith('.xml')).sort()
        const boardFile = `${directory.name}.xml`
        assert.ok(files.includes(boardFile), `Missing bundled Board XML: ${directory.name}/${boardFile}`)
        for (const scenarioFile of files.filter(file => file !== boardFile)) {
            fixtures.push({
                board: directory.name,
                scenario: path.basename(scenarioFile, '.xml'),
                boardPath: path.join(fixtureDirectory, boardFile),
                scenarioPath: path.join(fixtureDirectory, scenarioFile)
            })
        }
    }
    assert.ok(fixtures.length > 0, 'No bundled Board/Scenario fixtures were discovered')
    return fixtures
}

async function readBundledXML(filePath) {
    const contents = await fs.readFile(filePath, 'utf8')
    const linkTarget = contents.trim()
    if (!linkTarget.startsWith('<') && /^[a-zA-Z0-9_.-]+\.xml$/.test(linkTarget)) {
        const targetPath = path.join(path.dirname(filePath), linkTarget)
        const targetContents = await fs.readFile(targetPath, 'utf8').catch(() => null)
        if (targetContents?.trimStart().startsWith('<')) {
            return targetContents
        }
    }
    return contents
}

let stagedWorkingFolder = null

async function stageFixture(temporaryRoot, historyDirectory, fixture, transformScenario = text => text) {
    const caseName = `${fixture.board}-${fixture.scenario}`.replace(/[^a-zA-Z0-9_.-]/g, '_')
    const workingFolder = path.join(temporaryRoot, 'cases', caseName) + path.sep
    const boardPath = path.join(workingFolder, `${fixture.board}.board.xml`)
    const scenarioPath = path.join(workingFolder, 'scenario.xml')
    await fs.mkdir(workingFolder, {recursive: true})
    await fs.writeFile(boardPath, await readBundledXML(fixture.boardPath))
    const scenarioXML = transformScenario(await readBundledXML(fixture.scenarioPath))
    await fs.writeFile(scenarioPath, scenarioXML)
    stagedWorkingFolder = workingFolder
    await fs.writeFile(path.join(historyDirectory, 'config.json'), JSON.stringify({
        history: {
            working_folder: [workingFolder],
            board_file: [boardPath],
            scenario_file: [scenarioPath]
        }
    }))
    return workingFolder
}

async function currentScenarioData(driver) {
    return driver.executeScript(`
        const pinia = document.querySelector('#app')?.__vue_app__?.config?.globalProperties?.$pinia;
        const scenario = pinia?.state?.value?.scenario?.doc;
        return scenario && scenario.hv ? JSON.parse(JSON.stringify(scenario)) : null;
    `)
}

function assertScenarioDataPreserved(actual, expected, path = 'scenario') {
    assert.ok(actual !== null && actual !== undefined,
        `${path} is missing after switching parameter modes`)
    if (Array.isArray(expected)) {
        assert.ok(Array.isArray(actual), `${path} changed from an array`)
        assert.ok(actual.length >= expected.length, `${path} lost array entries`)
        expected.forEach((value, index) =>
            assertScenarioDataPreserved(actual[index], value, `${path}[${index}]`))
        return
    }
    if (expected && typeof expected === 'object') {
        assert.equal(typeof actual, 'object', `${path} changed from an object`)
        for (const [key, value] of Object.entries(expected)) {
            assert.ok(Object.prototype.hasOwnProperty.call(actual, key), `${path}.${key} was removed`)
            assertScenarioDataPreserved(actual[key], value, `${path}.${key}`)
        }
        return
    }
    if (['string', 'number'].includes(typeof actual) &&
        ['string', 'number'].includes(typeof expected)) {
        // XML text has no numeric type; schema processing may normalize "0" to 0.
        assert.equal(String(actual), String(expected), `${path} changed`)
        return
    }
    assert.deepEqual(actual, expected, `${path} changed`)
}

async function diagnosticMessages(driver) {
    return driver.executeScript('return (window.__e2eDiagnostics || []).slice(-30)')
}

async function captureCoverage(driver) {
    if (process.env.E2E_COVERAGE !== 'true') {
        return
    }
    const coverage = await driver.executeScript('return window.__coverage__ || null')
    assert.ok(coverage && Object.keys(coverage).length > 0,
        'Coverage was requested but the application was not instrumented')
    const coverageDirectory = path.join(artifacts, 'coverage')
    const reportDirectory = path.join(artifacts, 'coverage-report')
    await fs.mkdir(coverageDirectory, {recursive: true})
    await fs.writeFile(path.join(coverageDirectory, 'coverage.json'), JSON.stringify(coverage))
    const nyc = path.join(packageRoot, '..', '..', 'node_modules', 'nyc', 'bin', 'nyc.js')
    const report = spawnSync(process.execPath, [
        nyc, 'report',
        '--temp-dir', coverageDirectory,
        '--report-dir', reportDirectory,
        '--reporter', 'text',
        '--reporter', 'html',
        '--reporter', 'json-summary'
    ], {cwd: packageRoot, stdio: 'inherit'})
    assert.equal(report.status, 0, 'Failed to generate the E2E coverage report')
    const minimum = Number(process.env.E2E_COVERAGE_MIN || 0)
    if (minimum > 0) {
        const check = spawnSync(process.execPath, [
            nyc, 'check-coverage',
            '--temp-dir', coverageDirectory,
            '--statements', String(minimum),
            '--branches', String(minimum),
            '--functions', String(minimum),
            '--lines', String(minimum)
        ], {cwd: packageRoot, stdio: 'inherit'})
        assert.equal(check.status, 0, `E2E coverage is below ${minimum}%`)
    }
}

async function resetHistory(driver) {
    await driver.executeScript('return window.__TAURI_INTERNALS__.invoke("force_reset")')
}

async function backToWelcome(driver) {
    if (await driver.alertText() !== null) {
        await driver.acceptAlert()
    }
    if ((await driver.bodyText()).includes('Open Folder')) {
        return
    }
    if ((await driver.findElements('.modal.show')).length > 0) {
        await driver.executeScript(`
            const button = document.querySelector('.modal.show .modal-footer .btn-primary');
            if (button) button.click();
        `)
    }
    await waitUntil(async () =>
        (await driver.findElements('.modal.show')).length === 0 &&
        (await driver.findElements('.modal-backdrop')).length === 0, {
        timeout: 5000,
        message: 'the save result modal to finish closing'
    })
    await driver.executeScript("window.location.hash = '/'")
    await waitUntil(async () => (await driver.bodyText()).includes('Open Folder'), {
        timeout: 30000,
        message: 'the Welcome route after closing the working folder'
    })
}

async function openWorkingFolder(driver) {
    if (stagedWorkingFolder) {
        const selected = await waitUntil(async () => await driver.executeScript(`
            const select = document.querySelector('.form-select');
            return select?.value === arguments[0];
        `, [stagedWorkingFolder]), {
            timeout: 2000,
            message: `the working-folder selector to contain ${stagedWorkingFolder}`
        }).catch(() => false)
        if (!selected) {
            const previousResponses = await driver.executeScript(
                'return window.__e2eDialogResponses ? {...window.__e2eDialogResponses} : null')
            await mockNativeDialogs(driver, {open: stagedWorkingFolder.slice(0, -1), confirm: true})
            try {
                const form = await welcomeForm(driver, 'Use an existing configuration')
                await clickByTextWithin(driver, form, 'a', 'Browse for folder')
                await waitUntil(async () => {
                    const select = await driver.findElementWithin(form, 'select')
                    return await driver.property(select, 'value') === stagedWorkingFolder
                }, {message: 'the staged working folder to be browsed into history'})
            } finally {
                if (previousResponses) {
                    await mockNativeDialogs(driver, previousResponses)
                } else {
                    await restoreNativeDialogs(driver)
                }
            }
        }
    }
    await clickByText(driver, 'button', 'Open Folder')
    await waitUntil(async () => {
        const body = await driver.bodyText()
        return body.includes('Current scenario: scenario.xml') &&
            (!stagedWorkingFolder || body.includes(`Working folder: ${stagedWorkingFolder}`))
    }, {
        timeout: 60000,
        message: 'Board and Scenario to load in the selected native working folder'
    })
    await waitUntil(async () => await currentScenarioData(driver) !== null, {
        timeout: 30000,
        message: 'normalized scenario data to become available'
    })
}

async function saveScenario(driver, timeout = 120000, clicks = 1) {
    await driver.executeScript(`
        const button = Array.from(document.querySelectorAll('button'))
            .find(item => item.textContent.includes('Save Scenario and Launch Scripts'));
        button?.scrollIntoView({block: 'center'});
    `)
    try {
        await waitUntil(async () => {
            if (await driver.alertText() !== null ||
                (await driver.findElements('.modal.show')).length > 0) return true
            const body = await driver.bodyText()
            if (body.includes('All files successfully saved') || body.includes('failed') ||
                body.includes('Please check your configuration')) return true
            const buttons = await driver.findElements('button')
            for (const button of buttons) {
                if (!(await driver.text(button)).toLowerCase().includes('save scenario and launch scripts') ||
                    !await driver.displayed(button)) continue
                if (await driver.property(button, 'disabled')) return true
                await driver.executeScript(`
                    const saveButton = Array.from(document.querySelectorAll('button'))
                        .find(item => item.textContent.includes('Save Scenario and Launch Scripts'));
                    for (let index = 0; index < arguments[0]; index++) saveButton?.click();
                `, [clicks])
                return false
            }
            return false
        }, {timeout: 30000, message: 'scenario save to start'})
    } catch (error) {
        if (await driver.alertText() === null) {
            const state = await driver.executeScript(`
                return {
                    scenario: document.querySelector('#app')?.__vue_app__?.config?.globalProperties?.$pinia?.state?.value?.scenario?.doc,
                    buttons: Array.from(document.querySelectorAll('button'))
                        .filter(button => button.textContent.includes('Save Scenario'))
                        .map(button => ({disabled: button.disabled, text: button.innerText})),
                    modals: Array.from(document.querySelectorAll('.modal'))
                        .map(modal => ({classes: modal.className, text: modal.innerText}))
                };
            `).catch(() => null)
            throw new Error(`${error.message}: ${JSON.stringify(state)}`)
        }
    }
    try {
        return await waitUntil(async () => {
            const alert = await driver.alertText()
            if (alert !== null) {
                return {alert}
            }
            const body = await driver.bodyText()
            if (body.includes('All files successfully saved') || body.includes('failed') ||
                body.includes('Please check your configuration')) {
                const validation = await driver.executeScript(`
                    return document.querySelector('#app')?.__vue_app__?.config?.globalProperties?.$pinia
                        ?.state?.value?.validation?.errors;
                `)
                return {body, validation}
            }
            return false
        }, {timeout, message: 'scenario save result'})
    } catch (error) {
        const state = await driver.executeScript(`
            return {
                buttons: Array.from(document.querySelectorAll('button'))
                    .filter(button => button.textContent.includes('Save Scenario'))
                    .map(button => ({disabled: button.disabled, display: getComputedStyle(button).display})),
                modals: Array.from(document.querySelectorAll('.modal')).map(modal => ({
                    id: modal.id, classes: modal.className, text: modal.innerText
                }))
            };
        `).catch(() => null)
        throw new Error(`${error.message}: ${JSON.stringify(state)}`)
    }
}

async function mockNativeDialogs(driver, {open = null, confirm = true} = {}) {
    await driver.executeScript(`
        const responses = arguments[0];
        if (!window.__e2eOriginalFetch) {
            window.__e2eOriginalFetch = window.fetch;
        }
        window.__e2eDialogResponses = responses;
        window.__e2eAlerts = [];
        window.__e2eConfirmMessages = [];
        window.__e2eDialogTitles = [];
        if (!window.__e2eOriginalDialogFunctions) {
            window.__e2eOriginalDialogFunctions = {
                alert: window.alert,
                confirm: window.confirm
            };
        }
        window.alert = message => {
            window.__e2eAlerts.push(String(message));
        };
        window.confirm = message => {
            window.__e2eConfirmMessages.push(String(message));
            return Promise.resolve(window.__e2eDialogResponses.confirm);
        };
        window.fetch = (input, init) => {
            const url = typeof input === 'string' ? input : input.url;
            if (!url.startsWith('ipc://localhost/')) {
                return window.__e2eOriginalFetch(input, init);
            }
            const command = decodeURIComponent(url.slice('ipc://localhost/'.length));
            const current = window.__e2eDialogResponses;
            let result;
            if (command === 'plugin:dialog|open') {
                const args = JSON.parse(init.body || '{}');
                window.__e2eDialogTitles.push(args?.options?.title || '');
                result = current.open;
            } else if (command === 'plugin:dialog|confirm') {
                result = current.confirm;
            } else if (command === 'plugin:dialog|message') {
                const args = JSON.parse(init.body || '{}');
                window.__e2eConfirmMessages.push(String(args?.message || ''));
                result = current.confirm ? 'Ok' : 'Cancel';
            } else {
                return window.__e2eOriginalFetch(input, init);
            }
            return Promise.resolve(new Response(JSON.stringify(result), {
                status: 200,
                headers: {
                    'Content-Type': 'application/json',
                    'Tauri-Response': 'ok'
                }
            }));
        };
    `, [{open, confirm}])
    const dialogMock = await driver.executeScript(`
        return window.fetch.toString().includes('__e2eOriginalFetch');
    `)
    assert.ok(dialogMock, 'Native dialog transport mock was not installed')
}

async function restoreNativeDialogs(driver) {
    await driver.executeScript(`
        if (window.__e2eOriginalFetch) {
            window.fetch = window.__e2eOriginalFetch;
        }
        if (window.__e2eOriginalDialogFunctions) {
            window.alert = window.__e2eOriginalDialogFunctions.alert;
            window.confirm = window.__e2eOriginalDialogFunctions.confirm;
        }
        delete window.__e2eDialogResponses;
    `)
}

const bundledFixtures = await discoverBundledFixtures()

describe('Configurator native WebDriver workflow', {concurrency: false}, () => {
    let driver
    let driverProcess
    let driverLog
    let driverStartupError
    let temporaryRoot
    let workingFolder
    let historyDirectory
    let preserveTemporaryRoot = false

    before(async () => {
        assert.equal(process.platform, 'linux', 'This WebKitGTK E2E suite must run on Linux')
        await fs.access(application)
        await fs.mkdir(artifacts, {recursive: true})
        temporaryRoot = await fs.mkdtemp(path.join(os.tmpdir(), 'acrn-configurator-e2e-'))
        workingFolder = path.join(temporaryRoot, 'work') + path.sep
        const home = path.join(temporaryRoot, 'home')
        const configHome = path.join(temporaryRoot, 'config')
        historyDirectory = path.join(configHome, '.acrn-configurator')
        await fs.mkdir(workingFolder, {recursive: true})
        await fs.mkdir(home, {recursive: true})
        await fs.mkdir(historyDirectory, {recursive: true})

        const fixtureDirectory = path.join(repositoryRoot, 'misc', 'config_tools', 'data', 'nuc11tnbi5')
        const boardPath = path.join(workingFolder, 'nuc11tnbi5.board.xml')
        const scenarioPath = path.join(workingFolder, 'scenario.xml')
        await fs.copyFile(path.join(fixtureDirectory, 'nuc11tnbi5.xml'), boardPath)
        await fs.copyFile(path.join(fixtureDirectory, 'hybrid_launch_2user_vm.xml'), scenarioPath)
        await fs.writeFile(path.join(workingFolder, 'keep.sh'), 'user-owned script\n')
        await fs.writeFile(path.join(historyDirectory, 'config.json'), JSON.stringify({
            history: {
                working_folder: [workingFolder],
                board_file: [boardPath],
                scenario_file: [scenarioPath]
            }
        }))

        const port = await freePort()
        const nativePort = await freePort()
        driverLog = createWriteStream(path.join(artifacts, 'tauri-driver.log'))
        driverProcess = spawn(process.env.TAURI_DRIVER || 'tauri-driver', [
            '--port', String(port),
            '--native-port', String(nativePort),
            '--native-driver', process.env.WEBKIT_WEBDRIVER || 'WebKitWebDriver'
        ], {
            env: {
                ...process.env,
                HOME: home,
                XDG_CONFIG_HOME: configHome,
                DISPLAY: process.env.DISPLAY || ':0',
                WAYLAND_DISPLAY: process.env.WAYLAND_DISPLAY || 'wayland-0'
            },
            stdio: ['ignore', 'pipe', 'pipe']
        })
        driverProcess.stdout.pipe(driverLog)
        driverProcess.stderr.pipe(driverLog)
        driverProcess.once('error', error => {
            driverStartupError = error
            driverLog.write(`Failed to start tauri-driver: ${error.stack}\n`)
        })
        driverProcess.once('exit', (code, signal) => {
            if (!driver) {
                driverStartupError = new Error(`tauri-driver exited with code ${code}, signal ${signal}`)
            }
        })

        await waitUntil(async () => {
            if (driverStartupError) {
                throw driverStartupError
            }
            try {
                const response = await fetch(`http://127.0.0.1:${port}/status`, {
                    signal: AbortSignal.timeout(1000)
                })
                return response.ok
            } catch {
                return false
            }
        }, {timeout: 15000, message: 'tauri-driver to listen'})

        driver = await WebDriverClient.create({port, application})
        await fs.writeFile(path.join(artifacts, 'webdriver-session.json'), JSON.stringify({
            port, nativePort, sessionID: driver.sessionID
        }))
        await driver.executeScript(`
            window.__e2eDiagnostics = [];
            for (const method of ['error', 'warn']) {
                const original = console[method];
                console[method] = (...args) => {
                    window.__e2eDiagnostics.push(method + ': ' + args.map(arg =>
                        arg instanceof Error ? arg.stack : String(arg)).join(' '));
                    original.apply(console, args);
                };
            }
            window.addEventListener('error', event => {
                window.__e2eDiagnostics.push('error: ' + (event.error?.stack || event.message));
            });
            window.addEventListener('unhandledrejection', event => {
                window.__e2eDiagnostics.push('unhandledrejection: ' + String(event.reason));
            });
        `)
        try {
            await waitUntil(async () => (await driver.bodyText()).includes('Open Folder'), {
                timeout: 120000,
                message: 'Pyodide to initialize and the Welcome page to render'
            })
            await clickByText(driver, 'button', 'Open Folder')
            await waitUntil(async () => (await driver.bodyText()).includes('Current scenario: scenario.xml'), {
                timeout: 120000,
                message: 'Board and Scenario to load in the native application'
            })
            await waitUntil(async () => (await driver.bodyText()).includes('ACRN_Service_VM'), {
                timeout: 30000,
                message: 'VM tabs to render'
            })
            await driver.executeScript(`
                const app = document.querySelector('#app').__vue_app__;
                app.config.errorHandler = (error, instance, info) => {
                    const components = [];
                    for (let current = instance; current; current = current.parent) {
                        components.push(current.type.name || current.type.__name || 'Anonymous');
                    }
                    window.__e2eDiagnostics.push('Vue ' + info + ': ' + components.join(' <- '));
                    console.error(error);
                };
            `)
        } catch (error) {
            preserveTemporaryRoot = true
            await driver.screenshot(path.join(artifacts, 'startup-regression.png')).catch(() => {})
            console.error(`Native application startup body: ${await driver.bodyText().catch(() => '<unavailable>')}`)
            console.error(`Native diagnostics: ${JSON.stringify(await diagnosticMessages(driver))}`)
            throw error
        }
    })

    after(async () => {
        if (driver) {
            try {
                await captureCoverage(driver)
            } finally {
                await driver.close().catch(() => {})
            }
        }
        if (driverProcess && !driverProcess.killed) {
            driverProcess.kill('SIGTERM')
        }
        if (driverLog) {
            driverLog.end()
        }
        if (temporaryRoot && !preserveTemporaryRoot) {
            try {
                await fs.rm(temporaryRoot, {
                    recursive: true, force: true, maxRetries: 10, retryDelay: 200
                })
            } catch (error) {
                console.log(`Preserved E2E working directory after cleanup failed: ${temporaryRoot}`)
                console.log(`Cleanup error: ${error.message}`)
            }
        } else if (temporaryRoot) {
            console.log(`Preserved E2E working directory: ${temporaryRoot}`)
        }
    })

    test('switches from Advanced Parameters back to Basic Parameters', async () => {
        try {
            const basic = await driver.findElement('.bTab')
            const advanced = await driver.findElement('.aTab')
            const content = await driver.findElement('.tabContent')
            const basicText = await driver.text(content)
            const basicScenario = await currentScenarioData(driver)
            assert.ok(basicScenario, 'Initial scenario data is unavailable')

            await driver.click(advanced)
            await waitUntil(async () => (await driver.attribute(advanced, 'class')).includes('Active'), {
                timeout: 5000,
                message: 'Advanced Parameters to become active'
            })
            await waitUntil(async () => (await driver.text(content)).includes('Hypervisor capacities'), {
                timeout: 5000,
                message: 'Advanced-only hypervisor fields to render'
            })
            const advancedText = await driver.text(content)
            assert.notEqual(advancedText, basicText, 'The Advanced form was not rendered')

            await driver.click(basic)
            await waitUntil(async () => (await driver.attribute(basic, 'class')).includes('Active'), {
                timeout: 5000,
                message: 'Basic Parameters to become active again'
            })
            assert.equal(await driver.text(content), basicText, 'The Basic form was not restored')
            assertScenarioDataPreserved(await currentScenarioData(driver), basicScenario)
        } catch (error) {
            preserveTemporaryRoot = true
            console.error(`Parameter tab diagnostics: ${JSON.stringify(await diagnosticMessages(driver))}`)
            await driver.screenshot(path.join(artifacts, 'advanced-basic-regression.png'))
            throw error
        }
    })

    test('visits every VM in both parameter modes without changing its identity', async () => {
        const scenario = await currentScenarioData(driver)
        for (const vm of scenario.vm) {
            await selectVMTab(driver, vm.name)
            const advanced = await driver.findElement('.aTab')
            await driver.click(advanced)
            await waitUntil(async () => (await driver.attribute(advanced, 'class')).includes('Active'), {
                message: `${vm.name} Advanced Parameters`
            })
            const basic = await driver.findElement('.bTab')
            await driver.click(basic)
            await waitUntil(async () => (await driver.attribute(basic, 'class')).includes('Active'), {
                message: `${vm.name} Basic Parameters`
            })
            const current = await currentScenarioData(driver)
            assert.ok(current.vm.some(item => item['@id'] === vm['@id'] && item.name === vm.name))
        }
        await selectVMTab(driver, 'Hypervisor')
    })

    test('saves launch artifacts and reloads the scenario', async () => {
        try {
            const result = await saveScenario(driver, 120000, 2)
            assert.ok(result.body?.includes('All files successfully saved'), JSON.stringify(result))

            const scenarioXML = await fs.readFile(path.join(workingFolder, 'scenario.xml'), 'utf8')
            assert.match(scenarioXML, /<acrn-config>/)
            assert.match(scenarioXML, /<name>ACRN_Service_VM<\/name>/)
            assert.match(scenarioXML, /<name>POST_STD_VM1<\/name>/)
            assert.match(scenarioXML, /<name>POST_STD_VM2<\/name>/)
            const summary = await fs.readFile(path.join(workingFolder, 'config_summary.rst'), 'utf8')
            assert.ok(summary.length > 0, 'config_summary.rst is empty')
            const files = await fs.readdir(workingFolder)
            const launchScripts = files.filter(file => /^launch.*\.sh$/.test(file))
            assert.equal(launchScripts.length, 2, `Unexpected launch scripts: ${launchScripts.join(', ')}`)
            assert.equal(await fs.readFile(path.join(workingFolder, 'keep.sh'), 'utf8'), 'user-owned script\n')

            await closeVisibleModal(driver)
            await backToWelcome(driver)
            await openWorkingFolder(driver)
            assert.match(await driver.bodyText(), /ACRN_Service_VM/)
            await driver.screenshot(path.join(artifacts, 'native-workflow.png'))
        } catch (error) {
            preserveTemporaryRoot = true
            console.error(`Save modal diagnostic: ${JSON.stringify(await driver.executeScript(`
                const modal = document.querySelector('.modal');
                return {
                    href: window.location.href,
                    body: document.body.innerText.slice(0, 500),
                    modalClass: modal && modal.className,
                    backdropCount: document.querySelectorAll('.modal-backdrop').length
                };
            `))}`)
            console.error(`Native diagnostics: ${JSON.stringify(await diagnosticMessages(driver))}`)
            await driver.screenshot(path.join(artifacts, 'save-reload-regression.png'))
            throw error
        }
    })

    test('maximizes and restores the native window', async () => {
        await backToWelcome(driver)
        const controls = await driver.findElements('.controlButtons > *')
        assert.equal(controls.length, 4)
        await driver.click(controls[2])
        await waitUntil(async () => (await driver.bodyText()).includes('Open Folder'), {
            message: 'the application to remain responsive after changing window size'
        })
        await driver.click(controls[2])
        await waitUntil(async () => (await driver.bodyText()).includes('Open Folder'), {
            message: 'the application to remain responsive after restoring window size'
        })
    })

    test('creates a new working folder, imports a Board, and saves a new Scenario', async () => {
        const source = bundledFixtures.find(fixture =>
            fixture.board === 'nuc11tnbi5' && fixture.scenario === 'shared')
        assert.ok(source, 'The new-configuration fixture is missing')
        const newFolder = path.join(temporaryRoot, 'new-configuration')
        await fs.mkdir(newFolder, {recursive: true})
        await fs.writeFile(path.join(newFolder, 'scenario.xml'), 'old scenario')
        await fs.writeFile(path.join(newFolder, 'old.board.xml'), 'old board')
        await fs.writeFile(path.join(newFolder, 'launch_old.sh'), 'old launch')
        await fs.writeFile(path.join(newFolder, 'keep.txt'), 'user data\n')
        await resetHistory(driver)
        await backToWelcome(driver)
        await mockNativeDialogs(driver, {open: newFolder, confirm: false})
        try {
            let form = await welcomeForm(driver, 'Start a new configuration')
            await clickByTextWithin(driver, form, 'a', 'Browse for folder')
            const input = await driver.findElementWithin(form, 'input[type="text"]')
            await waitUntil(async () =>
                await driver.property(input, 'value') === newFolder + path.sep, {
                message: 'the browsed new working folder to be selected'
            })
            await clickByTextWithin(driver, form, 'button', 'Use this Folder')
            await waitUntil(async () => await driver.executeScript(
                'return window.__e2eConfirmMessages.length'), {message: 'new-folder confirmation'})
            assert.ok((await driver.bodyText()).includes('Start a new configuration'))
            assert.equal(await fs.readFile(path.join(newFolder, 'scenario.xml'), 'utf8'), 'old scenario')

            await setDialogResponses(driver, {open: newFolder, confirm: true})
            await clickByTextWithin(driver, form, 'button', 'Use this Folder')
            await waitUntil(async () =>
                (await driver.bodyText()).includes('No board information has been imported yet.'), {
                timeout: 60000,
                message: 'the new configuration route'
            })
            assert.equal(await driver.executeScript('return window.__e2eConfirmMessages[0]'),
                `Warning: delete previous config files in the following working folder: \n${newFolder + path.sep}?`)
            for (const removed of ['scenario.xml', 'old.board.xml', 'launch_old.sh']) {
                await assert.rejects(fs.access(path.join(newFolder, removed)), `${removed} was not cleaned`)
            }
            assert.equal(await fs.readFile(path.join(newFolder, 'keep.txt'), 'utf8'), 'user data\n')

            await clickByText(driver, 'button', 'Import Board File')
            assert.ok((await driver.bodyText()).includes('No board information has been imported yet.'))
            assert.equal(await currentScenarioData(driver), null)

            await setDialogResponses(driver, {open: source.boardPath, confirm: true})
            await clickByText(driver, 'a', 'Browse for file')
            await waitUntil(async () => await driver.executeScript(`
                return Array.from(document.querySelectorAll('select.form-select'))
                    .some(select => select.value === arguments[0]);
            `, [source.boardPath]), {message: 'the browsed Board to be selected'})
            await clickByText(driver, 'button', 'Import Board File')
            await waitUntil(async () =>
                (await driver.bodyText()).includes(`Current Board: ${newFolder}${path.sep}nuc11tnbi5.board.xml`), {
                timeout: 60000,
                message: 'the browsed Board to import'
            })
            await fs.access(source.boardPath)

            await clickByText(driver, 'button', 'Import Scenario')
            assert.equal(await currentScenarioData(driver), null,
                'An empty Scenario selection must not replace the current data')

            await clickByText(driver, 'button', 'Create Scenario')
            await waitUntil(async () =>
                (await driver.findElements('#newScenarioModal.show')).length === 1, {
                message: 'the new Scenario dialog'
            })
            await clickByText(driver, '#newScenarioModal label', 'Shared')
            const postLaunchInput = await driver.findElement('#newScenarioModal .vmNum input:last-of-type')
            await driver.clear(postLaunchInput)
            await driver.type(postLaunchInput, '2')
            await driver.clear(postLaunchInput)
            await driver.type(postLaunchInput, '1')
            await closeVisibleModal(driver)
            await waitUntil(async () => {
                const scenario = await currentScenarioData(driver)
                return scenario?.vm?.length === 2
            }, {message: 'the default shared Scenario to be created'})
            await selectVMTab(driver, 'Hypervisor')
            const invalidResult = await saveScenario(driver, 10000)
            assert.ok(invalidResult.body?.includes('Settings validate failed'), invalidResult.body)
            assert.ok(invalidResult.body?.includes('Scenario xml saved'), invalidResult.body)
            assert.ok(!invalidResult.body?.includes('Scenario xml save failed'), invalidResult.body)
            await closeVisibleModal(driver)
            const invalidScenario = await fs.readFile(path.join(newFolder, 'scenario.xml'), 'utf8')
            assert.match(invalidScenario, /<acrn-config/)
            await assert.rejects(fs.access(path.join(newFolder, 'config_summary.rst')),
                'An invalid Scenario must not generate a configuration summary')
            assert.equal((await fs.readdir(newFolder)).filter(file => /^launch.*\.sh$/.test(file)).length, 0,
                'An invalid Scenario must not generate launch scripts')
            await selectNaiveOption(driver, 'Build type', 'Release')
            await selectVMTab(driver, 'VM1')
            await setNamedFormValue(driver, 'VM name', 'E2E_POST_VM')
            await setFormControlValue(driver, '.tabContent select.form-select', 0)
            const result = await saveScenario(driver)
            assert.ok(result.body?.includes('All files successfully saved'), JSON.stringify(result))
            const scenarioXML = await fs.readFile(path.join(newFolder, 'scenario.xml'), 'utf8')
            assert.match(scenarioXML, /<name>E2E_POST_VM<\/name>/)
            assert.match(scenarioXML, /<pcpu_id>0<\/pcpu_id>/)
            assert.equal((await fs.readdir(newFolder)).filter(file => /^launch.*\.sh$/.test(file)).length, 1)

            await closeVisibleModal(driver)
            await backToWelcome(driver)
            await setDialogResponses(driver, {open: newFolder, confirm: true})
            form = await welcomeForm(driver, 'Use an existing configuration')
            await clickByTextWithin(driver, form, 'a', 'Browse for folder')
            const existingFolderSelect = await driver.findElementWithin(form, 'select')
            await waitUntil(async () =>
                await driver.property(existingFolderSelect, 'value') === newFolder + path.sep, {
                message: 'the new folder to be selected for reload'
            })
            await clickByTextWithin(driver, form, 'button', 'Open Folder')
            await waitUntil(async () => await currentScenarioData(driver) !== null, {
                timeout: 60000,
                message: 'the newly created Scenario to reload'
            })
            assert.match(await driver.bodyText(), /E2E_POST_VM/)
            const reloaded = await currentScenarioData(driver)
            assert.equal(reloaded.vm.find(vm => vm.name === 'E2E_POST_VM').cpu_affinity.pcpu[0].pcpu_id, 0)
        } catch (error) {
            preserveTemporaryRoot = true
            await driver.screenshot(path.join(artifacts, 'new-configuration-regression.png')).catch(() => {})
            console.error(`New configuration diagnostics: ${JSON.stringify(await diagnosticMessages(driver))}`)
            console.error(`New configuration DOM: ${JSON.stringify(await driver.executeScript(`
                return {
                    hash: window.location.hash,
                    modals: Array.from(document.querySelectorAll('.modal')).map(modal => ({
                        id: modal.id, classes: modal.className, text: modal.innerText
                    })),
                    backdrops: document.querySelectorAll('.modal-backdrop').length,
                    activeTab: document.querySelector('.TabBox .Tab.Active')?.innerText
                };
            `).catch(error => ({diagnosticError: error.message})))}`)
            throw error
        } finally {
            await restoreNativeDialogs(driver)
        }
    })

    test('creates, saves, and reloads a Hybrid Scenario from its template', async () => {
        const source = bundledFixtures.find(fixture =>
            fixture.board === 'nuc11tnbi5' && fixture.scenario === 'shared')
        assert.ok(source, 'The Hybrid Scenario fixture is missing')
        await stageFixture(temporaryRoot, historyDirectory, source)
        await backToWelcome(driver)
        await openWorkingFolder(driver)
        await expandAccordion(driver, '2. Create new or import an existing scenario')
        await mockNativeDialogs(driver, {confirm: true})
        try {
            await clickByText(driver, 'button', 'Create Scenario')
            await waitUntil(async () =>
                (await driver.findElements('#my-modal.show')).length === 1, {
                message: 'the Hybrid Scenario overwrite dialog'
            })
            await closeVisibleModal(driver)
            await waitUntil(async () =>
                (await driver.findElements('#newScenarioModal.show')).length === 1, {
                message: 'the Hybrid Scenario template dialog'
            })
            await clickByText(driver, '#newScenarioModal label', 'Hybrid')
            const inputs = await driver.findElements('#newScenarioModal .vmNum input')
            assert.equal(inputs.length, 3, 'Hybrid must configure pre-launched, Service, and post-launched VMs')
            await setFormControlValue(driver, '#newScenarioModal .vmNum input', 1, 0)
            await setFormControlValue(driver, '#newScenarioModal .vmNum input', 1, 2)
            await closeVisibleModal(driver)
            await waitUntil(async () => {
                const scenario = await currentScenarioData(driver)
                return scenario?.vm?.length === 3
            }, {message: 'the Hybrid Scenario to contain all three VM types'})
            let scenario = await currentScenarioData(driver)
            assert.deepEqual(scenario.vm.map(vm => vm.load_order),
                ['PRE_LAUNCHED_VM', 'SERVICE_VM', 'POST_LAUNCHED_VM'])

            await selectVMTab(driver, 'Hypervisor')
            await selectNaiveOption(driver, 'Build type', 'Release')
            for (const [vmName, pcpuID] of [['VM0', 0], ['VM2', 1]]) {
                await selectVMTab(driver, vmName)
                await setFormControlValue(driver, '.cpu_affinity select', pcpuID)
            }
            const result = await saveScenario(driver)
            assert.ok(result.body?.includes('All files successfully saved'), JSON.stringify(result))
            await closeVisibleModal(driver)

            await backToWelcome(driver)
            await openWorkingFolder(driver)
            scenario = await currentScenarioData(driver)
            assert.deepEqual(scenario.vm.map(vm => vm.load_order),
                ['PRE_LAUNCHED_VM', 'SERVICE_VM', 'POST_LAUNCHED_VM'])
            assert.equal(scenario.vm[0].cpu_affinity.pcpu[0].pcpu_id, 0)
            assert.equal(scenario.vm[2].cpu_affinity.pcpu[0].pcpu_id, 1)
        } catch (error) {
            preserveTemporaryRoot = true
            await driver.screenshot(path.join(artifacts, 'hybrid-scenario-regression.png')).catch(() => {})
            throw error
        } finally {
            await restoreNativeDialogs(driver)
        }
    })

    test('creates a missing home-relative folder and reports filesystem failures', async () => {
        const missingName = `acrn-e2e-missing-${path.basename(temporaryRoot)}`
        const missingFolder = path.join(temporaryRoot, 'home', missingName)
        const regularFile = path.join(temporaryRoot, 'not-a-directory')
        const uncleanable = path.join(temporaryRoot, 'uncleanable')
        await fs.writeFile(regularFile, 'not a directory\n')
        await fs.mkdir(uncleanable, {recursive: true})
        await fs.writeFile(path.join(uncleanable, 'scenario.xml'), 'preserve on failure')
        await backToWelcome(driver)
        await mockNativeDialogs(driver, {open: null, confirm: true})
        try {
            let form = await welcomeForm(driver, 'Start a new configuration')
            let input = await driver.findElementWithin(form, 'input[type="text"]')
            const originalFolder = await driver.property(input, 'value')
            await clickByTextWithin(driver, form, 'a', 'Browse for folder')
            assert.equal(await driver.property(input, 'value'), originalFolder,
                'Cancelling the folder picker must preserve the entered path')

            await setFormControlValue(driver, '.border-end-sm input[type="text"]', '')
            assert.equal(await driver.property(input, 'value'), '',
                'The working-folder input must be empty before using the default')
            await clickByTextWithin(driver, form, 'button', 'Use this Folder')
            await waitUntil(async () =>
                (await driver.bodyText()).includes('No board information has been imported yet.'), {
                timeout: 60000,
                message: 'the default working folder to be used when the input is empty'
            })
            await fs.access(path.join(temporaryRoot, 'home', 'acrn-work', 'MyConfiguration'))

            await backToWelcome(driver)
            form = await welcomeForm(driver, 'Start a new configuration')
            input = await driver.findElementWithin(form, 'input[type="text"]')
            await driver.clear(input)
            await driver.type(input, `~/${missingName}`)
            await clickByTextWithin(driver, form, 'button', 'Use this Folder')
            await waitUntil(async () =>
                (await driver.bodyText()).includes('No board information has been imported yet.'), {
                timeout: 60000,
                message: 'the home-relative working folder to be created'
            })
            await fs.access(missingFolder)

            await backToWelcome(driver)
            form = await welcomeForm(driver, 'Start a new configuration')
            input = await driver.findElementWithin(form, 'input[type="text"]')
            await driver.clear(input)
            await driver.type(input, regularFile)
            await clickByTextWithin(driver, form, 'button', 'Use this Folder')
            await waitUntil(async () => await driver.executeScript(
                'return window.__e2eAlerts.length === 1'), {message: 'folder creation failure alert'})
            assert.ok((await driver.bodyText()).includes('Start a new configuration'))

            await fs.chmod(uncleanable, 0o500)
            await driver.clear(input)
            await driver.type(input, uncleanable)
            await clickByTextWithin(driver, form, 'button', 'Use this Folder')
            await waitUntil(async () => await driver.executeScript(
                'return window.__e2eAlerts.length === 2'), {message: 'working-folder cleanup failure alert'})
            assert.equal(await fs.readFile(path.join(uncleanable, 'scenario.xml'), 'utf8'),
                'preserve on failure')
            assert.ok((await driver.bodyText()).includes('Start a new configuration'))
        } finally {
            await fs.chmod(uncleanable, 0o700).catch(() => {})
            await restoreNativeDialogs(driver)
        }
    })

    test('browses for and opens an existing working folder', async () => {
        const fixture = bundledFixtures.find(item =>
            item.board === 'nuc11tnbi5' && item.scenario === 'hybrid_launch_2user_vm')
        const folder = await stageFixture(temporaryRoot, historyDirectory, fixture)
        await resetHistory(driver)
        await backToWelcome(driver)
        await mockNativeDialogs(driver, {open: folder.slice(0, -1), confirm: true})
        try {
            const form = await welcomeForm(driver, 'Use an existing configuration')
            await clickByTextWithin(driver, form, 'a', 'Browse for folder')
            const select = await driver.findElementWithin(form, 'select')
            await waitUntil(async () => await driver.property(select, 'value') === folder, {
                message: 'the browsed existing folder to be added to history'
            })
            await clickByTextWithin(driver, form, 'button', 'Open Folder')
            await waitUntil(async () => await currentScenarioData(driver) !== null, {
                timeout: 60000,
                message: 'the browsed existing Scenario to load'
            })
            assert.match(await driver.bodyText(), /POST_STD_VM2/)
            const history = JSON.parse(await fs.readFile(path.join(historyDirectory, 'config.json'), 'utf8'))
            assert.ok(history.history.working_folder.includes(folder))
            await driver.click(await driver.findElement('.ms-4 > *'))
            await waitUntil(async () => (await driver.bodyText()).includes('Open Folder'), {
                message: 'the Configurator back control to return to Welcome'
            })
        } finally {
            await restoreNativeDialogs(driver)
        }
    })

    test('cancels and confirms Board replacement without deleting the external source', async () => {
        const initial = bundledFixtures.find(item =>
            item.board === 'nuc11tnbi5' && item.scenario === 'shared')
        const replacement = bundledFixtures.find(item => item.board === 'cfl-k700-i7')
        const folder = await stageFixture(temporaryRoot, historyDirectory, initial)
        await backToWelcome(driver)
        await openWorkingFolder(driver)
        await expandAccordion(driver, '1. Import a board configuration file')
        await selectVMTab(driver, 'Hypervisor')
        await driver.click(await driver.findElement('.aTab'))
        await waitUntil(async () => (await driver.bodyText()).includes('Memory Isolation for Performance'), {
            message: 'CAT to be open while replacing the Board'
        })
        await mockNativeDialogs(driver, {open: replacement.boardPath, confirm: true})
        try {
            const oldBoard = path.join(folder, 'nuc11tnbi5.board.xml')
            const newBoard = path.join(folder, 'cfl-k700-i7.board.xml')
            await clickByText(driver, 'button', 'Use a Different Board')
            await waitUntil(async () =>
                (await driver.bodyText()).includes('Board XML overwrite'), {message: 'Board overwrite dialog'})
            await closeVisibleModal(driver, '.btn-secondary')
            assert.match(await driver.bodyText(), /Current file: nuc11tnbi5\.board\.xml/)
            await fs.access(oldBoard)
            await assert.rejects(fs.access(newBoard))

            await clickByText(driver, 'button', 'Use a Different Board')
            await waitUntil(async () =>
                (await driver.findElements('.modal.show')).length === 1, {message: 'Board overwrite dialog'})
            await closeVisibleModal(driver)
            await waitUntil(async () =>
                (await driver.bodyText()).includes('Current file: cfl-k700-i7.board.xml'), {
                timeout: 60000,
                message: 'the replacement Board to load'
            })
            await waitUntil(async () => {
                try {
                    await fs.access(newBoard)
                    return true
                } catch {
                    return false
                }
            }, {message: 'the replacement Board file to be durable'})
            await fs.access(replacement.boardPath)
            await waitUntil(async () => {
                try {
                    await fs.access(oldBoard)
                    return false
                } catch {
                    return true
                }
            }, {message: 'the previous working-folder Board to be removed'})
        } finally {
            await restoreNativeDialogs(driver)
        }
    })

    test('keeps valid data and reports malformed Board and Scenario imports', async () => {
        const fixture = bundledFixtures.find(item =>
            item.board === 'nuc11tnbi5' && item.scenario === 'shared')
        const folder = await stageFixture(temporaryRoot, historyDirectory, fixture)
        const invalidBoard = path.join(temporaryRoot, 'invalid-board.xml')
        const invalidScenario = path.join(temporaryRoot, 'invalid-scenario.xml')
        await fs.writeFile(invalidBoard, '<acrn-config><board>')
        await fs.writeFile(invalidScenario, '<acrn-config><hv>')
        await backToWelcome(driver)
        await openWorkingFolder(driver)
        const originalScenario = await currentScenarioData(driver)
        await mockNativeDialogs(driver, {open: null, confirm: true})
        try {
            await expandAccordion(driver, '1. Import a board configuration file')
            await clickByText(driver, 'button', 'Use a Different Board')
            assert.equal((await driver.findElements('.modal.show')).length, 0,
                'Cancelling the Board picker must not open the overwrite dialog')

            await setDialogResponses(driver, {open: invalidBoard, confirm: true})
            await clickByText(driver, 'button', 'Use a Different Board')
            await waitUntil(async () =>
                (await driver.bodyText()).includes('Board XML overwrite'), {message: 'Board overwrite dialog'})
            await closeVisibleModal(driver)
            await waitUntil(async () => await driver.executeScript(
                'return window.__e2eAlerts.length === 1'), {message: 'malformed Board error'})
            assert.match(await driver.executeScript('return window.__e2eAlerts[0]'),
                /Loading .*invalid-board\.xml failed:/)
            assert.match(await driver.bodyText(), /Current file: nuc11tnbi5\.board\.xml/)
            assert.deepEqual(await currentScenarioData(driver), originalScenario)

            await expandAccordion(driver, '2. Create new or import an existing scenario')
            await setDialogResponses(driver, {open: invalidScenario, confirm: true})
            await clickByText(driver, 'a', 'Browse for scenario file')
            await waitUntil(async () => await driver.executeScript(`
                return Array.from(document.querySelectorAll('select.form-select'))
                    .some(select => select.value === arguments[0]);
            `, [invalidScenario]), {message: 'the malformed Scenario to be selected'})
            await clickByText(driver, 'button', 'Import Scenario')
            await waitUntil(async () =>
                (await driver.bodyText()).includes('Scenario XML Overwrite'), {
                message: 'Scenario overwrite dialog'
            })
            await closeVisibleModal(driver)
            await waitUntil(async () => await driver.executeScript(
                'return window.__e2eAlerts.length === 2'), {message: 'malformed Scenario error'})
            assert.match(await driver.executeScript('return window.__e2eAlerts[1]'),
                /Loading .*invalid-scenario\.xml failed:/)
            assert.deepEqual(await currentScenarioData(driver), originalScenario)
        } finally {
            await restoreNativeDialogs(driver)
        }
    })

    test('ignores stale Board and Scenario history responses', async () => {
        const fixture = bundledFixtures.find(item =>
            item.board === 'nuc11tnbi5' && item.scenario === 'shared')
        const folder = await stageFixture(temporaryRoot, historyDirectory, fixture)
        await backToWelcome(driver)
        await mockNativeDialogs(driver, {open: null, confirm: true})
        try {
            await driver.executeScript(`
                const previousFetch = window.fetch;
                window.__e2eHistoryTransport = previousFetch;
                window.__e2eDelayedHistory = [];
                window.__e2eCompletedHistory = [];
                window.fetch = async (input, init) => {
                    const url = typeof input === 'string' ? input : input.url;
                    const command = url.startsWith('ipc://localhost/')
                        ? decodeURIComponent(url.slice('ipc://localhost/'.length)) : '';
                    const args = command === 'get_history' ? JSON.parse(init.body || '{}') : null;
                    const type = args?.historyType;
                    if ((type === 'Board' || type === 'Scenario') &&
                        !window.__e2eDelayedHistory.includes(type)) {
                        window.__e2eDelayedHistory.push(type);
                        const response = await previousFetch(input, init);
                        await new Promise(resolve => setTimeout(resolve, 1500));
                        window.__e2eCompletedHistory.push(type);
                        return response;
                    }
                    return previousFetch(input, init);
                };
            `)
            await openWorkingFolder(driver)
            await waitUntil(async () => await driver.executeScript(
                'return window.__e2eCompletedHistory.length === 2'), {
                timeout: 10000,
                message: 'both stale history responses to complete'
            })
            const selected = await driver.executeScript(`
                return Array.from(document.querySelectorAll('select.form-select')).map(select => select.value);
            `)
            assert.ok(selected.includes(path.join(folder, 'nuc11tnbi5.board.xml')),
                `Stale Board history replaced the current selection: ${selected}`)
            assert.ok(selected.includes(path.join(folder, 'scenario.xml')),
                `Stale Scenario history replaced the current selection: ${selected}`)
        } finally {
            await driver.executeScript(`
                if (window.__e2eHistoryTransport) window.fetch = window.__e2eHistoryTransport;
            `)
            await restoreNativeDialogs(driver)
        }
    })

    test('ignores stale Scenario loads that resolve or fail after a newer import', async () => {
        const fixture = bundledFixtures.find(item =>
            item.board === 'nuc11tnbi5' && item.scenario === 'shared')
        const partitioned = bundledFixtures.find(item =>
            item.board === 'nuc11tnbi5' && item.scenario === 'partitioned')
        const slowValid = bundledFixtures.find(item =>
            item.board === 'nuc11tnbi5' && item.scenario === 'hybrid_launch_2user_vm')
        const fastValid = bundledFixtures.find(item =>
            item.board === 'nuc11tnbi5' && item.scenario === 'shared_launch_6user_vm')
        const slowInvalid = path.join(temporaryRoot, 'slow-invalid-scenario.xml')
        await fs.writeFile(slowInvalid, '<acrn-config><hv>')
        await stageFixture(temporaryRoot, historyDirectory, fixture)
        await backToWelcome(driver)
        await openWorkingFolder(driver)
        await expandAccordion(driver, '2. Create new or import an existing scenario')
        await mockNativeDialogs(driver, {open: null, confirm: true})
        try {
            await driver.executeScript(`
                const previousFetch = window.fetch;
                window.__e2eScenarioTransport = previousFetch;
                window.__e2eSlowPaths = [];
                window.__e2eSlowReadsStarted = [];
                window.__e2eSlowReadsCompleted = [];
                window.fetch = async (input, init) => {
                    const url = typeof input === 'string' ? input : input.url;
                    const command = url.startsWith('ipc://localhost/')
                        ? decodeURIComponent(url.slice('ipc://localhost/'.length)) : '';
                    const args = command === 'acrn_read' ? JSON.parse(init.body || '{}') : null;
                    if (window.__e2eSlowPaths.includes(args?.filePath)) {
                        const response = await previousFetch(input, init);
                        window.__e2eSlowReadsStarted.push(args.filePath);
                        await new Promise(resolve => setTimeout(resolve, 1500));
                        window.__e2eSlowReadsCompleted.push(args.filePath);
                        return response;
                    }
                    return previousFetch(input, init);
                };
            `)
            await driver.executeScript(`
                window.__e2eSlowPaths = [arguments[0], arguments[1]];
            `, [slowInvalid, slowValid.scenarioPath])

            const selectAndImport = async filePath => {
                await setDialogResponses(driver, {open: filePath, confirm: true})
                await clickByText(driver, 'a', 'Browse for scenario file')
                await waitUntil(async () => await driver.executeScript(`
                    return Array.from(document.querySelectorAll('select.form-select'))
                        .some(select => select.value === arguments[0]);
                `, [filePath]), {message: `${filePath} to be selected`})
                await clickByText(driver, 'button', 'Import Scenario')
                await waitUntil(async () =>
                    (await driver.findElements('#my-modal.show')).length === 1, {
                    message: 'the Scenario overwrite dialog'
                })
                await closeVisibleModal(driver)
            }

            await selectAndImport(slowInvalid)
            await waitUntil(async () => await driver.executeScript(
                'return window.__e2eSlowReadsStarted.includes(arguments[0])', [slowInvalid]), {
                message: 'the delayed malformed Scenario read'
            })
            await selectAndImport(partitioned.scenarioPath)
            await waitUntil(async () => (await currentScenarioData(driver))?.vm?.length === 2, {
                timeout: 60000,
                message: 'the newer partitioned import to win'
            })
            await waitUntil(async () => await driver.executeScript(
                'return window.__e2eSlowReadsCompleted.includes(arguments[0])', [slowInvalid]), {
                message: 'the stale malformed Scenario failure'
            })
            let alerts = await driver.executeScript('return window.__e2eAlerts')
            assert.ok(!alerts.some(message => message.includes(slowInvalid)), JSON.stringify(alerts))

            await selectAndImport(slowValid.scenarioPath)
            await waitUntil(async () => await driver.executeScript(
                'return window.__e2eSlowReadsStarted.includes(arguments[0])', [slowValid.scenarioPath]), {
                message: 'the delayed valid Scenario read'
            })
            await selectAndImport(fastValid.scenarioPath)
            await waitUntil(async () => (await currentScenarioData(driver))?.vm?.length === 7, {
                timeout: 60000,
                message: 'the newer shared import to win'
            })
            await waitUntil(async () => await driver.executeScript(
                'return window.__e2eSlowReadsCompleted.includes(arguments[0])', [slowValid.scenarioPath]), {
                message: 'the stale valid Scenario result'
            })
            alerts = await driver.executeScript('return window.__e2eAlerts')
            assert.ok(!alerts.some(message => message.includes(slowValid.scenarioPath)), JSON.stringify(alerts))
            assert.ok(alerts.some(message => message.includes(fastValid.scenarioPath)), JSON.stringify(alerts))
        } finally {
            await driver.executeScript(`
                if (window.__e2eScenarioTransport) window.fetch = window.__e2eScenarioTransport;
            `)
            await restoreNativeDialogs(driver)
        }
    })

    test('creates and imports replacement Scenarios through overwrite dialogs', async () => {
        const initial = bundledFixtures.find(item =>
            item.board === 'nuc11tnbi5' && item.scenario === 'hybrid_launch_2user_vm')
        const imported = bundledFixtures.find(item =>
            item.board === 'nuc11tnbi5' && item.scenario === 'partitioned')
        let folder = await stageFixture(temporaryRoot, historyDirectory, initial)
        await backToWelcome(driver)
        await openWorkingFolder(driver)
        await expandAccordion(driver, '2. Create new or import an existing scenario')
        await mockNativeDialogs(driver, {open: imported.scenarioPath, confirm: true})
        try {
            await clickByText(driver, 'button', 'Create Scenario')
            await waitUntil(async () =>
                (await driver.bodyText()).includes('Scenario XML Overwrite'), {message: 'Scenario overwrite dialog'})
            await closeVisibleModal(driver, '.btn-secondary')
            assert.equal((await currentScenarioData(driver)).vm.length, 4)

            await clickByText(driver, 'button', 'Create Scenario')
            await waitUntil(async () =>
                (await driver.findElements('#my-modal.show')).length === 1, {message: 'Scenario overwrite dialog'})
            await closeVisibleModal(driver)
            await waitUntil(async () =>
                (await driver.findElements('#newScenarioModal.show')).length === 1, {message: 'new Scenario dialog'})
            await driver.executeScript(`document.querySelector('#newScenarioModal input[value="hybrid"]').click()`)
            await closeVisibleModal(driver)
            await waitUntil(async () => (await currentScenarioData(driver))?.vm?.length === 3, {
                message: 'the hybrid replacement Scenario'
            })
            const created = await currentScenarioData(driver)
            assert.deepEqual(created.vm.map(vm => vm.load_order), [
                'PRE_LAUNCHED_VM', 'SERVICE_VM', 'POST_LAUNCHED_VM'
            ])

            folder = await stageFixture(temporaryRoot, historyDirectory, initial)
            await backToWelcome(driver)
            await openWorkingFolder(driver)
            await expandAccordion(driver, '2. Create new or import an existing scenario')
            await clickByText(driver, 'a', 'Browse for scenario file')
            await waitUntil(async () => await driver.executeScript(`
                return Array.from(document.querySelectorAll('select.form-select'))
                    .some(select => select.value === arguments[0]);
            `, [imported.scenarioPath]), {message: 'the browsed Scenario to be selected'})
            await clickByText(driver, 'button', 'Import Scenario')
            await waitUntil(async () =>
                (await driver.findElements('#my-modal.show')).length === 1, {message: 'Scenario import overwrite dialog'})
            await closeVisibleModal(driver)
            await waitUntil(async () => {
                const scenario = await currentScenarioData(driver)
                return scenario?.vm?.length === 2 &&
                    scenario.vm.every(vm => vm.name.startsWith('PRE_STD_VM'))
            }, {
                timeout: 60000,
                message: 'the imported partitioned Scenario'
            })
            const alerts = await driver.executeScript('return window.__e2eAlerts')
            assert.ok(alerts.some(message => message.includes('loaded success!')), JSON.stringify(alerts))
            const result = await saveScenario(driver)
            assert.ok(result.body?.includes('All files successfully saved'), JSON.stringify(result))
            const scenarioXML = await fs.readFile(path.join(folder, 'scenario.xml'), 'utf8')
            assert.doesNotMatch(scenarioXML, /<name>POST_STD_VM1<\/name>/)
            assert.equal((await fs.readdir(folder)).filter(file => /^launch.*\.sh$/.test(file)).length, 0)
            await closeVisibleModal(driver)
        } catch (error) {
            preserveTemporaryRoot = true
            await driver.screenshot(path.join(artifacts, 'scenario-replacement-regression.png')).catch(() => {})
            console.error(`Scenario replacement diagnostics: ${JSON.stringify(await diagnosticMessages(driver))}`)
            throw error
        } finally {
            await restoreNativeDialogs(driver)
        }
    })

    test('creates a partitioned Scenario and configures new VM and InterVM devices', async () => {
        const fixture = bundledFixtures.find(item =>
            item.board === 'nuc11tnbi5' && item.scenario === 'shared')
        const folder = await stageFixture(temporaryRoot, historyDirectory, fixture)
        await backToWelcome(driver)
        await openWorkingFolder(driver)
        await expandAccordion(driver, '2. Create new or import an existing scenario')
        try {
            for (const vmCounts of [[0, 0], [1, 0]]) {
                await clickByText(driver, 'button', 'Create Scenario')
                await waitUntil(async () =>
                    (await driver.bodyText()).includes('Scenario XML Overwrite'), {
                    message: 'Scenario overwrite dialog'
                })
                await closeVisibleModal(driver)
                await waitUntil(async () =>
                    (await driver.findElements('#newScenarioModal.show')).length === 1, {
                    message: 'the new Scenario dialog'
                })
                await clickByText(driver, '#newScenarioModal label', 'Partitioned')
                await waitUntil(async () =>
                    (await driver.findElements('#newScenarioModal input[type="number"]')).length === 2, {
                    message: 'partitioned VM count inputs'
                })
                const inputs = await driver.findElements('#newScenarioModal input[type="number"]')
                for (let index = 0; index < inputs.length; index++) {
                    await driver.clear(inputs[index])
                    await driver.type(inputs[index], String(vmCounts[index]))
                }
                await closeVisibleModal(driver)
                await waitUntil(async () =>
                    (await currentScenarioData(driver))?.vm?.length === vmCounts[0] + vmCounts[1], {
                    message: `partitioned Scenario with ${vmCounts.join('/')} VMs`
                })
                if (vmCounts[0] === 0) {
                    const emptyResult = await saveScenario(driver)
                    assert.ok(emptyResult.body?.includes('Please add at least one VM'), emptyResult.body)
                    await closeVisibleModal(driver)
                }
            }

            const invalidPreLaunched = await saveScenario(driver)
            assert.ok(invalidPreLaunched.body?.includes('Settings validate failed'),
                invalidPreLaunched.body)
            assert.ok(invalidPreLaunched.body?.includes('Scenario xml saved'),
                invalidPreLaunched.body)
            assert.ok(!invalidPreLaunched.body?.includes('Scenario xml save failed'),
                invalidPreLaunched.body)
            await closeVisibleModal(driver)
            assert.match(await fs.readFile(path.join(folder, 'scenario.xml'), 'utf8'),
                /<load_order>PRE_LAUNCHED_VM<\/load_order>/,
                'The invalid pre-launched Scenario must still be saved')

            await selectVMTab(driver, 'VM0')
            await driver.click(await driver.findElement('.aTab'))
            await driver.click(await driver.findElement('.bTab'))
            await setFormControlValue(driver, '.cpu_affinity select', 0)
            await clickByText(driver, '.TabAdd', 'Post-launched VM')
            await waitUntil(async () => (await currentScenarioData(driver)).vm.length === 3, {
                message: 'a Service VM and post-launched VM to be added'
            })
            let scenario = await currentScenarioData(driver)
            assert.deepEqual(scenario.vm.map(vm => vm.load_order),
                ['PRE_LAUNCHED_VM', 'SERVICE_VM', 'POST_LAUNCHED_VM'])

            await selectVMTab(driver, 'Hypervisor')
            await selectNaiveOption(driver, 'Build type', 'Release')
            await clickToolInSet(driver, '.tabContent .ToolSet', 0, 1)
            await waitUntil(async () => (await driver.findElements('.VUART_SETTING')).length === 1, {
                message: 'the first vUART connection'
            })
            await setFormControlValue(driver, '.VUART_SETTING select', 'VM0', 0)
            await setFormControlValue(driver, '.VUART_SETTING select', 'VM2', 1)
            await setFormControlValue(driver, '.VUART_SETTING input', '00:0a.0', 0)
            await setFormControlValue(driver, '.VUART_SETTING input', '00:0b.0', 1)
            await clickWidgetTool(driver, '.VUART_SETTING', 1)
            await waitUntil(async () => (await driver.findElements('.VUART_SETTING')).length === 2, {
                message: 'a second new vUART connection'
            })
            await setFormControlValue(driver, '.VUART_SETTING select', 'VM0', 3)
            await setFormControlValue(driver, '.VUART_SETTING select', 'VM1', 4)
            await clickWidgetTool(driver, '.VUART_SETTING', 0, 1)
            await waitUntil(async () => (await driver.findElements('.VUART_SETTING')).length === 1, {
                message: 'the second new vUART connection to be removed'
            })

            await clickToolInSet(driver, '.tabContent .ToolSet', 0, 0)
            await waitUntil(async () => (await driver.findElements('.IVSH_REGION')).length === 1, {
                message: 'the first IVSHMEM region'
            })
            await setFormControlValue(driver, '.IVSH_REGION input', 'E2E_PARTITIONED', 0)
            await setFormControlValue(driver, '.IVSH_REGION select', 'VM0', 2)
            await setFormControlValue(driver, '.IVSH_REGION select', 'VM2', 3)
            await setFormControlValue(driver, '.IVSH_REGION input', '00:08.0', 2)
            await setFormControlValue(driver, '.IVSH_REGION input', '00:09.0', 3)

            await selectVMTab(driver, 'VM2')
            await setFormControlValue(driver, '.cpu_affinity select', 2)
            await clickAddControlByTitle(driver, 'Virtio network device')
            await setFormControlValue(driver, '.virtio_network input', 'E2E_PARTITIONED_NET')
            await clickAddControlByTitle(driver, 'Virtio console device')
            await setFormControlValue(driver, '.virtio_console select', 'file')
            await setFormControlValue(driver, '.virtio_console input', '/tmp/e2e-partitioned-console.log')
            await clickAddControlByTitle(driver, 'Virtio input device')
            await setFirstAvailableOption(driver, '.virtio_input select')
            await setFormControlValue(driver, '.virtio_input input', 'e2e-partitioned-input')

            const result = await saveScenario(driver)
            assert.ok(result.body?.includes('All files successfully saved'), JSON.stringify(result))
            await closeVisibleModal(driver)
            await backToWelcome(driver)
            await openWorkingFolder(driver)
            scenario = await currentScenarioData(driver)
            assert.equal(scenario.hv.FEATURES.IVSHMEM.IVSHMEM_REGION[0].NAME, 'E2E_PARTITIONED')
            assert.equal(scenario.hv.vuart_connections.vuart_connection.length, 1)
            const post = scenario.vm.find(vm => vm.name === 'VM2')
            assert.equal(post.cpu_affinity.pcpu[0].pcpu_id, 2)
            assert.equal(post.virtio_devices.network[0].interface_name, 'E2E_PARTITIONED_NET')
            assert.equal(post.virtio_devices.console[0].output_file_path, '/tmp/e2e-partitioned-console.log')
            assert.equal(post.virtio_devices.input[0].id, 'e2e-partitioned-input')
        } catch (error) {
            preserveTemporaryRoot = true
            await driver.screenshot(path.join(artifacts, 'partitioned-workflow-regression.png')).catch(() => {})
            console.error(`Partitioned workflow diagnostics: ${JSON.stringify(await diagnosticMessages(driver))}`)
            throw error
        }
    })

    test('adds, edits, deletes, and reloads a post-launched VM', async () => {
        const fixture = bundledFixtures.find(item =>
            item.board === 'nuc11tnbi5' && item.scenario === 'hybrid_launch_2user_vm')
        const folder = await stageFixture(temporaryRoot, historyDirectory, fixture)
        await backToWelcome(driver)
        await openWorkingFolder(driver)
        await mockNativeDialogs(driver, {confirm: false})
        try {
            await driver.executeScript(`
                document.querySelector('img[alt="Add Post-launched VM"]').closest('.TabAdd').click()
            `)
            await waitUntil(async () => (await currentScenarioData(driver)).vm.length === 5, {
                message: 'a post-launched VM to be added'
            })
            await setNamedFormValue(driver, 'VM name', 'E2E_EXTRA_VM')
            await setFormControlValue(driver, '.tabContent select.form-select', 2)
            await clickToolInSet(driver, '.cpu_affinity .ToolSet', 1)
            await waitUntil(async () =>
                (await driver.findElements('.cpu_affinity select.form-select')).length === 2, {
                message: 'a second pCPU affinity row'
            })
            await setFormControlValue(driver, '.cpu_affinity select.form-select', 3)
            assert.equal((await currentScenarioData(driver)).vm.find(
                vm => vm.name === 'E2E_EXTRA_VM').cpu_affinity.pcpu.length, 2)
            await clickToolInSet(driver, '.cpu_affinity .ToolSet', 0, 1)
            await waitUntil(async () =>
                (await driver.findElements('.cpu_affinity select.form-select')).length === 1, {
                message: 'the second pCPU affinity row to be removed'
            })
            await clickToolInSet(driver, '.cpu_affinity .ToolSet', 0)
            let result = await saveScenario(driver)
            assert.ok(result.body?.includes('All files successfully saved'), JSON.stringify(result))
            assert.match(await fs.readFile(path.join(folder, 'scenario.xml'), 'utf8'),
                /<name>E2E_EXTRA_VM<\/name>/)
            assert.equal((await fs.readdir(folder)).filter(file => /^launch.*\.sh$/.test(file)).length, 3)
            await closeVisibleModal(driver)
            await backToWelcome(driver)
            await openWorkingFolder(driver)
            await selectVMTab(driver, 'E2E_EXTRA_VM')

            await clickByText(driver, 'button.deleteVM', 'Delete VM')
            await waitUntil(async () => await driver.executeScript(
                'return window.__e2eConfirmMessages.length === 1'), {message: 'VM deletion confirmation'})
            assert.equal((await currentScenarioData(driver)).vm.length, 5)
            await setDialogResponses(driver, {confirm: true})
            await clickByText(driver, 'button.deleteVM', 'Delete VM')
            await waitUntil(async () => (await currentScenarioData(driver)).vm.length === 4, {
                message: 'the post-launched VM to be deleted'
            })
            result = await saveScenario(driver)
            assert.ok(result.body?.includes('All files successfully saved'), JSON.stringify(result))
            assert.doesNotMatch(await fs.readFile(path.join(folder, 'scenario.xml'), 'utf8'),
                /<name>E2E_EXTRA_VM<\/name>/)
            assert.equal((await fs.readdir(folder)).filter(file => /^launch.*\.sh$/.test(file)).length, 2)
            await closeVisibleModal(driver)
            await backToWelcome(driver)
            await openWorkingFolder(driver)
            assert.doesNotMatch(await driver.bodyText(), /E2E_EXTRA_VM/)
        } catch (error) {
            preserveTemporaryRoot = true
            await driver.screenshot(path.join(artifacts, 'vm-edit-regression.png')).catch(() => {})
            console.error(`VM editing diagnostics: ${JSON.stringify(await diagnosticMessages(driver))}`)
            throw error
        } finally {
            await restoreNativeDialogs(driver)
        }
    })

    test('cascades Service VM deletion to post-launched VMs and persists the result', async () => {
        const fixture = bundledFixtures.find(item =>
            item.board === 'nuc11tnbi5' && item.scenario === 'hybrid_launch_2user_vm')
        const folder = await stageFixture(temporaryRoot, historyDirectory, fixture)
        await backToWelcome(driver)
        await openWorkingFolder(driver)
        await mockNativeDialogs(driver, {confirm: true})
        try {
            await selectVMTab(driver, 'ACRN_Service_VM')
            await clickByText(driver, 'button.deleteVM', 'Delete VM')
            await waitUntil(async () => (await currentScenarioData(driver)).vm.length === 1, {
                message: 'Service VM deletion to cascade'
            })
            const scenario = await currentScenarioData(driver)
            assert.equal(scenario.vm[0].name, 'SAFETY_VM0')
            assert.equal(scenario.hv.vuart_connections?.vuart_connection?.length || 0, 0,
                'The deleted VM left a dangling vUART connection')
            const result = await saveScenario(driver)
            assert.ok(result.body?.includes('All files successfully saved'), JSON.stringify(result))
            const scenarioXML = await fs.readFile(path.join(folder, 'scenario.xml'), 'utf8')
            assert.doesNotMatch(scenarioXML, /ACRN_Service_VM|POST_STD_VM/)
            assert.equal((await fs.readdir(folder)).filter(file => /^launch.*\.sh$/.test(file)).length, 0)
            await closeVisibleModal(driver)
        } finally {
            await restoreNativeDialogs(driver)
        }
    })

    test('edits and persists Virtio network, console, and input devices', async () => {
        const fixture = bundledFixtures.find(item =>
            item.board === 'nuc11tnbi5' && item.scenario === 'shared_launch_6user_vm')
        const folder = await stageFixture(temporaryRoot, historyDirectory, fixture)
        await backToWelcome(driver)
        await openWorkingFolder(driver)
        await selectVMTab(driver, 'POST_STD_VM1')
        try {
            await clickCheckboxByText(driver, 'Exclusively owns physical CPUs')
            await clickCheckboxByText(driver, 'Exclusively owns physical CPUs')
            await setFirstAvailableOption(driver, '.virtio_network select')
            await setFormControlValue(driver, '.virtio_network input', 'E2E_NET')
            await clickWidgetTool(driver, '.virtio_network', 1)
            await waitUntil(async () => (await driver.findElements('.virtio_network')).length === 2, {
                message: 'a second Virtio network device'
            })
            await setFormControlValue(driver, '.virtio_network input', 'E2E_NET2')
            await clickWidgetTool(driver, '.virtio_network', 0, 1)
            await waitUntil(async () => (await driver.findElements('.virtio_network')).length === 1, {
                message: 'the second Virtio network device to be removed'
            })

            await clickAddControlByTitle(driver, 'Virtio console device')
            await waitUntil(async () => (await driver.findElements('.virtio_console')).length === 1, {
                message: 'a Virtio console device'
            })
            await setFormControlValue(driver, '.virtio_console select', 'Virtio console', 0)
            await setFormControlValue(driver, '.virtio_console select', 'file')
            await setFormControlValue(driver, '.virtio_console input', '/tmp/e2e-console.log')
            await clickWidgetTool(driver, '.virtio_console', 1)
            await waitUntil(async () => (await driver.findElements('.virtio_console')).length === 2, {
                message: 'a second Virtio console device'
            })
            await clickWidgetTool(driver, '.virtio_console', 0, 1)

            await clickAddControlByTitle(driver, 'Virtio input device')
            await waitUntil(async () => (await driver.findElements('.virtio_input')).length === 1, {
                message: 'a Virtio input device'
            })
            await setFirstAvailableOption(driver, '.virtio_input select')
            await setFormControlValue(driver, '.virtio_input input', 'e2e-input')
            await clickWidgetTool(driver, '.virtio_input', 1)
            await waitUntil(async () => (await driver.findElements('.virtio_input')).length === 2, {
                message: 'a second Virtio input device'
            })
            await clickWidgetTool(driver, '.virtio_input', 0, 1)

            const result = await saveScenario(driver)
            assert.ok(result.body?.includes('All files successfully saved'), JSON.stringify(result))
            await closeVisibleModal(driver)
            await backToWelcome(driver)
            await openWorkingFolder(driver)
            await selectVMTab(driver, 'POST_STD_VM1')
            const vm = (await currentScenarioData(driver)).vm.find(item => item.name === 'POST_STD_VM1')
            assert.equal(vm.virtio_devices.network[0].interface_name, 'E2E_NET')
            assert.equal(vm.virtio_devices.console[0].backend_type, 'file')
            assert.equal(vm.virtio_devices.console[0].output_file_path, '/tmp/e2e-console.log')
            assert.equal(vm.virtio_devices.input[0].id, 'e2e-input')
            assert.ok(vm.virtio_devices.input[0].backend_device_file)
        } catch (error) {
            preserveTemporaryRoot = true
            await driver.screenshot(path.join(artifacts, 'virtio-devices-regression.png')).catch(() => {})
            console.error(`Virtio diagnostics: ${JSON.stringify(await diagnosticMessages(driver))}`)
            throw error
        }
    })

    test('edits preconfigured Virtio console TTY and socket backends', async () => {
        const fixture = bundledFixtures.find(item =>
            item.board === 'nuc11tnbi5' && item.scenario === 'shared_launch_6user_vm')
        const consoles = `
      <console>
        <use_type>Virtio console</use_type>
        <backend_type>tty</backend_type>
        <tty_device_path>/dev/ttyS0</tty_device_path>
      </console>
      <console>
        <use_type>Virtio console</use_type>
        <backend_type>sock server</backend_type>
        <sock_file_path>/tmp/e2e-console-before.sock</sock_file_path>
      </console>`
        await stageFixture(temporaryRoot, historyDirectory, fixture,
            text => text.replace('<console/>', consoles))
        await backToWelcome(driver)
        await openWorkingFolder(driver)
        await selectVMTab(driver, 'POST_STD_VM1')
        await waitUntil(async () => (await driver.findElements('.virtio_console')).length === 2, {
            message: 'the preconfigured TTY and socket consoles'
        })
        assert.equal((await driver.findElements('.virtio_console input')).length, 2)
        await setFormControlValue(driver, '.virtio_console input', '/dev/ttyS1', 0)
        await setFormControlValue(driver, '.virtio_console input', '/tmp/e2e-console-after.sock', 1)
        await waitUntil(async () => {
            const vm = (await currentScenarioData(driver)).vm.find(item => item.name === 'POST_STD_VM1')
            return vm.virtio_devices.console[0].tty_device_path === '/dev/ttyS1' &&
                vm.virtio_devices.console[1].sock_file_path === '/tmp/e2e-console-after.sock'
        }, {message: 'the console path edits to reach Scenario data'})
        const result = await saveScenario(driver)
        assert.ok(result.body?.includes('All files successfully saved'), JSON.stringify(result))
        await closeVisibleModal(driver)
        await backToWelcome(driver)
        await openWorkingFolder(driver)
        await selectVMTab(driver, 'POST_STD_VM1')
        const vm = (await currentScenarioData(driver)).vm.find(item => item.name === 'POST_STD_VM1')
        assert.equal(vm.virtio_devices.console[0].backend_type, 'tty')
        assert.equal(vm.virtio_devices.console[0].tty_device_path, '/dev/ttyS1')
        assert.equal(vm.virtio_devices.console[1].backend_type, 'sock server')
        assert.equal(vm.virtio_devices.console[1].sock_file_path, '/tmp/e2e-console-after.sock')
    })

    test('configures, reloads, and removes Virtio GPU displays', async () => {
        const fixture = bundledFixtures.find(item =>
            item.board === 'nuc11tnbi5' && item.scenario === 'shared_launch_6user_vm')
        const folder = await stageFixture(temporaryRoot, historyDirectory, fixture)
        await backToWelcome(driver)
        await openWorkingFolder(driver)
        await selectVMTab(driver, 'POST_STD_VM1')
        try {
            await clickAddControlByTitle(driver, 'Virtio GPU device')
            await waitUntil(async () => (await driver.findElements('.virtio_gpu')).length === 1, {
                message: 'a Virtio GPU device'
            })
            await setFormControlValue(driver, '.virtio_gpu select', 'Window')
            await waitUntil(async () => (await driver.findElements('.windowDisplay')).length === 1, {
                message: 'a Window GPU display'
            })
            await setFirstAvailableOption(driver, '.windowDisplay select')
            await setFormControlValue(driver, '.windowDisplay input', 0, 0)
            await setFormControlValue(driver, '.windowDisplay input', 0, 1)
            await clickToolInSet(driver, '.virtio_gpu_demo > div:has(> .ToolSet) > .ToolSet', 1)
            await waitUntil(async () => (await driver.findElements('.windowDisplay')).length === 2, {
                message: 'a second Window GPU display'
            })
            await setFirstAvailableOption(driver, '.windowDisplay select')
            await setFormControlValue(driver, '.windowDisplay input', 1920, 2)
            await setFormControlValue(driver, '.windowDisplay input', 1080, 3)
            await clickToolInSet(driver, '.virtio_gpu_demo > div:has(> .ToolSet) > .ToolSet', 0, 1)
            await waitUntil(async () => (await driver.findElements('.windowDisplay')).length === 1, {
                message: 'the second Window GPU display to be removed'
            })
            await clickToolInSet(driver, '.virtio_gpu_demo > div:has(> .ToolSet) > .ToolSet', 0)

            await setFormControlValue(driver, '.virtio_gpu_demo > .row select', 'Full screen')
            await setFirstAvailableOption(driver, '.virtio_gpu select')
            await clickToolInSet(driver, '.virtio_gpu_demo > div:has(> .ToolSet) > .ToolSet', 1)
            await waitUntil(async () => (await driver.findElements('.virtio_gpu select')).length === 3, {
                message: 'a second full-screen GPU display'
            })
            await setFirstAvailableOption(driver, '.virtio_gpu select')
            await clickToolInSet(driver, '.virtio_gpu_demo > div:has(> .ToolSet) > .ToolSet', 0, 1)
            await waitUntil(async () => (await driver.findElements('.virtio_gpu select')).length === 2, {
                message: 'the second full-screen GPU display to be removed'
            })
            await setFormControlValue(driver, '.virtio_gpu_demo > .row select', 'Window')

            let result = await saveScenario(driver)
            assert.ok(result.body?.includes('All files successfully saved'), JSON.stringify(result))
            await closeVisibleModal(driver)
            await backToWelcome(driver)
            await openWorkingFolder(driver)
            await selectVMTab(driver, 'POST_STD_VM1')
            let vm = (await currentScenarioData(driver)).vm.find(item => item.name === 'POST_STD_VM1')
            assert.equal(vm.virtio_devices.gpu[0].display_type, 'Window')
            assert.equal(vm.virtio_devices.gpu[0].displays.display.length, 1)
            assert.equal(String(vm.virtio_devices.gpu[0].displays.display[0].horizontal_offset), '0')
            assert.equal(String(vm.virtio_devices.gpu[0].displays.display[0].vertical_offset), '0')

            await clickWidgetTool(driver, '.virtio_gpu', 0)
            await waitUntil(async () => (await driver.findElements('.virtio_gpu')).length === 0, {
                message: 'the Virtio GPU device to be removed'
            })
            result = await saveScenario(driver)
            assert.ok(result.body?.includes('All files successfully saved'), JSON.stringify(result))
            await closeVisibleModal(driver)
            vm = (await currentScenarioData(driver)).vm.find(item => item.name === 'POST_STD_VM1')
            assert.equal(vm.virtio_devices.gpu?.length || 0, 0)
        } catch (error) {
            preserveTemporaryRoot = true
            await driver.screenshot(path.join(artifacts, 'virtio-gpu-regression.png')).catch(() => {})
            console.error(`GPU diagnostics: ${JSON.stringify(await diagnosticMessages(driver))}`)
            throw error
        }
    })

    test('updates IVSHMEM and vUART references and persists widget edits', async () => {
        const fixture = bundledFixtures.find(item =>
            item.board === 'whl-ipc-i5' && item.scenario === 'hybrid_rt_launch_1user_vm_waag')
        const folder = await stageFixture(temporaryRoot, historyDirectory, fixture)
        await backToWelcome(driver)
        await openWorkingFolder(driver)
        try {
            await selectVMTab(driver, 'POST_STD_VM1')
            await setNamedFormValue(driver, 'VM name', 'E2E_SHARED_VM')
            await waitUntil(async () => (await currentScenarioData(driver)).vm
                .some(vm => vm.name === 'E2E_SHARED_VM'), {message: 'the shared VM rename'})
            await selectVMTab(driver, 'Hypervisor')
            await waitUntil(async () => {
                const regions = (await currentScenarioData(driver)).hv.FEATURES.IVSHMEM.IVSHMEM_REGION
                return regions[0]?.IVSHMEM_VMS.IVSHMEM_VM
                    .some(vm => vm.VM_NAME === 'E2E_SHARED_VM')
            }, {message: 'the IVSHMEM rename propagation'})
            let scenario = await currentScenarioData(driver)
            assert.ok(scenario.hv.FEATURES.IVSHMEM.IVSHMEM_REGION[0].IVSHMEM_VMS.IVSHMEM_VM
                .some(vm => vm.VM_NAME === 'E2E_SHARED_VM'))

            await setNamedFormValue(driver, 'Region name', 'E2E_REGION')
            await clickToolInSet(driver, '.IVSH_REGION_CONTENT .ToolSet', 1)
            await waitUntil(async () =>
                (await driver.findElements('.IVSH_REGION_CONTENT .ToolSet')).length === 3, {
                message: 'a third IVSHMEM shared VM'
            })
            await clickIVSHMEMMinusForVM(driver, '')
            await waitUntil(async () =>
                (await currentScenarioData(driver)).hv.FEATURES.IVSHMEM.IVSHMEM_REGION[0]
                    .IVSHMEM_VMS.IVSHMEM_VM.length === 2, {
                message: 'the empty IVSHMEM shared VM to be removed'
            })
            await clickIVSHMEMMinusForVM(driver, 'E2E_SHARED_VM')
            await clickWidgetTool(driver, '.IVSH_REGION', 1)
            await waitUntil(async () => (await driver.findElements('.IVSH_REGION')).length === 2, {
                message: 'a second IVSHMEM region'
            })
            await clickWidgetTool(driver, '.IVSH_REGION', 0, 1)
            await waitUntil(async () => (await driver.findElements('.IVSH_REGION')).length === 1, {
                message: 'the second IVSHMEM region to be removed'
            })
            scenario = await currentScenarioData(driver)
            assert.equal(scenario.hv.FEATURES.IVSHMEM.IVSHMEM_REGION.length, 1)
            assert.equal(scenario.hv.FEATURES.IVSHMEM.IVSHMEM_REGION[0].NAME, 'E2E_REGION')
            assert.deepEqual(scenario.hv.FEATURES.IVSHMEM.IVSHMEM_REGION[0]
                .IVSHMEM_VMS.IVSHMEM_VM.map(vm => vm.VM_NAME), ['PRE_RT_VM0', 'E2E_SHARED_VM'])

            await clickWidgetTool(driver, '.VUART_SETTING', 1)
            await waitUntil(async () => (await driver.findElements('.VUART_SETTING')).length === 2, {
                message: 'a second vUART connection'
            })
            await clickWidgetTool(driver, '.VUART_SETTING', 0, 1)
            await waitUntil(async () => (await driver.findElements('.VUART_SETTING')).length === 1, {
                message: 'the second vUART connection to be removed'
            })
            await setFormControlValue(driver, '.VUART_SETTING select', 'E2E_SHARED_VM', 0)
            await setFormControlValue(driver, '.VUART_SETTING select', 'PRE_RT_VM0', 1)
            await setFormControlValue(driver, '.VUART_SETTING select', 'pci')
            await waitUntil(async () => (await driver.findElements('.VUART_SETTING input')).length === 2, {
                message: 'PCI vUART BDF fields'
            })
            await setFormControlValue(driver, '.VUART_SETTING input', '00:08.0')
            await setFormControlValue(driver, '.VUART_SETTING input', '00:09.0')
            await setFormControlValue(driver, '.VUART_SETTING select', 'legacy')
            await setFormControlValue(driver, '.VUART_SETTING input', '0x2F8')
            await setFormControlValue(driver, '.VUART_SETTING input', '0x2F8')

            await selectVMTab(driver, 'PRE_RT_VM0')
            await setNamedFormValue(driver, 'VM name', 'E2E_PRE_RT')
            await waitUntil(async () => (await currentScenarioData(driver)).hv.vuart_connections
                .vuart_connection[0].endpoint[1].vm_name === 'E2E_PRE_RT', {
                message: 'the vUART endpoint rename propagation'
            })

            const result = await saveScenario(driver)
            assert.ok(result.body?.includes('All files successfully saved'), JSON.stringify(result))
            await closeVisibleModal(driver)
            await backToWelcome(driver)
            await openWorkingFolder(driver)
            scenario = await currentScenarioData(driver)
            assert.equal(scenario.hv.FEATURES.IVSHMEM.IVSHMEM_REGION[0].NAME, 'E2E_REGION')
            assert.ok(scenario.hv.FEATURES.IVSHMEM.IVSHMEM_REGION[0].IVSHMEM_VMS.IVSHMEM_VM
                .some(vm => vm.VM_NAME === 'E2E_SHARED_VM'))
            assert.equal(scenario.hv.vuart_connections.vuart_connection.length, 1)
            assert.equal(scenario.hv.vuart_connections.vuart_connection[0].type, 'legacy')
            assert.equal(scenario.hv.vuart_connections.vuart_connection[0].endpoint[1].vm_name,
                'E2E_PRE_RT')

            await mockNativeDialogs(driver, {confirm: true})
            try {
                await selectVMTab(driver, 'E2E_SHARED_VM')
                await clickByText(driver, 'button', 'Delete VM')
                await waitUntil(async () => !(await currentScenarioData(driver)).vm
                    .some(vm => vm.name === 'E2E_SHARED_VM'), {
                    message: 'the shared VM and its references to be removed'
                })
                scenario = await currentScenarioData(driver)
                assert.equal(scenario.hv.FEATURES.IVSHMEM.IVSHMEM_REGION.length, 0)
                assert.equal(scenario.hv.vuart_connections.vuart_connection.length, 0)
                const deletionResult = await saveScenario(driver)
                assert.ok(deletionResult.body?.includes('All files successfully saved'), deletionResult.body)
                await closeVisibleModal(driver)
            } finally {
                await restoreNativeDialogs(driver)
            }
        } catch (error) {
            preserveTemporaryRoot = true
            await driver.screenshot(path.join(artifacts, 'intervm-widgets-regression.png')).catch(() => {})
            console.error(`InterVM widget diagnostics: ${JSON.stringify(await diagnosticMessages(driver))}`)
            throw error
        }
    })

    test('repairs an empty CPU affinity element through the VM form', async () => {
        const fixture = bundledFixtures.find(item =>
            item.board === 'nuc11tnbi5' && item.scenario === 'shared')
        const folder = await stageFixture(temporaryRoot, historyDirectory, fixture, text => text
            .replace(/(<name>POST_STD_VM1<\/name>[\s\S]*?)<cpu_affinity>[\s\S]*?<\/cpu_affinity>/,
                '$1<cpu_affinity/>'))
        await backToWelcome(driver)
        await openWorkingFolder(driver)
        await selectVMTab(driver, 'POST_STD_VM1')
        await clickToolInSet(driver, '.cpu_affinity .ToolSet', 0)
        await waitUntil(async () => (await driver.findElements('.cpu_affinity select')).length > 0, {
            message: 'a new pCPU row for an empty affinity element'
        })
        await setFormControlValue(driver, '.cpu_affinity select', 0)
        const result = await saveScenario(driver)
        assert.ok(result.body?.includes('All files successfully saved'), JSON.stringify(result))
        const scenarioXML = await fs.readFile(path.join(folder, 'scenario.xml'), 'utf8')
        assert.match(scenarioXML, /<name>POST_STD_VM1<\/name>[\s\S]*?<pcpu_id>0<\/pcpu_id>/)
        await closeVisibleModal(driver)
    })

    test('saves a Scenario with missing CAT CPU affinity and reports its validation errors', async () => {
        const fixture = bundledFixtures.find(item =>
            item.board === 'adl-asrock' && item.scenario === 'shared')
        const folder = await stageFixture(temporaryRoot, historyDirectory, fixture, text => text
            .replace('<RDT_ENABLED>n</RDT_ENABLED>', '<RDT_ENABLED>y</RDT_ENABLED>')
            .replace(/(<name>POST_STD_VM1<\/name>[\s\S]*?)<cpu_affinity>[\s\S]*?<\/cpu_affinity>/, '$1'))
        const scenarioPath = path.join(folder, 'scenario.xml')
        const originalXML = await fs.readFile(scenarioPath, 'utf8')
        await backToWelcome(driver)
        await openWorkingFolder(driver)
        await selectVMTab(driver, 'Hypervisor')
        await driver.click(await driver.findElement('.aTab'))
        await waitUntil(async () => {
            const body = await driver.bodyText()
            return body.includes('VM POST_STD_VM1 has no CPU affinity setting') &&
                body.includes('Please set CPU affinity for all VMs')
        }, {message: 'the CAT CPU affinity diagnostic'})
        const result = await saveScenario(driver)
        assert.ok(result.body?.includes('Settings validate failed'), result.body)
        assert.ok(result.body?.includes('Scenario xml saved'), result.body)
        assert.ok(!result.body?.includes('Scenario xml save failed'), result.body)
        const savedXML = await fs.readFile(scenarioPath, 'utf8')
        assert.notEqual(savedXML, originalXML, 'The invalid Scenario must still be written')
        assert.match(savedXML, /<RDT_ENABLED>y<\/RDT_ENABLED>/)
        await assert.rejects(fs.access(path.join(folder, 'config_summary.rst')),
            'An invalid CAT Scenario must not generate a configuration summary')
        await closeVisibleModal(driver)
    })

    test('confirms CAT changes, applies cache defaults, and saves them', async () => {
        const fixture = bundledFixtures.find(item =>
            item.board === 'adl-asrock' && item.scenario === 'shared')
        const existingCacheRegion = `
            <CACHE_REGION>
              <CACHE_ALLOCATION>
                <CACHE_ID>0x0</CACHE_ID>
                <CACHE_LEVEL>3</CACHE_LEVEL>
                <POLICY><VM>ACRN_Service_VM</VM><VCPU>0</VCPU><TYPE>Unified</TYPE><CLOS_MASK>0x1fff</CLOS_MASK></POLICY>
                <POLICY><VM>POST_STD_VM1</VM><VCPU>0</VCPU><TYPE>Unified</TYPE><CLOS_MASK>0x1f</CLOS_MASK></POLICY>
                <POLICY><VM>POST_STD_VM2</VM><VCPU>0</VCPU><TYPE>Unified</TYPE><CLOS_MASK>0x1000</CLOS_MASK></POLICY>
                <POLICY><VM>POST_RT_VM1</VM><VCPU>0</VCPU><TYPE>Unified</TYPE><CLOS_MASK>0x0</CLOS_MASK></POLICY>
              </CACHE_ALLOCATION>
            </CACHE_REGION>`
        await stageFixture(temporaryRoot, historyDirectory, fixture,
            text => text.replace('</hv>', `${existingCacheRegion}</hv>`))
        await backToWelcome(driver)
        await openWorkingFolder(driver)
        await mockNativeDialogs(driver, {confirm: false})
        try {
            await clickByText(driver, '.TabAdd', 'Pre-launched VM')
            await waitUntil(async () => (await currentScenarioData(driver)).vm.length === 5, {
                message: 'a pre-launched VM for CAT affinity'
            })
            await setFormControlValue(driver, '.cpu_affinity select', 10, 0)
            await clickToolInSet(driver, '.cpu_affinity .ToolSet', 1)
            await setFormControlValue(driver, '.cpu_affinity select', 11, 1)
            await selectVMTab(driver, 'POST_RT_VM1')
            await setFormControlValue(driver, '.cpu_affinity select', 8, 0)
            await setFormControlValue(driver, '.cpu_affinity select', 9, 1)
            await selectVMTab(driver, 'POST_STD_VM1')
            await driver.click(await driver.findElement('.aTab'))
            await clickCheckboxByText(driver, 'VM Virtual Cache Allocation Tech')
            await setNamedFormValue(driver, 'Maximum virtual CLOS', 4)
            await waitUntil(async () => (await currentScenarioData(driver)).vm
                .find(vm => vm.name === 'POST_STD_VM1').virtual_cat_support === 'y', {
                message: 'VM VCAT support to be enabled'
            })
            await selectVMTab(driver, 'Hypervisor')
            await driver.click(await driver.findElement('.aTab'))
            await waitUntil(async () => (await driver.bodyText()).includes('Memory Isolation for Performance'), {
                message: 'CAT settings'
            })
            if ((await currentScenarioData(driver)).hv.FEATURES.RDT.RDT_ENABLED !== 'y') {
                await clickCheckboxByText(driver, 'Cache Allocation Technology')
                await waitUntil(async () =>
                    (await currentScenarioData(driver)).hv.FEATURES.RDT.RDT_ENABLED === 'y', {
                    message: 'CAT to be enabled'
                })
            }
            await waitUntil(async () => (await driver.findElements('.slider-handle')).length > 0, {
                timeout: 5000,
                message: `CAT allocation controls: ${JSON.stringify(await driver.executeScript(`
                    return {
                        regions: document.querySelector('#app')?.__vue_app__?.config?.globalProperties?.$pinia?.state?.value?.scenario?.board?.CAT_INFO,
                        rdt: document.querySelector('#app')?.__vue_app__?.config?.globalProperties?.$pinia?.state?.value?.scenario?.doc?.hv?.FEATURES?.RDT,
                        text: document.querySelector('.tabContent').innerText.slice(-1200)
                    };
                `))}`
            })
            await clickCheckboxByText(driver, 'Cache Allocation Technology')
            await waitUntil(async () => await driver.executeScript(
                'return window.__e2eConfirmMessages.length === 1'), {message: 'CAT disable confirmation'})
            assert.equal((await currentScenarioData(driver)).hv.FEATURES.RDT.RDT_ENABLED, 'y')
            assert.equal(await checkboxCheckedByText(driver, 'Cache Allocation Technology'), true)

            await setDialogResponses(driver, {confirm: true})
            await clickCheckboxByText(driver, 'Cache Allocation Technology')
            await waitUntil(async () =>
                (await currentScenarioData(driver)).hv.FEATURES.RDT.RDT_ENABLED === 'n', {
                message: 'CAT to be disabled'
            })
            await waitUntil(async () =>
                !(await checkboxCheckedByText(driver, 'Cache Allocation Technology')), {
                message: 'CAT checkbox to be unchecked in the rendered form'
            })
            assert.equal((await driver.findElements('.slider-handle')).length, 0)
            await clickCheckboxByText(driver, 'Code and Data Prioritization')
            await waitUntil(async () =>
                (await currentScenarioData(driver)).hv.FEATURES.RDT.CDP_ENABLED === 'y', {
                message: 'CDP to be enabled'
            })
            await clickCheckboxByText(driver, 'Code and Data Prioritization')
            await waitUntil(async () =>
                (await currentScenarioData(driver)).hv.FEATURES.RDT.CDP_ENABLED === 'n', {
                message: 'CDP to be disabled'
            })
            await waitUntil(async () =>
                !(await checkboxCheckedByText(driver, 'Code and Data Prioritization')), {
                message: 'CDP checkbox to be unchecked in the rendered form'
            })

            // Disabling the parent while CDP is enabled must clear both the
            // domain switches and the rendered controls, not just the store.
            await clickCheckboxByText(driver, 'Code and Data Prioritization')
            await waitUntil(async () =>
                (await currentScenarioData(driver)).hv.FEATURES.RDT.CDP_ENABLED === 'y', {
                message: 'CDP to be re-enabled with CAT'
            })
            await clickCheckboxByText(driver, 'Cache Allocation Technology')
            await waitUntil(async () => {
                const rdt = (await currentScenarioData(driver)).hv.FEATURES.RDT
                return rdt.RDT_ENABLED === 'n' && rdt.CDP_ENABLED === 'n'
            }, {message: 'CAT and CDP to be disabled together'})
            await waitUntil(async () =>
                !(await checkboxCheckedByText(driver, 'Cache Allocation Technology')) &&
                !(await checkboxCheckedByText(driver, 'Code and Data Prioritization')), {
                message: 'CAT and CDP checkboxes to be unchecked together'
            })
            await clickCheckboxByText(driver, 'Virtual Cache Allocation Technology')
            await waitUntil(async () =>
                (await currentScenarioData(driver)).hv.FEATURES.RDT.VCAT_ENABLED === 'y', {
                message: 'VCAT to be enabled'
            })
            await waitUntil(async () => (await driver.bodyText()).includes('Virtual CAT'), {
                message: 'the VM VCAT allocation policy'
            })
            await clickByText(driver, 'button', 'Apply basic real-time defaults')
            const upper = await driver.findElement('.slider-handle-upper')
            const lower = await driver.findElement('.slider-handle-lower')
            await driver.executeScript(`
                document.querySelector('.slider-handle-upper')?.scrollIntoView({block: 'center'});
            `)
            await driver.dragBy(upper, 60, 0)
            await driver.dragTo(upper, lower)
            await new Promise(resolve => setTimeout(resolve, 150))
            await driver.dragBy(upper, 40, 0)
            await driver.pressKeys([''])

            const result = await saveScenario(driver)
            assert.ok(result.body?.includes('All files successfully saved'), JSON.stringify(result))
            const saved = await currentScenarioData(driver)
            assert.equal(saved.hv.FEATURES.RDT.RDT_ENABLED, 'y')
            assert.equal(saved.hv.FEATURES.RDT.VCAT_ENABLED, 'y')
            const allocations = saved.hv.CACHE_REGION.CACHE_ALLOCATION
            assert.ok(allocations.some(allocation => allocation.POLICY
                .some(policy => policy.VM === 'POST_RT_VM1')))
            assert.ok(allocations.some(allocation => allocation.POLICY
                .some(policy => policy.VM === 'POST_STD_VM1')))
            await closeVisibleModal(driver)
        } catch (error) {
            preserveTemporaryRoot = true
            await driver.screenshot(path.join(artifacts, 'cat-regression.png')).catch(() => {})
            console.error(`CAT diagnostics: ${JSON.stringify(await diagnosticMessages(driver))}`)
            throw error
        } finally {
            await restoreNativeDialogs(driver)
        }
    })

    test('reports when real-time CAT policies exceed the cache capacity', async () => {
        const fixture = bundledFixtures.find(item =>
            item.board === 'adl-asrock' && item.scenario === 'shared')
        const affinity = `<cpu_affinity>${Array.from({length: 10}, () =>
            '<pcpu><pcpu_id>8</pcpu_id><real_time_vcpu>y</real_time_vcpu></pcpu>').join('')}</cpu_affinity>`
        await stageFixture(temporaryRoot, historyDirectory, fixture, text => text
            .replace(/(<name>POST_RT_VM1<\/name>[\s\S]*?)<cpu_affinity>[\s\S]*?<\/cpu_affinity>/,
                `$1${affinity}`))
        await backToWelcome(driver)
        await openWorkingFolder(driver)
        await selectVMTab(driver, 'Hypervisor')
        await driver.click(await driver.findElement('.aTab'))
        await mockNativeDialogs(driver, {confirm: true})
        try {
            await clickCheckboxByText(driver, 'Cache Allocation Technology')
            await waitUntil(async () => (await driver.findElements('.slider-handle')).length > 0, {
                message: 'CAT controls with excess real-time policies'
            })
            await clickByText(driver, 'button', 'Apply basic real-time defaults')
            await waitUntil(async () => await driver.executeScript(
                'return window.__e2eAlerts.length === 1'), {message: 'the CAT capacity alert'})
            const alerts = await driver.executeScript('return window.__e2eAlerts')
            assert.ok(alerts[0].includes('too many realtime cpu'), JSON.stringify(alerts))
        } finally {
            await restoreNativeDialogs(driver)
        }
    })

    test('initializes CAT controls for a new Scenario', async () => {
        const fixture = bundledFixtures.find(item =>
            item.board === 'adl-asrock' && item.scenario === 'shared')
        await stageFixture(temporaryRoot, historyDirectory, fixture)
        await backToWelcome(driver)
        await openWorkingFolder(driver)
        await expandAccordion(driver, '2. Create new or import an existing scenario')
        await clickByText(driver, 'button', 'Create Scenario')
        await waitUntil(async () =>
            (await driver.findElements('#my-modal.show')).length === 1, {
            message: 'the new Scenario overwrite dialog'
        })
        await closeVisibleModal(driver)
        await waitUntil(async () =>
            (await driver.findElements('#newScenarioModal.show')).length === 1, {
            message: 'the new Scenario template dialog'
        })
        await clickByText(driver, '#newScenarioModal label', 'Shared')
        await closeVisibleModal(driver)
        await waitUntil(async () => (await currentScenarioData(driver))?.vm?.length === 2, {
            message: 'the new Shared Scenario'
        })
        await selectVMTab(driver, 'Hypervisor')
        await driver.click(await driver.findElement('.aTab'))
        await waitUntil(async () => (await driver.bodyText()).includes('Memory Isolation for Performance'), {
            message: 'CAT defaults for the new Scenario'
        })
        const rdt = (await currentScenarioData(driver)).hv.FEATURES.RDT
        assert.deepEqual(rdt, {RDT_ENABLED: 'n', CDP_ENABLED: 'n', VCAT_ENABLED: 'n'})
        await clickCheckboxByText(driver, 'Cache Allocation Technology')
        await waitUntil(async () =>
            (await currentScenarioData(driver)).hv.FEATURES.RDT.RDT_ENABLED === 'y', {
            message: 'CAT to be enabled for the new Scenario'
        })
        await waitUntil(async () => (await driver.findElements('.slider-handle')).length > 0, {
            message: 'CAT allocation controls without a real-time VM'
        })
        await clickByText(driver, 'button', 'Apply basic real-time defaults')
    })

    test('normalizes incompatible CAT switches and reports unsupported CDP on save', async () => {
        const fixture = bundledFixtures.find(item =>
            item.board === 'adl-asrock' && item.scenario === 'shared')
        const folder = await stageFixture(temporaryRoot, historyDirectory, fixture, text => text
            .replace('<CDP_ENABLED>n</CDP_ENABLED>', '<CDP_ENABLED>y</CDP_ENABLED>')
            .replace('<VCAT_ENABLED>n</VCAT_ENABLED>', '<VCAT_ENABLED>y</VCAT_ENABLED>'))
        const scenarioPath = path.join(folder, 'scenario.xml')
        const summaryPath = path.join(folder, 'config_summary.rst')
        const originalXML = await fs.readFile(scenarioPath, 'utf8')
        const previousSummary = 'Previous valid configuration summary\n'
        await fs.writeFile(summaryPath, previousSummary)
        await backToWelcome(driver)
        await openWorkingFolder(driver)
        await selectVMTab(driver, 'Hypervisor')
        await driver.click(await driver.findElement('.aTab'))
        await waitUntil(async () => {
            const rdt = (await currentScenarioData(driver)).hv.FEATURES.RDT
            return rdt.RDT_ENABLED === 'y' && rdt.CDP_ENABLED === 'y' && rdt.VCAT_ENABLED === 'n'
        }, {message: 'incompatible CAT switches to be normalized'})
        await waitUntil(async () => (await driver.findElements('.slider-handle')).length > 0, {
            message: 'Code and Data CAT allocation controls'
        })
        assert.ok((await driver.bodyText()).includes('_Code'))
        assert.ok((await driver.bodyText()).includes('_Data'))
        await clickByText(driver, 'button', 'Apply basic real-time defaults')
        const result = await saveScenario(driver)
        assert.ok(result.body?.includes('Settings validate failed'), result.body)
        assert.ok(result.body?.includes('Scenario xml saved'), result.body)
        assert.ok(!result.body?.includes('Scenario xml save failed'), result.body)
        assert.ok((await driver.bodyText()).includes('Code and Data Prioritization can only be opened'))
        const savedXML = await fs.readFile(scenarioPath, 'utf8')
        assert.notEqual(savedXML, originalXML, 'The rejected CDP configuration must still be saved')
        assert.match(savedXML, /<CDP_ENABLED>y<\/CDP_ENABLED>/)
        assert.equal(await fs.readFile(summaryPath, 'utf8'), previousSummary,
            'An invalid CDP Scenario must not update an existing configuration summary')
        await closeVisibleModal(driver)
    })

    for (const fixture of bundledFixtures) {
        test(`round-trips bundled ${fixture.board}/${fixture.scenario}.xml`, async () => {
            let loaded
            let expected
            let actual
            const artifactBase = `${fixture.board}-${fixture.scenario}`.replace(/[^a-zA-Z0-9_.-]/g, '_')
            try {
                await stageFixture(temporaryRoot, historyDirectory, fixture)
                await backToWelcome(driver)
                await openWorkingFolder(driver)
                loaded = await currentScenarioData(driver)
                assert.ok(loaded, 'Loaded scenario data is unavailable')

                const result = await saveScenario(driver)
                assert.equal(result.alert, undefined, `Bundled Scenario was rejected: ${result.alert}`)
                assert.ok(result.body?.includes('All files successfully saved'), JSON.stringify(result))
                expected = await currentScenarioData(driver)
                assert.ok(expected, 'Saved scenario data is unavailable')
                assertScenarioDataPreserved(expected, loaded)

                await closeVisibleModal(driver)
                await backToWelcome(driver)
                await openWorkingFolder(driver)
                actual = await currentScenarioData(driver)
                assert.deepEqual(actual, expected,
                    'Saved XML did not reload to semantically equivalent scenario data')
            } catch (error) {
                preserveTemporaryRoot = true
                if (expected) {
                    await fs.writeFile(path.join(artifacts, `${artifactBase}-expected.json`),
                        JSON.stringify(expected, null, 2))
                }
                if (actual) {
                    await fs.writeFile(path.join(artifacts, `${artifactBase}-actual.json`),
                        JSON.stringify(actual, null, 2))
                }
                await driver.screenshot(path.join(artifacts, `${artifactBase}-regression.png`)).catch(() => {})
                throw error
            }
        })
    }

    test('saves a loadable Scenario with duplicate VM names and reports the error', async () => {
        const source = bundledFixtures.find(fixture =>
            fixture.board === 'nuc11tnbi5' && fixture.scenario === 'hybrid_launch_2user_vm')
        assert.ok(source, 'The duplicate-name test fixture is missing')
        const fixture = {...source, scenario: 'duplicate-vm-name'}
        const folder = await stageFixture(temporaryRoot, historyDirectory, fixture, scenarioXML => {
            const invalidXML = scenarioXML.replace(
                '<name>POST_STD_VM2</name>', '<name>POST_STD_VM1</name>')
            assert.notEqual(invalidXML, scenarioXML, 'Failed to construct the invalid Scenario')
            return invalidXML
        })
        const scenarioPath = path.join(folder, 'scenario.xml')
        const originalXML = await fs.readFile(scenarioPath, 'utf8')
        await backToWelcome(driver)
        await openWorkingFolder(driver)

        const result = await saveScenario(driver)
        assert.ok(result.body?.includes('Multiple VMs have the same name: POST_STD_VM1'),
            `Expected a visible duplicate-name validation error, got: ${JSON.stringify(result)}`)
        assert.ok(result.body?.includes('Scenario xml saved'), result.body)
        assert.ok(result.body?.includes('Settings validate failed'), result.body)
        assert.ok(!result.body?.includes('Scenario xml save failed'), result.body)
        assert.notEqual(await fs.readFile(scenarioPath, 'utf8'), originalXML,
            'The duplicate-name Scenario must still be written')
        await assert.rejects(fs.access(path.join(folder, 'config_summary.rst')),
            'A duplicate-name Scenario must not generate a configuration summary')
        await closeVisibleModal(driver)
    })
})
