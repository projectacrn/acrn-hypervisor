export interface ValidationIssue {
    source?: string
    message: string
    paths: string[]
    [key: string]: unknown
}

export interface ValidationErrors {
    syntactic_errors: ValidationIssue[]
    semantic_errors: ValidationIssue[]
}

export interface ValidationScenario {
    vm: Array<{
        '@id': number | string
        [key: string]: unknown
    }>
}

export type MappedValidationErrors = Record<string, ValidationIssue[]>
