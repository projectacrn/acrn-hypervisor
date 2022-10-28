<template>
  <div><br><b>Virtio GPU device</b></div>
  <div><br><span v-html="this.GPUDescription" style="color: rgb(153, 153, 153)"></span></div>
  <div class="virtio_gpu" v-if="defaultVal && defaultVal.length>0">
    <div class="virtio_gpu_demo" v-for="(virtio_gpu, index) in defaultVal">
      <b style="margin-bottom: 2rem">Virtual displays</b>
      <b-row class="align-items-center my-2 mt-4">
        <b-col md="2">
          <label>
            <n-popover trigger="hover" placement="top-start">
              <template #trigger>
                <IconInfo/>
              </template>
              <span v-html="this.GPUConfiguration.properties.display_type.description"></span>
            </n-popover>
            Display type:
          </label>
        </b-col>
        <b-col md="4">
          <b-form-select v-model="virtio_gpu.display_type" :options="GPUDisplayType"/>
        </b-col>
      </b-row>
      <div class="align-items-center my-2 mt-4" v-if="virtio_gpu.display_type === 'Window'"
           v-for="(window_display, window_index) in virtio_gpu.displays.display">
        <div style="float: left; text-indent: 2em;">
          <b>Display {{ window_index + 1 }}</b>
        </div>
        <div class="ToolSet" style="float: right;">
          <div @click="removeDisplay(virtio_gpu.displays.display, window_index)">
            <Icon size="18px">
              <Minus/>
            </Icon>
          </div>
          <div @click="addWindowDisplay(virtio_gpu.displays.display, window_index)">
            <Icon size="18px">
              <Plus/>
            </Icon>
          </div>
        </div>
        <br>
        <div class="windowDisplay" style="text-indent: 1em;">
          <b-row class="align-items-center my-2 mt-4">
            <b-col md="3">
              <label>
                <n-popover trigger="hover" placement="top-start">
                  <template #trigger>
                    <IconInfo/>
                  </template>
                  <span v-html="this.DisplayConfiguration.properties.window_resolutions.description"></span>
                </n-popover>
                Window resolutions:
              </label>
            </b-col>
            <b-col md="4">
              <b-form-select v-model="window_display.window_resolutions" :options="WindowResolutionsType"/>
            </b-col>
          </b-row>
          <b-row class="align-items-center my-2">
            <b-col md="3">
              <label>
                <n-popover trigger="hover" placement="top-start">
                  <template #trigger>
                    <IconInfo/>
                  </template>
                  <span v-html="this.DisplayConfiguration.properties.horizontal_offset.description"></span>
                </n-popover>
                Horizontal offset (pixels):
              </label>
            </b-col>
            <b-col md="4">
              <b-form-input v-model="window_display.horizontal_offset"/>
            </b-col>
          </b-row>
          <b-row class="align-items-center my-2">
            <b-col md="3">
              <label>
                <n-popover trigger="hover" placement="top-start">
                  <template #trigger>
                    <IconInfo/>
                  </template>
                  <span v-html="this.DisplayConfiguration.properties.vertical_offset.description"></span>
                </n-popover>
                Vertical offset (pixels):
              </label>
            </b-col>
            <b-col md="4">
              <b-form-input v-model="window_display.vertical_offset"/>
            </b-col>
          </b-row>
        </div>
      </div>

      <div class="align-items-center my-2 mt-4" v-else-if="virtio_gpu.display_type === 'Full screen'"
           v-for="(fullScreen_display, fullScreen_display_index) in virtio_gpu.displays.display">
        <div style="float: left; text-indent: 2em;">
          <b>Display {{ index + 1 }}</b>
        </div>
        <div class="ToolSet" style="float: right;">
          <div @click="removeDisplay(virtio_gpu.displays.display, fullScreen_display_index)">
            <Icon size="18px">
              <Minus/>
            </Icon>
          </div>
          <div @click="addFullScreenDisplay(virtio_gpu.displays.display, fullScreen_display_index)">
            <Icon size="18px">
              <Plus/>
            </Icon>
          </div>
        </div>
        <br>
        <b-row style="text-indent: 1em" class="align-items-center my-2 mt-4">
          <b-col md="2">
            <label>
              <n-popover trigger="hover" placement="top-start">
                <template #trigger>
                  <IconInfo/>
                </template>
                <span v-html="this.DisplayConfiguration.properties.monitor_id.description"></span>
              </n-popover>
              Monitor id:
            </label>
          </b-col>
          <b-col md="4">
            <b-form-select v-model="fullScreen_display.monitor_id" :options="MonitorIdType"/>
          </b-col>
        </b-row>
      </div>
    </div>

    <div class="ToolSet">
      <div>
        <Icon size="18px" color="rgb(60,60,60)" @click="removeVirtioGPU">
          <Minus/>
        </Icon>
      </div>
    </div>
  </div>

  <div v-else>
    <div class="ToolSet" style="margin-inline-start: 1rem; margin-bottom: 1rem">
      <div @click="addVirtioGPU">
        <Icon size="18px">
          <Plus/>
        </Icon>
      </div>
    </div>
  </div>
</template>

<script>
import _ from "lodash";
import {Icon} from "@vicons/utils";
import {Plus, Minus} from '@vicons/fa'
import {fieldProps, vueUtils} from '@lljj/vue3-form-naive';
import IconInfo from '@lljj/vjsf-utils/icons/IconInfo.vue';
import {BRow} from "bootstrap-vue-3";

export default {
  name: "GPU",
  components: {BRow, Icon, Plus, Minus, IconInfo},
  props: {
    ...fieldProps,
    fieldProps: {
      type: null,
      default: null
    }
  },
  data() {
    return {
      GPUDescription: this.rootSchema.definitions['VMConfigType']['properties']['virtio_devices']['properties']['gpu']['description'],
      GPUConfiguration: this.rootSchema.definitions['VirtioGPUConfiguration'],
      GPUDisplayType: this.rootSchema.definitions['VirtioGPUDisplayType']['enum'],
      DisplayConfiguration: this.rootSchema.definitions['DisplayConfiguration'],
      WindowResolutionsType: this.rootSchema.definitions['WindowResolutionsType']['enum'],
      MonitorIdType: this.rootSchema.definitions['DisplayConfiguration']['properties']['monitor_id']['enum'],
      defaultVal: vueUtils.getPathVal(this.rootFormData, this.curNodePath)
    };
  },
  watch: {
    rootFormData: {
      handler(newValue, oldValue) {
        this.defaultVal = vueUtils.getPathVal(newValue, this.curNodePath)
      },
      deep: true
    },
    defaultVal: {
      handler(newValue, oldValue) {
        // Note: `newValue` will be equal to `oldValue` here
        // on nested mutations as long as the object itself
        // hasn't been replaced.
        vueUtils.setPathVal(this.rootFormData, this.curNodePath, newValue);
      },
      deep: true
    }
  },
  methods: {
    addWindowDisplay(display, index) {
      display.splice(index + 1, 0, {
        "window_resolutions": "",
        "horizontal_offset": "",
        "vertical_offset": "",
      })
    },
    addFullScreenDisplay(display, index) {
      display.splice(index + 1, 0, {
        "monitor_id": "",
      })
    },
    removeDisplay(display, index) {
      if (display.length <= 1) {
        return
      }
      display.splice(index, 1)
    },
    removeVirtioGPU() {
      this.defaultVal = []
    },
    addVirtioGPU() {
      if (!_.isArray(this.defaultVal)) {
        this.defaultVal = []
      }
      this.defaultVal.splice(1, 0, {
        "display_type": "",
        "displays": {
          "display": [
            {
              "window_resolutions": "",
              "horizontal_offset": "",
              "vertical_offset": "",
              "monitor_id": "",
            }
          ]
        }
      })
    }
  }
}
</script>

<style scoped>
.ToolSet {
  display: flex;
  flex-direction: row;
  justify-content: space-around;
  gap: 0.5rem;
  max-width: 5rem;
}

.ToolSet div {
  cursor: pointer;
  border: 1px solid gray;
  border-radius: 3px;
  background: #f9f9f9;
  padding: 5px 5px 3px;
}

.virtio_gpu {
  display: flex;
  gap: 2rem;
  align-items: end;
}

.virtio_gpu_demo {
  width: 100%;
  border: 2px solid #cccccc;
  padding: 12px 50px 12px 6px;
  border-radius: 5px;
  margin-bottom: 1rem;
}
</style>
