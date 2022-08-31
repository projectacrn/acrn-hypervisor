<template>
  <div style="border:1px solid gray;background: #F5F5F5;padding: 1rem">
    <b>{{ uiOptions.title }}</b>
    <div class="p-4">
      <div v-if="defaultVal.pcpu && defaultVal.pcpu.length>0">
        <b-row v-if="is_std_vm">
          <label>
            <n-popover trigger="hover" placement="top-start" style="width: 500px">
              <template #trigger>
                <IconInfo/>
              </template>
              <span> Enable a VM exclusively owns the physical CPUs assigned to it. </span>
            </n-popover>
          Exclusively owns physical CPUs: </label>
          <b-col>own_pcpu</b-col>
          <b-col>
            <b-form-checkbox v-model="pcpu_owned" :value="'y'" :uncheckedValue="'n'"
              @click="click_own_pcpu"
            >
            </b-form-checkbox>
          </b-col>
          <b-col></b-col>
          <b-col></b-col>
          <b-col></b-col>
        </b-row>
        <b-row>
          <b-col></b-col>
          <b-col></b-col>
          <b-col>Virtual CPU ID</b-col>
          <b-col class="ps-5">
            <label>
              <n-popover trigger="hover" placement="top-start" style="width: 500px">
                <template #trigger>
                  <IconInfo/>
                </template>
                <span v-html="this.CPUAffinityConfiguration.properties.real_time_vcpu.description"> </span>
              </n-popover>
              Real-time vCPU:</label>
          </b-col>
          <b-col></b-col>
        </b-row>
        <b-row class="align-items-center"
               v-for="(cpu,index) in defaultVal.pcpu">
          <b-col>
            <label class="requiredField" v-if="index===0"></label>
            <label>
              <n-popover trigger="hover" placement="top-start">
                <template #trigger>
                  <IconInfo/>
                </template>
                <span v-html="this.CPUAffinityConfiguration.properties.pcpu_id.description"></span>
              </n-popover>
              pCPU ID
            </label>
          </b-col>
          <b-col>
            <b-form-select :state="validateCPUAffinity(cpu.pcpu_id)" v-model="cpu.pcpu_id">
              <option v-for="pcpu in pcpuid_enum.enum" :value="pcpu">
                {{pcpuid_enum.enumNames[pcpuid_enum.enum.indexOf(pcpu)]}}
              </option>
            </b-form-select>
            <b-form-invalid-feedback>
              pCPU ID is required!
            </b-form-invalid-feedback>
          </b-col>
          <b-col class="p-3 col-3">
            <div style="padding:9px;border-radius: 9px;width: 100%;border: 1px solid dimgray;background: lightgray;">
              {{ vCPUName(index) }}
            </div>
          </b-col>
          <b-col class="p-3">
            <b-form-checkbox v-model="cpu.real_time_vcpu" :value="'y'" :uncheckedValue="'n'" :disabled="!isRTVM"/>
          </b-col>
          <b-col>
            <div class="ToolSet">
              <div @click="removePCPU(index)">
                <Icon size="18px">
                  <Minus/>
                </Icon>
              </div>
              <div @click="addPCPU(index)" :class="{'d-none': (this.defaultVal.pcpu.length-1)!==index}">
                <Icon size="18px">
                  <Plus/>
                </Icon>
              </div>
            </div>
          </b-col>
        </b-row>
      </div>
      <b-row v-else>
        <b-col>
          <div class="ToolSet">
            <div @click="addPCPU(-1)">
              <Icon size="18px">
                <Plus/>
              </Icon>
            </div>
          </div>
        </b-col>
      </b-row>
    </div>
  </div>
</template>

<script>
import {
  fieldProps,
  vueUtils,
  formUtils,
  schemaValidate
} from '@lljj/vue3-form-naive';
import _ from 'lodash'
import {Icon} from "@vicons/utils";
import {Plus, Minus} from '@vicons/fa'
import {BFormInput, BRow} from "bootstrap-vue-3";
import IconInfo from '@lljj/vjsf-utils/icons/IconInfo.vue';

export default {
  name: "cpu_affinity",
  components: {BRow, BFormInput, Icon, Plus, Minus, IconInfo},
  props: {
    ...fieldProps,
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
        vueUtils.setPathVal(this.rootFormData, this.curNodePath, newValue);
      },
      deep: true
    }
  },
  computed: {
    isRTVM() {
      return vueUtils.getPathVal(this.rootFormData, 'vm_type') === 'RTVM'
    },
    is_std_vm() {
      return vueUtils.getPathVal(this.rootFormData, 'vm_type') === 'STANDARD_VM'
    },
    pcpuid_enum() {
      return window.getCurrentFormSchemaData().BasicConfigType.definitions.CPUAffinityConfiguration.properties.pcpu_id
    },
    pcpu_owned() {
      return vueUtils.getPathVal(this.rootFormData, 'own_pcpu')
    },
    uiOptions() {
      return formUtils.getUiOptions({
        schema: this.schema,
        uiSchema: this.uiSchema
      });
    },
  },
  data() {
    return {
      CPUAffinityConfiguration: this.rootSchema.definitions['CPUAffinityConfiguration'],
      defaultVal: vueUtils.getPathVal(this.rootFormData, this.curNodePath)
    }
  },
  mounted() {
    if (!_.isArray(this.defaultVal.pcpu)) {
      this.defaultVal.pcpu = []
      this.addPCPU(-1)
    }
  },
  methods: {
    validateCPUAffinity(pcpu_id) {
      return _.isNumber(pcpu_id) && this.pcpuid_enum.enum.indexOf(pcpu_id) >= 0
    },
    vCPUName(index) {
      return `${this.rootFormData.name} vCPU ${index}`
    },
    addPCPU(index) {
      this.defaultVal.pcpu.splice(index + 1, 0, {pcpu_id: null, real_time_vcpu: "n"})
    },
    click_own_pcpu() {
      let newValue = this.pcpu_owned === 'y' ? 'n' : 'y';
      vueUtils.setPathVal(this.rootFormData, 'own_pcpu', newValue)
    },
    removePCPU(index) {
      if (this.defaultVal.pcpu.length === 1) {
        // prevent delete for the last one
        return
      }
      this.defaultVal.pcpu.splice(index, 1)
    }
  }
}
</script>

<style scoped>
.requiredField:before {
  content: '*';
  color: red;
}

.ToolSet {
  display: flex;
  justify-content: end;
  gap: 0.5rem;
  max-width: 5rem;
  width: 100%;
}

.ToolSet div {
  cursor: pointer;
  border: 1px solid gray;
  border-radius: 3px;
  background: #f9f9f9;
  padding: 5px 5px 3px;
}
</style>
