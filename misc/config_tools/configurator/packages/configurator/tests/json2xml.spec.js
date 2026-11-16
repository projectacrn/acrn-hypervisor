import {describe, expect, it} from 'vitest'
import JSON2XML from '../src/lib/json2xml'

describe('JSON2XML', () => {
    it('escapes attributes without modifying the input object', () => {
        const input = {
            'acrn-config': {
                '@board': 'board&"<',
                vm: [
                    {'@id': 1, name: 'VM & 1'}
                ]
            }
        }
        const original = structuredClone(input)
        const converter = new JSON2XML()

        const xml = converter.convert(input)

        expect(input).toEqual(original)
        expect(xml).toContain('<acrn-config board="board&amp;&quot;&lt;">')
        expect(xml).toContain('<vm id="1">')
        expect(xml).toContain('<name>VM &amp; 1</name>')
        expect(converter.convert(input)).toBe(xml)
    })
})
