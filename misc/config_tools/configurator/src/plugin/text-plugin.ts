import _ from "lodash";
import * as fs from "fs";

export default function textFileResolver(fileRegex: RegExp) {
    function compileFileToJS(id: string, src: string) {
        if (_.endsWith(id, '.txt')) {
            let src = fs.readFileSync(id, {encoding: "utf-8"})
            return "export default " + JSON.stringify(src)
        }
        return "export default " + JSON.stringify(src)
    }

    return {
        name: 'text-file-resolver',
        transform(src: string, id: string) {
            if (fileRegex.test(id)) {
                return {
                    code: compileFileToJS(id, src),
                    map: null // 如果可行将提供 source map
                }
            }
        }
    }
}