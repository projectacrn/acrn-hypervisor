import {describe, expect, it} from 'vitest'
import {hexToRange, rangeToHex} from '../../src/domain/cat/masks'
// Lossy normalization is intentional here: current code counts and compacts
// non-contiguous bits and truncates masks wider than maxLength.

describe('golden CAT mask conversions', () => {
    it.each([
        ['wider mask with at least maxLength enabled bits', '0x1ff', 8],
        ['wider mask is reduced to its enabled-bit count', '0x101', 8],
        ['non-contiguous wider mask is made contiguous', '0x1a5', 8],
        ['narrower mask is left padded', '0x0f', 8],
        ['narrower mask without a prefix', 'f', 8],
        ['zero mask', '0x00', 8],
        ['all-ones mask', '0xff', 8],
        ['non-contiguous mask within maxLength', '0x81', 8],
        ['twelve-bit mask', '0x0abc', 12]
    ])('%s', (_label, mask, maxLength) => {
        expect({hexToRange: hexToRange(mask, maxLength)}).toMatchSnapshot()
    })

    it.each([
        ['zero-width range', [0, 0], 8],
        ['low range', [0, 4], 8],
        ['high range', [4, 8], 8],
        ['all bits', [0, 8], 8],
        ['twelve-bit range', [2, 10], 12]
    ])('%s', (_label, range, maxLength) => {
        expect({rangeToHex: rangeToHex(range, maxLength)}).toMatchSnapshot()
    })

    it.each(['0x00', '0x01', '0x0f', '0x3c', '0x7f', '0xff', '0x1ff', '0x1a5'])(
        'pins the current round-trip for %s', mask => {
            const range = hexToRange(mask, 8)
            const roundTrip = rangeToHex(range, 8)

            expect({mask, range, roundTrip}).toMatchSnapshot()
        })
})
