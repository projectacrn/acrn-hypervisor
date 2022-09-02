<template>
  <div class="VUART_SETTINGS" v-if="defaultVal && defaultVal.length>0">
    <div class="VUART_SETTING" v-for="(VUARTConn, index) in defaultVal">
      <div class="VUART_SETTING_CONTENT">
        <b style="margin-bottom: 2rem">Connection_{{ index + 1 }}</b>

        <b-row class="align-items-center my-2 mt-4">
          <b-col md="2">
            <label>
              <n-popover trigger="hover" placement="top-start">
                <template #trigger>
                  <IconInfo/>
                </template>
                <span v-html="this.VMConfigType.properties.name.description"></span>
              </n-popover>
              {{ vmNameTitle }}
            </label>
          </b-col>
          <b-col md="4">
            <b-form-select :state="validation(VUARTConn.endpoint[0].vm_name)" v-model="VUARTConn.endpoint[0].vm_name"
                           :options="vmNames"
                           @input="selectChange(index,VUARTConn.endpoint[0].vm_name,VUARTConn.endpoint[0],0)"></b-form-select>
            <b-form-invalid-feedback>
              must have value
            </b-form-invalid-feedback>
          </b-col>
        </b-row>

        <b-row class="align-items-center my-2">
          <b-col md="2">
            Connected to:
          </b-col>
        </b-row>

        <b-row class="align-items-center my-2">
          <b-col md="2">
            <label>
              <n-popover trigger="hover" placement="top-start">
                <template #trigger>
                  <IconInfo/>
                </template>
                <span v-html="this.VMConfigType.properties.name.description"></span>
              </n-popover>
              {{ vmNameTitle }}:
            </label>
          </b-col>
          <b-col md="4">
            <b-form-select :state="validation(VUARTConn.endpoint[1].vm_name)" v-model="VUARTConn.endpoint[1].vm_name"
                           :options="vmNames"
                           @input="selectChange(index,VUARTConn.endpoint[1].vm_name,VUARTConn.endpoint[1],1)"></b-form-select>
            <b-form-invalid-feedback>
              must have value
            </b-form-invalid-feedback>
          </b-col>
        </b-row>

        <b-row class="align-items-center my-2">
          <b-col md="2">
            <label>
              <n-popover trigger="hover" placement="top-start">
                <template #trigger>
                  <IconInfo/>
                </template>
                <span v-html="this.VuartConnectionType.properties.type.description"></span>
              </n-popover>
              {{ vuartConnectionTypeTitle }}:
            </label>
          </b-col>
          <b-col md="4">
            <b-form-select :state="validation(VUARTConn.type)" v-model="VUARTConn.type"
                           :options="VuartType"></b-form-select>
            <b-form-invalid-feedback>
              must have value
            </b-form-invalid-feedback>
          </b-col>
        </b-row>

        <div class="m-3 mt-4 d-flex flex-column gap-2">
          <b>Advanced Virtual UART configurations (OPTIONAL):</b>
          <p></p>
          <b-row class="justify-content-sm-start">
            <b-col sm="4">
              {{ vuartEndpointTitle }}:
            </b-col>
            <b-col sm="4" v-if="VUARTConn.type === 'legacy'">
              <n-popover trigger="hover" placement="top-start">
                <template #trigger>
                  <IconInfo/>
                </template>
                <span v-html="this.VuartEndpointType.io_port.description"></span>
              </n-popover>
              {{ vuartVIoPortTitle }}:
            </b-col>
            <b-col sm="4" v-else-if="VUARTConn.type === 'pci'">
              <n-popover trigger="hover" placement="top-start">
                <template #trigger>
                  <IconInfo/>
                </template>
                <span v-html="this.VuartEndpointType.vbdf.description"></span>
              </n-popover>
              {{ vuartVBDFTitle }}
            </b-col>
          </b-row>
          <b-row class="justify-content-sm-start align-items-center">
            <b-col sm="4"> Connection_{{ index + 1 }}-{{ VUARTConn.endpoint[0].vm_name }}</b-col>
            <b-col sm="4">
              <b-form-input v-model="VUARTConn.endpoint[0].io_port" v-if="VUARTConn.type === 'legacy'"
                            :placeholder="vIoPortPlaceholder"/>
              <b-form-input :state="validateVBDF(VUARTConn.endpoint[0].vbdf)" v-model="VUARTConn.endpoint[0].vbdf"
                            v-else-if="VUARTConn.type === 'pci'" :placeholder="vBDFPlaceholder"/>
              <b-form-invalid-feedback>
                {{ this.VBDFType["err:pattern"] }}
              </b-form-invalid-feedback>
            </b-col>
          </b-row>
          <b-row class="justify-content-sm-start align-items-center">
            <b-col sm="4"> Connection_{{ index + 1 }}-{{ VUARTConn.endpoint[1].vm_name }}</b-col>
            <b-col sm="4">
              <b-form-input v-model="VUARTConn.endpoint[1].io_port" v-if="VUARTConn.type === 'legacy'"
                            :placeholder="vIoPortPlaceholder"/>
              <b-form-input :state="validateVBDF(VUARTConn.endpoint[1].vbdf)" v-model="VUARTConn.endpoint[1].vbdf"
                            v-else-if="VUARTConn.type === 'pci'" :placeholder="vBDFPlaceholder"/>
              <b-form-invalid-feedback>
                {{ this.VBDFType["err:pattern"] }}
              </b-form-invalid-feedback>
            </b-col>
          </b-row>
        </div>
      </div>
      <div class="ToolSet">
        <div @click="removeVUARTConnection(index)">
          <Icon size="18px">
            <Minus/>
          </Icon>
        </div>
        <div @click="addVUARTConnection(index)">
          <Icon size="18px">
            <Plus/>
          </Icon>
        </div>

      </div>
    </div>

  </div>
  <div v-else>
    <div class="ToolSet">
      <div @click="addVUARTConnection">
        <Icon size="18px">
          <Plus/>
        </Icon>
      </div>
    </div>
  </div>
</template>

<script>
import _ from 'lodash';
import {Icon} from "@vicons/utils";
import {Plus, Minus} from '@vicons/fa'
import {fieldProps, vueUtils} from '@lljj/vue3-form-naive';
import IconInfo from '@lljj/vjsf-utils/icons/IconInfo.vue';


export default {
  name: 'VUART',
  components: {Icon, Plus, Minus, IconInfo},
  props: {
    ...fieldProps,
    // Todo: use ui:fieldProps to pass getScenarioData function
    fieldProps: {
      type: null,
      default: null
    }
  },
  computed: {
    vmNames() {
      let currentScenarioData = window.getCurrentScenarioData()
      let vmNames = []
      for (let i = 0; i < currentScenarioData.vm.length; i++) {
        vmNames.push(currentScenarioData.vm[i].name)
      }
      return vmNames
    }
  },
  data() {
    let enumValue = this.rootSchema.definitions['VuartType']['enum']
    let enumNames = this.rootSchema.definitions['VuartType']['enumNames']
    let VuartType = []
    enumValue.forEach((item, i) => {
      VuartType.push({value: item, text: enumNames[i]})
    })
    let epTypeProp = this.rootSchema.definitions.VuartEndpointType.properties
    let conTypeProp = this.rootSchema.definitions.VuartConnectionType.properties
    return {
      selected: {0: {0: {oldValue: ""}, 1: {oldValue: ""}}},
      VuartEndpointType: this.rootSchema.definitions['VuartEndpointType']['properties'],
      vmNameTitle: epTypeProp.vm_name.title,
      vuartConnectionTypeTitle: conTypeProp.type.title,
      vuartEndpointTitle: conTypeProp.endpoint['title'],
      vuartVIoPortTitle: epTypeProp.io_port.title,
      vuartVBDFTitle: epTypeProp.vbdf.title,
      vIoPortPlaceholder: epTypeProp.io_port['ui:options']['placeholder'],
      vBDFPlaceholder: epTypeProp.vbdf['ui:options']['placeholder'],
      VuartType,
      IOPortDefault: this.rootSchema.definitions['VuartEndpointType']['properties']['io_port']['default'],
      VMConfigType: this.rootSchema.definitions['VMConfigType'],
      VuartConnectionType: this.rootSchema.definitions['VuartConnectionType'],
      VBDFType: this.rootSchema.definitions['VBDFType'],
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
    validation(value) {
      return (value != null) && (value.length != 0);
    },
    validateVBDF(value) {
      var regexp = new RegExp(this.VBDFType.pattern);
      return (value != null) && regexp.test(value);
    },
    removeVUARTConnection(index) {
      this.defaultVal.splice(index, 1);
    },
    addVUARTConnection(index) {
      if (!_.isArray(this.defaultVal)) {
        this.defaultVal = []
      }
      this.defaultVal.splice(index + 1, 0, {
        "name": "Connection_" + this.defaultVal.length,
        "type": "pci",
        "endpoint": [
          {
            "vm_name": "",
            "io_port": "",
            "vbdf": "",
          },
          {
            "vm_name": "",
            "io_port": "",
            "vbdf": "",
          }
        ]
      })
    },
    selectChange(index, value, obj, endpoint) {
      if (this.selected[index]) {
        if (this.selected[index][endpoint].oldValue != value) {
          obj.vbdf = ""
          obj.io_port = ""
        }
      } else {
        this.selected[index] = {0: {oldValue: ""}, 1: {oldValue: ""}}
        obj.vbdf = ""
        obj.io_port = ""
      }
      this.selected[index][endpoint]["oldValue"] = value
    },
  }
};
</script>

<style scoped>
label:before {
  content: '*';
  color: red;
  margin-right: 3px;
}

.form-control {
  line-height: 2;
}

.VUART_SETTINGS {
  display: flex;
  flex-direction: column;
  width: 100%;
  gap: 2rem;
}

.VUART_SETTING {
  display: flex;
  align-items: start;
  gap: 1rem;
  width: 100%;
}

.VUART_SETTING_CONTENT {
  border: 1px solid gray;
  border-radius: 5px;
  padding: 25px;
  width: 100%;
}

.ToolSet {
  display: flex;
  justify-content: space-around;
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
