# ACRN Configurator WASM Python Module

Every file must set `__package__ = 'configurator.pyodide'` before import,
set this magic var can resolve python relative import error when we direct run it.

## Function define

Every python script need a test function and a main function.

### test

run script will call this function,
so please set script default params in this function

### main

in js side will use this function.
like:

```javascript
// after pyodide install all dependices
var launch_cfg_gen = pyodide.pyimport("configurator.pyodide.launch_cfg_gen").main;
var board_xml = this.readFile('xxxx/board.xml');
var scenario_xml = this.readFile('xxx/scenario.xml');
var launch_scripts = launch_cfg_gen(board_xml, scenario_xml);
console.log(launch_scripts)
```
