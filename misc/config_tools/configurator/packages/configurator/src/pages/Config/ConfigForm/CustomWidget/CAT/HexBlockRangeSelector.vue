<template>
  <div>
    <Slider v-model="hexField" :max="max" :tooltips="false" :options="sliderOptions"/>
  </div>
</template>

<script>
import Slider from '@vueform/slider'
import configurator from '../../../../../lib/acrn'


const wait = ms => new Promise(resolve => setTimeout(resolve, ms));





export default {
  name: "HexBlockRangeSelector",
  components: {
    Slider,
  },
  computed: {
    sliderOptions() {
      let options = {}
      // if (this.isVcat) {
      //   options['behaviour'] = 'drag-fixed'
      // }
      return options
    },
    hexField: {
      get() {
        return configurator.cat.hexToRange(this.modelValue, this.max)
      },
      set(value) {
        if (value[1] - value[0] === 0) {
          wait(50).then(() => {
            this.hexField = this.lastValue
          })
        } else {
          this.lastValue = JSON.parse(JSON.stringify(value))
        }
        this.$emit("update:modelValue", configurator.cat.rangeToHex(value, this.max))
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