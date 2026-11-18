import {beforeEach, describe, expect, it, vi} from 'vitest'
import {createPinia, setActivePinia} from 'pinia'
import {useValidationStore} from '../../src/stores/validation'
import type {ValidationErrors} from '../../src/domain/validation/types'

function deferred<T>() {
    let resolve!: (value: T) => void
    let reject!: (reason?: unknown) => void
    const promise = new Promise<T>((resolvePromise, rejectPromise) => {
        resolve = resolvePromise
        reject = rejectPromise
    })
    return {promise, resolve, reject}
}

function validationErrors(): ValidationErrors {
    return {
        syntactic_errors: [],
        semantic_errors: [{
            source: 'semantic',
            message: 'invalid name',
            paths: ['/acrn-config/vm[1]/name']
        }]
    }
}

function makeRequest(
    validateScenario: (boardXml: string, scenarioXml: string) => ValidationErrors | Promise<ValidationErrors>,
    isCurrent: () => boolean = () => true
) {
    return {
        boardXml: '<board/>',
        scenarioXml: '<scenario/>',
        doc: {vm: [{'@id': 7}]},
        isCurrent,
        validateScenario
    }
}

beforeEach(() => {
    setActivePinia(createPinia())
})

describe('validation store', () => {
    it('maps and stores current validation results', async () => {
        const store = useValidationStore()
        const result = validationErrors()
        const validateScenario = vi.fn(() => result)

        await expect(store.validate(makeRequest(validateScenario))).resolves.toEqual({
            7: [result.semantic_errors[0]]
        })

        expect(validateScenario).toHaveBeenCalledWith('<board/>', '<scenario/>')
        expect(store.errors).toEqual({7: [result.semantic_errors[0]]})
        expect(store.revision).toBe(1)
    })

    it('discards a successful result after a newer validation revision', async () => {
        const store = useValidationStore()
        const pending = deferred<ValidationErrors>()
        const validateScenario = vi.fn(() => pending.promise)

        const validating = store.validate(makeRequest(validateScenario))
        store.clear()
        pending.resolve(validationErrors())

        await expect(validating).resolves.toBeNull()
        expect(store.errors).toEqual({})
        expect(store.revision).toBe(2)
    })

    it('discards a successful result when its scenario is no longer current', async () => {
        const store = useValidationStore()
        const isCurrent = vi.fn(() => false)

        await expect(store.validate(makeRequest(() => validationErrors(), isCurrent)))
            .resolves.toBeNull()

        expect(isCurrent).toHaveBeenCalledOnce()
        expect(store.errors).toEqual({})
        expect(store.revision).toBe(1)
    })

    it('suppresses a rejection after a newer validation revision', async () => {
        const store = useValidationStore()
        const pending = deferred<ValidationErrors>()

        const validating = store.validate(makeRequest(() => pending.promise))
        store.clear()
        pending.reject(new Error('stale validation failed'))

        await expect(validating).resolves.toBeNull()
        expect(store.errors).toEqual({})
        expect(store.revision).toBe(2)
    })

    it('suppresses a rejection when its scenario is no longer current', async () => {
        const store = useValidationStore()
        const isCurrent = vi.fn(() => false)

        await expect(store.validate(makeRequest(
            () => Promise.reject(new Error('old scenario failed')), isCurrent
        ))).resolves.toBeNull()

        expect(isCurrent).toHaveBeenCalledOnce()
        expect(store.errors).toEqual({})
    })

    it('propagates a current validation failure and preserves existing errors', async () => {
        const store = useValidationStore()
        const existing = [{message: 'existing', paths: []}]
        store.errors = {'-1': existing}
        const failure = new Error('validator unavailable')

        await expect(store.validate(makeRequest(() => Promise.reject(failure))))
            .rejects.toBe(failure)

        expect(store.errors).toEqual({'-1': existing})
        expect(store.revision).toBe(1)
    })
})
