<template>
  <div class="VUART_SETTINGS" v-if="defaultVal && defaultVal.length>0">
    <div class="VUART_SETTING" v-for="(VUARTConn, index) in defaultVal">
      <div class="VUART_SETTING_CONTENT">
        <b style="margin-bottom: 2rem">Connection_{{ index + 1 }}</b>

        <b-row class="align-items-center my-2 mt-4">
          <b-col md="2">
            <label>VM name: </label>
          </b-col>
          <b-col md="4">
            <b-form-select v-model="VUARTConn.endpoint[0].vm_name" :options="vmNames"></b-form-select>
          </b-col>
        </b-row>

        <b-row class="align-items-center my-2">
          <b-col md="2">
            Connected to:
          </b-col>
        </b-row>

        <b-row class="align-items-center my-2">
          <b-col md="2">
            <label>VM name: </label>
          </b-col>
          <b-col md="4">
            <b-form-select v-model="VUARTConn.endpoint[1].vm_name" :options="vmNames"></b-form-select>
          </b-col>
        </b-row>

        <b-row class="align-items-center my-2">
          <b-col md="2">
            <label>Type: </label>
          </b-col>
          <b-col md="4">
            <b-form-select v-model="VUARTConn.type" :options="VuartType"></b-form-select>
          </b-col>
        </b-row>

        <div class="m-3 mt-4 d-flex flex-column gap-2">
          <b>Advanced Virtual UART configurations (OPTIONAL):</b>
          <p></p>
          <b-row class="justify-content-sm-start">
            <b-col sm="4">
              Virtual UART port:
            </b-col>
            <b-col sm="4" v-if="VUARTConn.type === 'legacy'"> I/O address: </b-col>
            <b-col sm="4" v-else-if="VUARTConn.type === 'pci'"> VBDF </b-col>
          </b-row>
          <b-row class="justify-content-sm-start align-items-center">
            <b-col sm="4"> Connection_{{ index }}-{{ VUARTConn.endpoint[0].vm_name }} </b-col>
            <b-col sm="4">
              <b-form-input v-model="VUARTConn.endpoint[0].io_port" v-if="VUARTConn.type === 'legacy'"/>
              <b-form-input v-model="VUARTConn.endpoint[0].vbdf" v-else-if="VUARTConn.type === 'pci'"/>
            </b-col>
          </b-row>
          <b-row class="justify-content-sm-start align-items-center">
            <b-col sm="4"> Connection_{{ index }}-{{ VUARTConn.endpoint[1].vm_name }} </b-col>
            <b-col sm="4">
              <b-form-input v-model="VUARTConn.endpoint[1].io_port" v-if="VUARTConn.type === 'legacy'"/>
              <b-form-input v-model="VUARTConn.endpoint[1].vbdf" v-else-if="VUARTConn.type === 'pci'"/>
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


export default {
  name: 'VUART',
  components: {Icon, Plus, Minus},
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
      VuartType: this.rootSchema.definitions['VuartType']['enum'],
      IOPortDefault: this.rootSchema.definitions['VuartEndpointType']['properties']['io_port']['default'],
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
    removeVUARTConnection(index) {
      this.defaultVal.splice(index, 1);
    },
    addVUARTConnection(index) {
      if (!_.isArray(this.defaultVal)) {
        this.defaultVal = []
      }
      this.defaultVal.splice(index + 1, 0, {
        "name": "Connection_" + this.defaultVal.length,
        "type": "legacy",
        "endpoint": [
          {
            "vm_name": "",
            "io_port": this.IOPortDefault,
            "vbdf": "",
          },
          {
            "vm_name": "",
            "io_port": this.IOPortDefault,
            "vbdf": "",
          }
        ]
      })
    }
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
