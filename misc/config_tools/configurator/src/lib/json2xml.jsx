import _ from 'lodash'

export default class JSON2XML {
    constructor(indent = '  ', newline = '\n') {
        this.xml = ''
        this.indent = indent
        this.newline = newline
    }

    convert = (jsObject) => {
        return this.handleConvert(jsObject)
    }

    handleConvert(jsObject) {
        if (!(_.isObject(jsObject) || _.isArray(jsObject))) {
            throw new Error('Not js object')
        }
        this.xml = '<?xml version="1.0" encoding="utf-8"?>'
        for (const jsObjectKey in jsObject) {
            this.#convertObject(0, jsObjectKey, jsObject[jsObjectKey])
        }
        return this.xml
    }

    #getAttrText(jso) {
        let attrs = {}
        Object.keys(jso).map((childKey) => {
                if (_.startsWith(childKey, '@')) {
                    attrs[childKey.substr(1)] = jso[childKey]
                    delete jso[childKey]
                }
            }
        )
        let attrText = ''
        for (const attrsKey in attrs) {
            attrText += ` ${attrsKey}="${attrs[attrsKey]}"`
        }
        return attrText
    }

    addNewlineAndIndent(deepth) {
        this.xml += this.newline + _.repeat(this.indent, deepth)
    }

    #convertObject(deepth, selfName, jsObjectElement) {
        // this.#convertObject('acrn-config',jso['acrn-config'])
        // let example = {
        //     'acrn-config': {
        //         '@board': 'whl-ipc-i7',
        //         hv: {},
        //         vm: [
        //             {'@id': 1},
        //             {'@id': 2},
        //             {'@id': 3}
        //         ]
        //     }
        // }

        if (_.isString(jsObjectElement) || _.isNumber(jsObjectElement)) {
            // this.#convertObject('boot_args', 'default')
            this.addNewlineAndIndent(deepth)
            let val = _.escape(`${jsObjectElement}`)
            this.xml += `<${selfName}>${val}</${selfName}>`
        } else if (_.isArray(jsObjectElement)) {
            // this.#convertObject('cpuid', [1,2,3])
            // this.#convertObject('vm', [{},{},{}])
            for (const index in jsObjectElement) {
                this.#convertObject(deepth, selfName, jsObjectElement[index])
            }
        } else if (_.isObject(jsObjectElement)) {
            // this.#convertObject('acrn-config',jso['acrn-config'])
            let attrText = this.#getAttrText(jsObjectElement)
            this.addNewlineAndIndent(deepth)
            this.xml += `<${selfName}${attrText}>`
            for (const key in jsObjectElement) {
                this.#convertObject(deepth + 1, key, jsObjectElement[key])
            }
            this.addNewlineAndIndent(deepth)
            this.xml += `</${selfName}>`
        } else if (jsObjectElement == null) {
            //pass
        } else {
            console.log(jsObjectElement)
            debugger;
            throw new Error("Unknown Object")
        }
    }
}