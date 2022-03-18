import _ from "lodash";
import xml2json, {parseXml} from "./xml2json";
import JSON2XML from "./json2xml";

import {fs, path} from "@tauri-apps/api"


// noinspection JSUnresolvedFunction,JSUnresolvedVariable
class BackendBase {
    listConfig = async () => {
        await this.init

        let config = await this.read(this.configPath)
        if (config == null) {
            return {}
        }
        try {
            config = JSON.parse(config)
        } catch (e) {
            return {}
        }
        return config
    }

    getConfig = async (key, defaultValue) => {
        let config = await this.listConfig()
        console.log(key, config, defaultValue)
        if (config.hasOwnProperty(key)) {
            return config[key]
        }
        return defaultValue
    }

    setConfig = async (key, value) => {
        let config = await this.listConfig()
        config[key] = value
        config = JSON.stringify(config)
        await this.write(this.configPath, config)
    }

    read = async () => {
        throw new Error('Not Impl')
    }
    write = async () => {
        throw new Error('Not Impl')
    }
    list = async () => {
        throw new Error('Not Impl')
    }
    remove = async () => {
        throw new Error('Not Impl')
    }
}

export class LocalStorageBackend extends BackendBase {
    constructor() {
        super()
        this.rootFSName = 'acrn-configurator'
        this.configPath = 'config.json'
    }

    _readRootFS = () => {
        let localStorageFS = localStorage.getItem(this.rootFSName)
        if (localStorageFS == null) {
            return {}
        }
        localStorageFS = JSON.parse(localStorageFS)
        return localStorageFS
    }

    _writeRootFS = (fs) => {
        localStorage.setItem(this.rootFSName, JSON.stringify(fs))
    }

    read = async (filePath) => {
        let localStorageFS = this._readRootFS()
        if (localStorageFS.hasOwnProperty(filePath)) {
            return localStorageFS[filePath]
        }
        return null
    }

    write = async (filePath, value) => {
        let localStorageFS = this._readRootFS()
        localStorageFS[filePath] = value
        this._writeRootFS(localStorageFS)
    }
    list = async (dirPath) => {
        let localStorageFS = this._readRootFS()
        let dirFiles = []
        for (let filepath in localStorageFS) {
            if (_.startsWith(filepath, dirPath)) {
                dirFiles.push(filepath)
            }
        }
        return dirFiles
    }
    remove = async (filename) => {
        let localStorageFS = this._readRootFS()
        if (localStorageFS.hasOwnProperty(filename)) {
            delete localStorageFS[filename]
        }
    }
}

export class TauriLocalFSBackend extends BackendBase {
    constructor() {
        super()
        this.init = path.homeDir()
            .then(async (dirPath) => {
                this.configDir = await path.join(dirPath, '.acrn-configurator')
                this.configPath = await path.join(this.configDir, 'config.json')
                return fs.readDir(this.configDir)
                    .catch(async () => {
                        fs.createDir(this.configDir, {recursive: true})
                            .then(() => this.write(this.configPath, "{}"))
                            .catch((reason) => {
                                alert("Create config dir failed. Error:" + reason)
                            })
                    })
            })
    }


    read = async (filePath) => {
        await this.init
        return await fs.readTextFile(filePath)
            .catch(reason => {
                // Todo: add isFile function to check perm
                // alert("Read file error! Error: " + reason);
                return null
            })
    }

    write = async (filePath, value) => {
        await this.init
        return await fs.writeFile({
            path: filePath,
            contents: value
        }).catch((reason => alert("Write file error! Error: " + reason)))
    }

    list = async (dirPath) => {
        await this.init
        return (await fs.readDir(dirPath))
            .filter((file) => file.children == null)
            .map((file) => file.path)
    }

    remove = async (filePath) => {
        await this.init
        return await fs.removeFile(filePath)
    }

}

export class Helper {
    constructor(configBackend, fsBackend) {
        this.configBackend = configBackend
        this.fsBackend = fsBackend
        this.color = {
            blue: "#1475b2",
            green: "#42c02e",
            deepGray: "#606060"
        }
    }

    log(title, content, backgroundColor = this.color.green) {
        console.log(
            "%c ".concat(title, " %c ").concat(content, " "),
            "padding: 1px; border-radius: 3px 0 0 3px; color: #fff; background: ".concat(this.color.deepGray, ";"),
            "padding: 1px; border-radius: 0 3px 3px 0; color: #fff; background: ".concat(backgroundColor, ";")
        )
    }

    resolveHome = async (filepath) => {
        if (filepath[0] === '~') {
            return await path.join(await path.homeDir(), filepath.slice(1))
        }
        return filepath;
    }

    convertXMLTextToObj = (XMLText) => {
        let jsonText = xml2json(parseXml(XMLText))
        // console.log(scenarioData)
        return JSON.parse(jsonText)
    }

    convertObjToXML = (o) => {
        // convert our js object to xml
        let json2xml = new JSON2XML()
        // noinspection UnnecessaryLocalVariableJS
        let xml = json2xml.convert(o)
        return xml
    }

    read = async filePath => {
        return await this.fsBackend.read(filePath)
    }

    save = async (filePath, text) => {
        await this.fsBackend.write(filePath, text)
    };

    list = async dirPath => {
        return await this.fsBackend.list(dirPath)
    };

    remove = async filePath => {
        await this.fsBackend.remove(filePath).catch((reason => alert("Remove file failed! Error:" + reason)))
    }

    getConfig = async (configKeyName, defaultValue) => {
        return await this.configBackend.getConfig(configKeyName, defaultValue)
    }

    setConfig = async (configKeyName, value) => {
        return await this.configBackend.setConfig(configKeyName, value)
    };

}
