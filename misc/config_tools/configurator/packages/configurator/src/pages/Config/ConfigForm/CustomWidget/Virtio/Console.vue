<template>
  <div class="virtio_consoles" v-if="defaultVal && defaultVal.length>0">
    <div class="virtio_console" v-for="(console, index) in defaultVal">
      <div class="virtio_console_demo">
        <b style="margin-bottom: 2rem">Virtio console device</b>
        <b-row class="align-items-center my-2 mt-4">
          <b-col md="2">
            <label>
              <n-popover trigger="hover" placement="top-start">
              <template #trigger>
                <IconInfo/>
              </template>
              <span v-html="this.ConsoleConfiguration.properties.use_type.description"></span>
              </n-popover>Use type:
            </label>
          </b-col>
          <b-col md="4">
            <b-form-select v-model="console.use_type" :options="getUseTypes"/>
          </b-col>
        </b-row>

        <b-row class="align-items-center">
          <b-col md="2">
            <label>
              <n-popover trigger="hover" placement="top-start">
              <template #trigger>
                <IconInfo/>
              </template>
              <span v-html="this.ConsoleConfiguration.properties.backend_type.description"></span>
              </n-popover>Backend type:
            </label>
          </b-col>
          <b-col md="4">
            <b-form-select v-model="console.backend_type" :options="ConsoleBackendType"/>
          </b-col>
        </b-row>

        <b-row class="align-items-center my-2" v-if="console.backend_type === 'file'">
          <b-col md="2">
            <label>
              <n-popover trigger="hover" placement="top-start">
              <template #trigger>
                <IconInfo/>
              </template>
              <span v-html="this.ConsoleConfiguration.properties.output_file_path.description"></span>
              </n-popover>Output file path:
            </label>
          </b-col>
          <b-col md="4">
            <b-form-input v-model="console.output_file_path"/>
          </b-col>
        </b-row>

        <b-row class="align-items-center my-2" v-else-if="console.backend_type === 'tty'">
          <b-col md="2">
            <label>
              <n-popover trigger="hover" placement="top-start">
              <template #trigger>
                <IconInfo/>
              </template>
              <span v-html="this.ConsoleConfiguration.properties.tty_device_path.description"></span>
              </n-popover>TTY device path:
            </label>
          </b-col>
          <b-col md="4">
            <b-form-input v-model="console.tty_device_path"/>
          </b-col>
        </b-row>

        <b-row class="align-items-center my-2" v-else-if="console.backend_type === 'sock client' || console.backend_type === 'sock server'">
          <b-col md="2">
            <label>
              <n-popover trigger="hover" placement="top-start">
              <template #trigger>
                <IconInfo/>
              </template>
              <span v-html="this.ConsoleConfiguration.properties.sock_file_path.description"></span>
              </n-popover>Sock file path:
            </label>
          </b-col>
          <b-col md="4">
            <b-form-input v-model="console.sock_file_path"/>
          </b-col>
        </b-row>

      </div>
      <div class="ToolSet">
        <div>
          <Icon size="18px" color="rgb(60,60,60)" @click="removeVirtioConsole(index)">
            <Minus/>
          </Icon>
        </div>
        <div>
          <Icon size="18px" color="rgb(60,60,60)" @click="addVirtioConsole">
            <Plus/>
          </Icon>
        </div>
      </div>

    </div>

  </div>
  <div v-else>
    <b style="margin-bottom: 2rem">Virtio console device</b>
    <div class="ToolSet">
      <div @click="addVirtioConsole">
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

export default {
  name: "Console",
  components: {Icon, Plus, Minus, IconInfo},
  props: {
    ...fieldProps,
    fieldProps: {
      type: null,
      default: null
    }
  },
  data() {
    return {
      ConsoleConfiguration: this.rootSchema.definitions['VirtioConsoleConfiguration'],
      enumNames: this.rootSchema.definitions['VirtioConsoleUseType']['enumNames'],
      enum: this.rootSchema.definitions['VirtioConsoleUseType']['enum'],
      ConsoleBackendType: this.rootSchema.definitions['BasicVirtioConsoleBackendType']['enum'],
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
  computed: {
    getUseTypes() {
      let enumOptions = []
      for (let i = 0; i < this.enumNames.length; i++) {
        let enumOption = {text: this.enumNames[i], value: this.enum[i]}
        enumOptions.push(enumOption)
      }
      return enumOptions
    },
  },
  methods: {
    removeVirtioConsole(index) {
      this.defaultVal.splice(index, 1);
    },
    addVirtioConsole() {
      if (!_.isArray(this.defaultVal)) {
        this.defaultVal = []
      }
      this.defaultVal.push({
        "use_type": "Virtio console",
        "backend_type": "stdio",
        "output_file_path": "",
        "sock_file_path": "",
        "tty_device_path": "",
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
  margin: 1rem;
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
.virtio_consoles{
  width: 100%;
}
.virtio_console {
  width: 100%;
  display: flex;
  gap: 2rem;
  align-items: end;
}

.virtio_console_demo {
  width: 100%;
  border: 2px solid #cccccc;
  padding: 12px 2px 12px 6px;
  border-radius: 5px;
  margin-bottom: 1rem;
}
</style>
