function count(source: string, target: string): number {
    return (source.match(new RegExp(target, 'g')) || []).length
}

/**
 * Convert the legacy compact range representation to slider offsets.
 * The truncation, padding and non-contiguous-bit compaction are intentional.
 */
export function hexToRange(hexValue: string, maxValue: number): [number, number] {
    let strBin = Number.parseInt(hexValue, 16).toString(2)
    const blockLength = strBin.length
    const blockEnabledLength = count(strBin, '1')

    if (blockLength > maxValue) {
        if (blockEnabledLength >= maxValue) {
            strBin = '1'.repeat(maxValue)
        } else {
            strBin = '0'.repeat(maxValue - blockEnabledLength) + '1'.repeat(blockEnabledLength)
        }
    } else if (blockLength < maxValue) {
        strBin = '0'.repeat(maxValue - blockLength) + strBin
    }

    const start = strBin.indexOf('1') !== -1 ? strBin.indexOf('1') : 0
    const end = start + count(strBin, '1')
    return [start, end]
}

export function rangeToHex(value: [number, number], max: number): string {
    let newHexValue = '0'.repeat(value[0]) + '1'.repeat(value[1] - value[0]) + '0'.repeat(max - value[1])
    newHexValue = Number.parseInt(newHexValue, 2).toString(16)
    const zeroPadding = '0'.repeat(Number.parseInt('1'.repeat(max), 2).toString(16).length - newHexValue.length)
    return '0x' + zeroPadding + newHexValue
}
