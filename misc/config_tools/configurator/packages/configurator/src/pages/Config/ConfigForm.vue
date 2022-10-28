<template>
  <div style="position: relative">
    <b-button
        variant="primary"
        size="lg"
        class="deleteVM"
        @click="deleteVM"
        :class="{'d-none':currentActiveVMID===-1}"
    >
      <Icon>
        <Minus/>
      </Icon>
      Delete VM
    </b-button>
  </div>

  <div class="abTabs">
    <div class="bTab" @click="this.currentFormMode='BasicConfigType'"
         :class="{Active:this.currentFormMode==='BasicConfigType'}">
      Basic Parameters
    </div>
    <div class="aTab" @click="this.currentFormMode='AdvancedConfigType'"
         :class="{Active:this.currentFormMode==='AdvancedConfigType'}">
      Advanced Parameters
    </div>
  </div>
  <div class="tabContent">
    <div class="notice">
      <text style="color: red">*</text>
      are required fields
    </div>
    <VueForm
        :key="(currentActiveVMID===-1?'HV':`VM${currentActiveVMID}`)+currentFormMode+'ConfigForm'"
        v-model="currentFormData"
        :form-props="formProps"
        :ui-schema="uiSchema"
        :schema="currentFormSchema[currentFormMode]"
        @change="dataChange"
    >
      <template></template>
    </VueForm>
  </div>
</template>

<script>
import VueForm, {i18n} from "@lljj/vue3-form-naive"
import {Icon} from "@vicons/utils";
import {Minus} from "@vicons/fa"
import localizeEn from 'ajv-i18n/localize/en';
import IVSHMEM_REGION from "./ConfigForm/CustomWidget/IVSHMEM_REGION.vue";
import cpu_affinity from "./ConfigForm/CustomWidget/cpu_affinity.vue";
import VUART from "./ConfigForm/CustomWidget/VUART.vue";
import Network from "./ConfigForm/CustomWidget/Virtio/Network.vue";
import Console from "./ConfigForm/CustomWidget/Virtio/Console.vue";
import Input from "./ConfigForm/CustomWidget/Virtio/Input.vue";
import CAT from "./ConfigForm/CustomWidget/CAT.vue";
import GPU from "./ConfigForm/CustomWidget/Virtio/GPU.vue";

i18n.useLocal(localizeEn);

export default {
  name: "ConfigForm",
  components: {Icon, Minus, VueForm, CAT, VUART, cpu_affinity, IVSHMEM_REGION},
  emits: ['scenarioConfigFormDataUpdate', 'deleteVM', 'vmNameChange', 'updateCpuAffinity'],
  props: {
    currentActiveVMID: {type: Number},
    currentFormSchema: {type: Object},
    currentFormData: {type: Object}
  },
  data() {
    return {
      vmNameCache: '',
      currentFormMode: 'BasicConfigType',
      formProps: {
        "inline": false,
        "inlineFooter": false,
        "labelPosition": "left",
        "labelWidth": "300px",
        "labelSuffix": "："
      },
      uiSchema: {
        "virtio_devices": {
          "network": {
            "ui:title": "",
            'ui:field': Network,
          },
          "console": {
            "ui:title": "",
            'ui:field': Console,
          },
          "input": {
            "ui:title": "",
            'ui:field': Input,
          },
          "gpu": {
            "ui:title": "",
            "ui:field": GPU,
          },
        },
        "cpu_affinity": {
          'ui:field': cpu_affinity
        },
        CACHE_REGION: {
          "ui:field": CAT
        },
        "FEATURES": {
          "IVSHMEM": {
            "ui:title": "InterVM shared memory",
            "IVSHMEM_REGION": {
              "ui:title": "",
              "ui:sortable": false,
              "ui:field": IVSHMEM_REGION,
            }
          }
        },
        "vuart_connections": {
          "ui:title": "InterVM Virtual UART Connection",
          "vuart_connection": {
            "ui:sortable": false,
            "ui:field": VUART,
          }
        }
      }
    };
  },
  methods: {
    deleteVM() {
      this.$emit('deleteVM')
    },
    dataChange({newValue, oldValue}) {
      let newID = newValue.hasOwnProperty('@id') ? newValue['@id'] : -1;
      let oldID = oldValue.hasOwnProperty('@id') ? oldValue['@id'] : -1;
      let vmType = newValue.hasOwnProperty('vm_type') ? newValue['vm_type'] : "";

      if (newID === oldID) {
        // skip hv settings
        if (newID !== -1) {
          if (this.vmNameCache !== newValue.name) {
            this.$emit('vmNameChange', newValue.name, this.vmNameCache)
            this.vmNameCache = newValue.name;
          }
          if (vmType != 'RTVM') {
            this.$emit('updateCpuAffinity', newID)
          }
        }
        this.$emit('scenarioConfigFormDataUpdate', newID, newValue)
      } else {
        if (newID !== -1) {
          this.vmNameCache = newValue.name
        }
      }
    }
  }
}
</script>

<style lang="scss">
.nav-tabs .nav-link {
  /*noinspection CssNoGenericFontName*/
  font-family: Roboto;
  font-style: normal;
  font-weight: 500;
  font-size: 22px;
  line-height: 26px;

  background: #E1F2EF;
  border: 1px solid #007B81;
  box-sizing: border-box;
  box-shadow: inset 0 -1px 0 #D7D7D7;
  border-radius: 5px 5px 0 0;
  border-bottom: none;

  color: #007B81;
  margin-right: 4px;
  padding: 1rem 1.5rem;
}

.nav-tabs .nav-link.active {
  mix-blend-mode: normal;
  border: 1px solid #373A77;
  background: #007B81;
  border-bottom: none;
  box-sizing: border-box;
  border-radius: 5px 5px 0 0;


  font-weight: 600;

  color: white;
}

.tab-pane {
  margin-top: -2px;
  border: 1px solid rgb(55, 58, 119);
  box-sizing: border-box;
  border-radius: 0 5px 5px;
  padding: 2rem;
}

.nav-tabs {
  border-bottom: none;
}

.deleteVM {
  height: 52px;
  max-width: 210px;
  width: 100%;
  position: absolute;
  right: 0;
}

.n-popover {
  max-width: 60%;
}

.abTabs {
  display: flex;
  gap: 3px;
  z-index: 1;

  & .aTab, & .bTab {
    cursor: pointer;
    border: 1px solid #007B81;
    border-bottom: none;
    border-radius: 5px 5px 0 0;
    background: #E1F2EF;
    padding: 1rem;
    color: #007B81;
    font-size: 1.2rem;

    &.Active {
      background: #007B81;
      border: 1px solid #373A77;
      border-bottom: none;
      color: white;
      font-weight: 800;
    }
  }
}


.tabContent {
  border: 1px solid purple;
  padding: 1rem;
  background: white;
}

.notice {
  padding: 1rem 0;
}
</style>