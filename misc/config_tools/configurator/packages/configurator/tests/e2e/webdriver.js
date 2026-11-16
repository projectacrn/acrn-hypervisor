import assert from 'node:assert/strict'

const ELEMENT_KEY = 'element-6066-11e4-a52e-4f735466cecf'

export class WebDriverClient {
    constructor(baseURL, sessionID) {
        this.baseURL = baseURL
        this.sessionID = sessionID
    }

    static async create({port, application}) {
        const baseURL = `http://127.0.0.1:${port}`
        const response = await WebDriverClient.request(baseURL, 'POST', '/session', {
            capabilities: {
                alwaysMatch: {
                    'tauri:options': {application}
                }
            }
        })
        const sessionID = response.sessionId || response.value?.sessionId
        assert.ok(sessionID, `WebDriver did not return a session ID: ${JSON.stringify(response)}`)
        return new WebDriverClient(baseURL, sessionID)
    }

    static async request(baseURL, method, requestPath, body) {
        const response = await fetch(baseURL + requestPath, {
            method,
            headers: {'Content-Type': 'application/json'},
            body: body === undefined ? undefined : JSON.stringify(body),
            signal: AbortSignal.timeout(30000)
        })
        const text = await response.text()
        let payload
        try {
            payload = text ? JSON.parse(text) : {}
        } catch {
            throw new Error(`WebDriver returned non-JSON ${response.status}: ${text}`)
        }
        if (!response.ok || payload.value?.error) {
            throw new Error(`WebDriver ${method} ${requestPath} failed: ${text}`)
        }
        return payload
    }

    request(method, requestPath, body) {
        return WebDriverClient.request(
            this.baseURL,
            method,
            `/session/${this.sessionID}${requestPath}`,
            body
        )
    }

    async findElement(selector) {
        const response = await this.request('POST', '/element', {
            using: 'css selector', value: selector
        })
        return response.value[ELEMENT_KEY]
    }

    async findElements(selector) {
        const response = await this.request('POST', '/elements', {
            using: 'css selector', value: selector
        })
        return response.value.map(element => element[ELEMENT_KEY])
    }

    async findElementWithin(element, selector) {
        const response = await this.request('POST', `/element/${element}/element`, {
            using: 'css selector', value: selector
        })
        return response.value[ELEMENT_KEY]
    }

    async findElementsWithin(element, selector) {
        const response = await this.request('POST', `/element/${element}/elements`, {
            using: 'css selector', value: selector
        })
        return response.value.map(child => child[ELEMENT_KEY])
    }

    async click(element) {
        await this.request('POST', `/element/${element}/click`, {})
    }

    async clear(element) {
        await this.request('POST', `/element/${element}/clear`, {})
    }

    async type(element, text) {
        await this.request('POST', `/element/${element}/value`, {
            text, value: Array.from(text)
        })
    }

    async pressKeys(keys) {
        const actions = []
        for (const key of keys) {
            actions.push({type: 'keyDown', value: key}, {type: 'keyUp', value: key})
        }
        await this.request('POST', '/actions', {
            actions: [{type: 'key', id: 'keyboard', actions}]
        })
        await this.request('DELETE', '/actions')
    }

    async dragBy(element, x, y) {
        const origin = {[ELEMENT_KEY]: element}
        await this.request('POST', '/actions', {
            actions: [{
                type: 'pointer', id: 'mouse', parameters: {pointerType: 'mouse'},
                actions: [
                    {type: 'pointerMove', duration: 0, origin, x: 0, y: 0},
                    {type: 'pointerDown', button: 0},
                    {type: 'pointerMove', duration: 250, origin, x, y},
                    {type: 'pointerUp', button: 0}
                ]
            }]
        })
        await this.request('DELETE', '/actions')
    }

    async dragTo(element, target) {
        const origin = {[ELEMENT_KEY]: element}
        const destination = {[ELEMENT_KEY]: target}
        await this.request('POST', '/actions', {
            actions: [{
                type: 'pointer', id: 'mouse', parameters: {pointerType: 'mouse'},
                actions: [
                    {type: 'pointerMove', duration: 0, origin, x: 0, y: 0},
                    {type: 'pointerDown', button: 0},
                    {type: 'pointerMove', duration: 250, origin: destination, x: 0, y: 0},
                    {type: 'pointerUp', button: 0}
                ]
            }]
        })
        await this.request('DELETE', '/actions')
    }

    async text(element) {
        const response = await this.request('GET', `/element/${element}/text`)
        return response.value
    }

    async attribute(element, name) {
        const response = await this.request('GET', `/element/${element}/attribute/${name}`)
        return response.value
    }

    async property(element, name) {
        const response = await this.request('GET', `/element/${element}/property/${name}`)
        return response.value
    }

    async displayed(element) {
        const response = await this.request('GET', `/element/${element}/displayed`)
        return response.value
    }

    async bodyText() {
        return this.text(await this.findElement('body'))
    }

    async back() {
        await this.request('POST', '/back', {})
    }

    async alertText() {
        try {
            const response = await this.request('GET', '/alert/text')
            return response.value
        } catch (error) {
            if (error.message.includes('no such alert')) {
                return null
            }
            throw error
        }
    }

    async acceptAlert() {
        await this.request('POST', '/alert/accept', {})
    }

    async executeScript(script, args = []) {
        const response = await this.request('POST', '/execute/sync', {script, args})
        return response.value
    }

    async screenshot(destination) {
        const {writeFile} = await import('node:fs/promises')
        const response = await this.request('GET', '/screenshot')
        await writeFile(destination, Buffer.from(response.value, 'base64'))
    }

    async close() {
        if (this.sessionID) {
            await this.request('DELETE', '')
            this.sessionID = null
        }
    }
}

export async function waitUntil(callback, {timeout = 30000, interval = 250, message = 'condition'} = {}) {
    const deadline = Date.now() + timeout
    let lastError
    while (Date.now() < deadline) {
        try {
            const result = await callback()
            if (result) {
                return result
            }
        } catch (error) {
            lastError = error
        }
        await new Promise(resolve => setTimeout(resolve, interval))
    }
    throw new Error(`Timed out waiting for ${message}${lastError ? `: ${lastError.message}` : ''}`)
}
