export default function textFileResolver(fileRegex) {
    function compileFileToJS(src) {
        return src;
    }
    return {
        name: 'transform-xsd-file',
        transform(src, id) {
            if (fileRegex.test(id)) {
                return {
                    code: compileFileToJS(src),
                    map: null // 如果可行将提供 source map
                };
            }
        }
    };
}
//# sourceMappingURL=text-plugin.js.map