<template>
  <div>
    <Slider v-model="hexField" :max="max" :tooltips="false" :options="sliderOptions"/>
  </div>
</template>

<script>
import Slider from '@vueform/slider'

function count(source, target) {
  return (source.match(new RegExp(target, 'g')) || []).length;
}

const wait = ms => new Promise(resolve => setTimeout(resolve, ms));

let getHexValue = (value, max) => {
  let newHexValue = '0'.repeat(value[0]) + '1'.repeat(value[1] - value[0]) + '0'.repeat(max - value[1])
  newHexValue = (parseInt(newHexValue, 2).toString(16))
  let zeroPadding = '0'.repeat(Number.parseInt('1'.repeat(max), 2).toString(16).length - newHexValue.length)
  newHexValue = '0x' + zeroPadding + newHexValue;
  return newHexValue;
}

export default {
  name: "HexBlockRangeSelector",
  components: {
    Slider,
  },
  computed: {
    sliderOptions() {
      let options = {}
      if (this.isVcat) {
        options['behaviour'] = 'drag-fixed'
      }
      return options
    },
    hexField: {
      get() {
        let str_bin = Number.parseInt(this.modelValue).toString(2);
        let block_length = str_bin.length;
        let block_enabled_length = count(str_bin, "1");

        let start = 0
        let end = 0

        if (block_length > this.max) {
          if (block_enabled_length >= this.max) {
            str_bin = "1".repeat(this.max);
          } else {
            str_bin = "0".repeat(this.max - block_enabled_length) + "1".repeat(block_enabled_length);
          }
        } else {
          if (block_length < this.max) {
            str_bin = "0".repeat(this.max - block_length) + str_bin;
          }
        }

        start = str_bin.indexOf("1") !== -1 ? str_bin.indexOf("1") : 0;
        end = start + count(str_bin, "1");

        // noinspection UnnecessaryLocalVariableJS
        let result = [start, end]
        return result

      },
      set(value) {
        if (value[1] - value[0] === 0) {
          wait(50).then(() => {
            this.hexField = this.lastValue
          })
        } else {
          this.lastValue = JSON.parse(JSON.stringify(value))
        }
        this.$emit("update:modelValue", getHexValue(value, this.max))
      }
    }
  },
  data() {
    return {lastValue: [0, 1]}
  },
  props: {
    isVcat: {
      type: Boolean
    },
    modelValue: {
      type: String
    },
    min: {
      type: Number,
      default: 0
    },
    max: {
      type: Number,
      default: 10
    }
  }
}
</script>

<style lang="scss">
:root {
  --slider-connect-bg: #ffd966;
  --slider-connect-bg-disabled: #bfbfbf;
}

@import "@vueform/slider/themes/default.css";

.slider-horizontal {
  height: 33px;
}

[disabled] .slider-handle {
  background: #bfbfbf !important;
}

.slider-base, .slider-connects {
  height: 30px;
  top: 1px;
  border-radius: unset;
  background: linear-gradient(to right, #c9d5ea 99%, white 0);
}

.slider-horizontal .slider-handle {
  transform: translate(51%, 0px);
  background: #ffd966;
  height: 29px;
  top: 1px;
  border: unset;
  border-radius: unset;
  box-shadow: unset;

  &.slider-handle-upper {
    transform: translate(-51%, 0px);
  }
}
</style>
<style>
.slider-base, .slider-connects {
  background-size: v-bind(` calc(100% / ${max}) 100% `);
}
</style>