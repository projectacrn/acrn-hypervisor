<template>
  <div class="IVSH_REGIONS" v-if="defaultVal && defaultVal.length>0">
    <div class="IVSH_REGION" v-for="(IVSHMEM_VMO, index) in defaultVal">
      <div class="IVSH_REGION_CONTENT">
        <b style="margin-bottom: 2rem">InterVM shared memory region</b>

        <b-row class="align-items-center my-2 mt-4">
          <b-col md="2">
            <label>
              <n-popover trigger="hover" placement="top-start">
              <template #trigger>
                  <IconInfo/>
              </template>
              <span v-html="this.IVSHMEMRegionType.properties.NAME.description"></span>
            </n-popover>Region name:
            </label>
          </b-col>
          <b-col md="4">
              <b-form-input :state="validateIvshrgName(IVSHMEM_VMO.NAME)" v-model="IVSHMEM_VMO.NAME" placeholder="Must be 1-27 characters and only letters, digits and '_'."/>
              <b-form-invalid-feedback>
                Name must be between 1-27 characters and contain only letters, digits and "_".
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
                <span v-html="this.IVSHMEMRegionType.properties.PROVIDED_BY.description"></span>
              </n-popover>Emulated by:
            </label>
          </b-col>
          <b-col md="4">
            <b-form-select v-model="IVSHMEM_VMO.PROVIDED_BY" :options="providerType"></b-form-select>
          </b-col>
        </b-row>

        <b-row class="align-items-center my-2">
          <b-col md="2">
            <label>
              <n-popover trigger="hover" placement="top-start">
                <template #trigger>
                    <IconInfo/>
                </template>
                <span v-html="this.IVSHMEMRegionType.properties.IVSHMEM_SIZE.description"></span>
              </n-popover>Size (MB):
            </label>
          </b-col>
          <b-col md="4">
            <b-form-select v-model="IVSHMEM_VMO.IVSHMEM_SIZE" :options="IVSHMEMSize"></b-form-select>
          </b-col>
        </b-row>

        <div class="m-3 mt-4 d-flex flex-column gap-2">
          <b>Shared VMs</b>
          <p>Select all VMs that will use this shared memory region</p>
          <b-row>
            <b-col sm="2" offset-sm="6">
              <n-popover trigger="hover" placement="top-start">
                <template #trigger>
                    <IconInfo/>
                </template>
                <span v-html="this.IVSHMEM_VM.properties.VBDF.description"></span>
              </n-popover>Virtual BDF:
            </b-col>
          </b-row>
          <b-row class="justify-content-between align-items-center"
                 v-for="(IVSHMEM_VM,index) in IVSHMEM_VMO.IVSHMEM_VMS.IVSHMEM_VM">
            <b-col sm="2">
              <label>
                <n-popover trigger="hover" placement="top-start">
                  <template #trigger>
                      <IconInfo/>
                  </template>
                  <span v-html="this.IVSHMEM_VM.properties.VM_NAME.description"></span>
                </n-popover>VM name:
              </label>
            </b-col>
            <b-col sm="3">
              <b-form-select :state="validation(IVSHMEM_VM.VM_NAME)" v-model="IVSHMEM_VM.VM_NAME" :options="vmNames"></b-form-select>
              <b-form-invalid-feedback>
                must have value
              </b-form-invalid-feedback>
            </b-col>
            <b-col sm="3">
              <b-form-input :state="validateVBDF(IVSHMEM_VM.VBDF)" v-model="IVSHMEM_VM.VBDF" placeholder="00:[device].[function], e.g. 00:0c.0. All fields are in hexadecimal."/>
              <b-form-invalid-feedback>
                {{ this.VBDFType["err:pattern"] }}
              </b-form-invalid-feedback>
            </b-col>
            <b-col sm="3">
              <div class="ToolSet">
                <div @click="removeSharedVM(IVSHMEM_VMO.IVSHMEM_VMS.IVSHMEM_VM,index)">
                  <Icon size="18px">
                    <Minus/>
                  </Icon>
                </div>
                <div @click="addSharedVM(IVSHMEM_VMO.IVSHMEM_VMS.IVSHMEM_VM,index)">
                  <Icon size="18px">
                    <Plus/>
                  </Icon>
                </div>
              </div>
            </b-col>
          </b-row>

        </div>
      </div>
      <div class="ToolSet">
        <div @click="removeIVSHMEM_VMO(index)">
          <Icon size="18px">
            <Minus/>
          </Icon>
        </div>
        <div @click="addIVSHMEM_VMO(index)">
          <Icon size="18px">
            <Plus/>
          </Icon>
        </div>
      </div>

    </div>

  </div>
  <div v-else>
    <div class="ToolSet">
      <div @click="addIVSHMEM_VMO">
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
  name: 'IVSHMEM_REGION',
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
    return {
      providerType: this.rootSchema.definitions['ProviderType']['enum'],
      IVSHMEMSize: this.rootSchema.definitions['IVSHMEMSize']['enum'],
      IVSHMEMRegionType: this.rootSchema.definitions['IVSHMEMRegionType'],
      IVSHMEM_VM: this.rootSchema.definitions['IVSHMEMVM'],
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
    validateIvshrgName(value) {
      var regexp = new RegExp(this.IVSHMEMRegionType.properties.NAME.pattern);
      return (value != null) && regexp.test(value);
    },
    validateVBDF(value) {
      var regexp = new RegExp(this.VBDFType.pattern);
      return (value != null) && regexp.test(value);
    },
    validation(value) {
      return (value != null) && (value.length !== 0);
    },
    addSharedVM(vms, index) {
      // add new item after current item
      vms.splice(index + 1, 0, {
        "VM_NAME": "",
        "VBDF": ""
      })
    },
    removeSharedVM(vms, index) {
      if (vms.length <= 2) {
        return
      }
      vms.splice(index, 1)
    },
    removeIVSHMEM_VMO(index) {
      this.defaultVal.splice(index, 1);
    },
    addIVSHMEM_VMO(index) {
      if (!_.isArray(this.defaultVal)) {
        this.defaultVal = []
      }
      this.defaultVal.splice(index + 1, 0, {
        "NAME": "",
        "PROVIDED_BY": "Hypervisor",
        "IVSHMEM_SIZE": "2",
        "IVSHMEM_VMS": {
          "IVSHMEM_VM": [
            {
              "VM_NAME": "",
              "VBDF": ""
            },
            {
              "VM_NAME": "",
              "VBDF": ""
            }
          ]
        }
      })
    }
  }
};
</script>

<!--suppress CssUnusedSymbol -->
<style scoped>
label:before {
  content: '*';
  color: red;
  margin-right: 3px;
}

.form-control {
  line-height: 2;
}

.IVSH_REGIONS {
  display: flex;
  flex-direction: column;
  width: 100%;
  gap: 2rem;
}

.IVSH_REGION {
  display: flex;
  align-items: start;
  gap: 1rem;
  width: 100%;
}

.IVSH_REGION_CONTENT {
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
