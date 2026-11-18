import type {MappedValidationErrors, ValidationErrors, ValidationIssue, ValidationScenario} from './types'

export function mapErrors(errors: ValidationErrors, scenarioData: ValidationScenario): MappedValidationErrors {
    const formErrors: MappedValidationErrors = {}

    const translate = (error: ValidationIssue) => {
        error.paths.forEach(path => {
            const formPath = path.split('/')[2]
            let vmid: number | string = -1
            if (formPath === 'hv') {
                vmid = -1
            } else if (formPath === 'vm') {
                vmid = scenarioData.vm[0]['@id']
            } else if (/vm\[\d+]/.test(formPath)) {
                const match = /vm\[(\d+)]/.exec(formPath)
                const vmIndex = Number.parseInt(match![1], 10) - 1
                vmid = scenarioData.vm[vmIndex]['@id']
            }
            if (!Object.prototype.hasOwnProperty.call(formErrors, vmid)) {
                formErrors[vmid] = []
            }
            if (!formErrors[vmid].includes(error)) {
                formErrors[vmid].push(error)
            }
        })
    }

    errors.syntactic_errors.forEach(translate)
    errors.semantic_errors.forEach(translate)
    return formErrors
}
