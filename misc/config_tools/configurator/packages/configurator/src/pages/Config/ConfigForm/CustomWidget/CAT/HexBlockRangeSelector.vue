<template>
  <div>
    <Slider v-model="hexField" :merge="-1" :max="max" :tooltips="false"/>
  </div>
</template>

<script>
import Slider from '@vueform/slider'

export default {
  name: "HexBlockRangeSelector",
  components: {
    Slider,
  },
  computed: {
    hexField: {
      get() {
        let strBin = parseInt(this.modelValue).toString(2)
        let rangeStart = 0;
        for (let i = strBin.length - 1; i >= 0; i--) {
          if (strBin[i] === '0') {
            rangeStart++
          } else {
            break
          }
        }
        let rangeEnd = strBin.length;
        return [this.max - rangeEnd, this.max - rangeStart]
      },
      set(value) {
        if (value[0] - value[1] === 0) {
          this.hexField = this.lastVal;
          return;
        }
        this.lastVal = value;
        let result = ""
        for (let i = 0; i < this.max; i++) {
          let tmp = null;
          if (i < value[0]) {
            tmp = '0'
          } else if (i < value[1]) {
            tmp = '1'
          } else if (i >= value[1]) {
            tmp = '0'
          } else {
            debugger
            throw new Error('???')
          }
          result += tmp;
        }
        console.log(result)
        result = '0x' + (parseInt(result, 2).toString(16))
        console.log(result)
        this.$emit("update:modelValue", result);
      }
    }
  },
  data() {
    return {lastVal: [0, 1]}
  },
  props: {
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