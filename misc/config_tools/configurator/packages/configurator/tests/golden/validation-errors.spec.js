import {describe, expect, it} from 'vitest'
import Config from '../../src/pages/Config.vue'

describe('golden validation error mapping', () => {
    it('buckets syntactic and semantic errors by HV and VM id', () => {
        const syntactic = {
            source: 'syntactic',
            message: 'bad HV setting',
            paths: ['/acrn-config/hv/DEBUG_OPTIONS/RELEASE']
        }
        const unindexedVm = {
            source: 'syntactic',
            message: 'bad first VM setting',
            paths: ['/acrn-config/vm/name']
        }
        const indexedVm = {
            source: 'semantic',
            message: 'bad second VM setting',
            paths: ['/acrn-config/vm[2]/cpu_affinity', '/acrn-config/vm[1]/name']
        }
        const repeatedVm = {
            source: 'semantic',
            message: 'duplicate paths',
            paths: ['/acrn-config/vm[3]/name', '/acrn-config/vm[3]/name']
        }
        const thirdVm = {
            source: 'semantic',
            message: 'bad third VM setting',
            paths: ['/acrn-config/vm[3]/name']
        }
        const scenario = {
            vm: [{'@id': 10}, {'@id': 20}, {'@id': 30}]
        }

        const translated = Config.methods.translateErrors({
            syntactic_errors: [syntactic, unindexedVm],
            semantic_errors: [indexedVm, repeatedVm, thirdVm]
        }, scenario)

        // Existing behavior treats vm[N] as one-based and deduplicates the same
        // error object when several of its paths land in the same VM bucket.
        expect(translated).toMatchSnapshot()
    })
})
