import {describe, expect, it, vi} from 'vitest'
import JSON2XML from '../src/lib/json2xml'

describe('JSON to XML conversion errors', () => {
    it('rejects values that are not objects', () => {
        const converter = new JSON2XML()

        expect(() => converter.convert('scenario')).toThrow('Not js object')
        expect(() => converter.convert(42)).toThrow('Not js object')
    })

    it('reports unsupported nested values', () => {
        const converter = new JSON2XML()
        const logged = vi.spyOn(console, 'log').mockImplementation(() => {})

        expect(() => converter.convert({'acrn-config': {hv: Symbol('unsupported')}})).toThrow('Unknown Object')

        logged.mockRestore()
    })

    it('skips null elements instead of failing', () => {
        const converter = new JSON2XML()

        expect(converter.convert({'acrn-config': {hv: null}})).toContain('<acrn-config>')
    })
})
