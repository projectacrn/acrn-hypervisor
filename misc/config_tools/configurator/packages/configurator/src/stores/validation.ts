import {defineStore} from 'pinia'
import {mapErrors} from '../domain/validation/mapErrors'
import type {
    MappedValidationErrors,
    ValidationErrors,
    ValidationScenario,
} from '../domain/validation/types'

interface ValidationRequest {
    boardXml: string
    scenarioXml: string
    doc: ValidationScenario
    isCurrent: () => boolean
    validateScenario: (boardXml: string, scenarioXml: string) => ValidationErrors | Promise<ValidationErrors>
}

export const useValidationStore = defineStore('validation', {
    state: () => ({
        errors: {} as MappedValidationErrors,
        revision: 0,
    }),
    actions: {
        clear(): void {
            this.revision++
            this.errors = {}
        },
        apply(errors: ValidationErrors, doc: ValidationScenario): MappedValidationErrors {
            this.errors = mapErrors(errors, doc)
            return this.errors
        },
        async validate(request: ValidationRequest): Promise<MappedValidationErrors | null> {
            const revision = ++this.revision
            try {
                const result = await request.validateScenario(request.boardXml, request.scenarioXml)
                if (revision !== this.revision || !request.isCurrent()) {
                    return null
                }
                return this.apply(result, request.doc)
            } catch (error) {
                if (revision !== this.revision || !request.isCurrent()) {
                    return null
                }
                throw error
            }
        },
    },
})
