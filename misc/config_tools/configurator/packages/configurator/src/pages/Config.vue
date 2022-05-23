<template>

  <Banner>
    <div class="ms-4 ps-3 py-4 text-white d-flex align-items-center">
      <Icon size="32px" color="#69bfad" @click="back" style="cursor: pointer;">
        <AngleLeft/>
      </Icon>
      Working folder: {{ WorkingFolder }}
    </div>
  </Banner>

  <b-accordion free>

    <b-accordion-item :visible='showFlag'>
      <template #title>
        <div class="d-flex flex-column">
          <div class="p-1 ps-3 fs-4">1. Import a board configuration file</div>
          <div class="py-2" style="letter-spacing: 0.49px;">
            {{ currentBoardFile }}
          </div>
          <div class="py-2" style="letter-spacing: 0.49px;">
            {{ currentBoardManu }}
            <br/>
            {{ CurrentBoardProd }}
          </div>
        </div>
      </template>
      <Board v-model:WorkingFolder="WorkingFolder" v-model:board="board" v-model:schemas="schemas"
             @boardUpdate="boardUpdate"/>
    </b-accordion-item>

    <Banner/>

    <b-accordion-item :visible='showFlag'>
      <template #title>
        <div class="d-flex flex-column">
          <div class="p-1 ps-3 fs-4">2. Create new or import an existing scenario</div>
          <div class="py-2" style="letter-spacing: 0.49px;">
            {{ scenarioHaveData ? 'Current scenario: scenario.xml' : '' }}
          </div>
        </div>
      </template>
      <Scenario v-if="boardHaveData" :scenario="scenario" :WorkingFolder="WorkingFolder"
                @scenarioUpdate="scenarioUpdate"/>
    </b-accordion-item>
    <Banner>
      <div style="position: relative">
        <button type="button" :disabled="!scenarioHaveData" class="btn btn-primary btn-lg SaveButton"
                @click="saveScenario">
          Save Scenario and Launch Scripts
        </button>
      </div>
    </Banner>

    <b-accordion-item visible>
      <template #title>
        <div class="p-1 ps-3 d-flex w-100 justify-content-between align-items-center">
          <div class="fs-4">3. Configure settings for scenario and launch scripts</div>
        </div>
      </template>

      <div v-if="scenarioHaveData">
        <div class="p-4">
          <TabBox
              :scenario="scenario"
              :activeVMID="activeVMID"
              @tabActive="switchTab"
              @addVM="addVM"
          />
        </div>
        <div v-if="errors">
          <div class="px-4" style="color: red" v-for="error in errors">{{ error }}</div>
        </div>
        <div class="p-4">
          <ConfigForm
              :currentActiveVMID="activeVMID"
              :currentFormSchema="currentFormSchema"
              :currentFormData="currentFormData"

              @deleteVM="deleteVM"
              @scenarioConfigFormDataUpdate="scenarioConfigFormDataUpdate"
              @vmNameChange="vmNameChange"
          />
        </div>
      </div>

    </b-accordion-item>
  </b-accordion>
</template>

<script>
import _ from 'lodash';
import {Icon} from "@vicons/utils";
import {AngleLeft} from "@vicons/fa";
import Banner from '../components/common/Banner.vue';
import Board from "./Config/Board.vue";
import Scenario from "./Config/Scenario.vue";
import TabBox from "./Config/ConfigForm/TabBox.vue";
import ConfigForm from "./Config/ConfigForm.vue";

import configurator from "../lib/acrn";

export default {
  name: "Config",
  components: {ConfigForm, TabBox, Scenario, Icon, Board, Banner, AngleLeft},
  props: {
    WorkingFolder: {type: String},
    isNewConfig: {type: String}
  },
  mounted() {
    this.updateCurrentFormSchema()
    window.getCurrentFormSchemaData = this.getCurrentFormSchemaData
    window.getCurrentScenarioData = this.getCurrentScenarioData
    window.getBoardData = this.getBoardData
    this.showFlag = this.isNewConfig === 'true'
  },
  data() {
    return {
      activeVMID: -1,
      schemas: {
        // HV:{},PreLaunchedVM:{},ServiceVM:{},
      },
      board: {},
      scenario: {},
      currentFormSchema: {},
      currentFormData: {},
      currentBoardFile: '',
      currentBoardManu: '',
      CurrentBoardProd: '',
      showFlag: false,
      errors: []
    }
  },
  computed: {
    scenarioHaveData() {
      return !_.isEmpty(this.scenario);
    },
    boardHaveData() {
      return !_.isEmpty(this.board);
    }
  },
  methods: {
    back() {
      this.$router.back()
    },
    updateCurrentBoardInfo() {
      // update the info in title
      this.currentBoardFile = 'Current file: ' + this.board['name']
      let boardContent = this.board.content
      let reg = /(?<=Manufacturer).+(?=\\n)/
      let result = boardContent.match(/Manufacturer.+\n/gm)
      console.log(result)
      if (result && result.length > 0) {
        this.currentBoardManu = result[0]
      }
      var result_p = boardContent.match(/Product Name.+\n/gm)
      if (result_p && result_p.length > 0) {
        this.CurrentBoardProd = result_p[0]
      }
    },
    boardUpdate(boardInfo, scenarioJSONSchema) {
      this.board = boardInfo;
      this.schemas = scenarioJSONSchema
      this.updateCurrentFormSchema()
      this.updateCurrentBoardInfo()
    },
    updateCurrentFormSchema() {
      if (this.activeVMID === -1) {
        this.currentFormSchema = this.schemas.HV
      } else {
        this.scenario.vm.map((vmConfig) => {
          if (vmConfig['@id'] === this.activeVMID) {
            let vm_schema = {
              PRE_LAUNCHED_VM: "PreLaunchedVM",
              SERVICE_VM: "ServiceVM",
              POST_LAUNCHED_VM: "PostLaunchedVM"
            }
            this.currentFormSchema = this.schemas[vm_schema[vmConfig['load_order']]]
          }
        })
      }
    },
    scenarioUpdate(scenarioData) {
      this.errors = []
      this.scenario = scenarioData;
      this.showFlag = false;
      this.updateCurrentFormSchema()
      this.updateCurrentFormData()
    },
    getCurrentFormSchemaData() {
      return this.currentFormSchema
    },
    getCurrentScenarioData() {
      return this.scenario
    },
    getBoardData() {
      return this.board
    },
    updateCurrentFormData() {
      if (this.activeVMID === -1) {
        this.currentFormData = this.scenario.hv;
      }
      for (let i = 0; i < this.scenario.vm.length; i++) {
        if (this.scenario.vm[i]['@id'] === this.activeVMID) {
          this.currentFormData = this.scenario.vm[i]
        }
      }
    },
    switchTab(tabVMID) {
      this.activeVMID = tabVMID;
      this.updateCurrentFormSchema()
      this.updateCurrentFormData()
    },
    addVM(load_order) {
      let maxVMID = -1;
      let haveService = false;
      this.scenario.vm.map((vmConfig) => {
        let vmID = vmConfig['@id'];
        if (vmID > maxVMID) {
          maxVMID = vmID
        }
        if (vmConfig['load_order'] === 'SERVICE_VM') {
          haveService = true;
        }
      })
      if (load_order === 'POST_LAUNCHED_VM' && !haveService) {
        maxVMID++;
        this.scenario.vm.push(configurator.newVM(maxVMID, 'SERVICE_VM'))
      }
      maxVMID++;
      this.scenario.vm.push(configurator.newVM(maxVMID, load_order))
    },
    deleteVM() {
      let currentVMIndex = -1;
      let vmConfigcurrent = []
      let postvmlist = []
      let isserivevm = false
      let msg = ''
      this.scenario.vm.map((vmConfig, vmIndex) => {
        if (vmConfig['@id'] === this.activeVMID) {
          currentVMIndex = vmIndex
          vmConfigcurrent = vmConfig
        }
        if (vmConfig['load_order'] === 'POST_LAUNCHED_VM') {
          postvmlist.push(vmIndex)
          console.log(postvmlist)
        }
      })
      if (vmConfigcurrent['load_order'] === 'SERVICE_VM') {
        msg = "Post-launched VMs require the Service VM. If you proceed, all post-launched VMs and their settings will also be deleted. Are you sure you want to proceed?"
        isserivevm = true
      } else {
        msg = `Delete this virtual machine? VM${this.activeVMID}\n\nThe associated launch script will also be deleted if it exists.`
      }
      confirm(msg).then((r) => {
        if (r) {
          if (isserivevm) {
            for (let i = postvmlist.length - 1; i >= 0; i--) {
              this.scenario.vm.splice(postvmlist[i], 1);
            }
          }
          this.vmNameChange('', this.scenario.vm[currentVMIndex].name)
          this.scenario.vm.splice(currentVMIndex, 1);
          this.activeVMID = -1;
          this.updateCurrentFormSchema()
          this.updateCurrentFormData()
        }
      })
    },
    cleanLaunchScript() {
      configurator.readDir(this.WorkingFolder, false)
        .then((files) => {
          if (files.length > 0) {
            for (let i = 0; i < files.length; i++) {
              let arr = files[i].path.split('.')
              let suffix = arr[arr.length - 1]
              if (suffix == 'sh') {
                configurator.removeFile(files[i].path)
                .catch((err) => alert(`${err}`))
              }
            }
          }
        })
    },
    scenarioConfigFormDataUpdate(vmid, data) {
      if (vmid === -1) {
        this.scenario.hv = data
      } else {
        this.scenario.vm.map((vmConfig, vmIndex) => {
          if (vmConfig['@id'] === vmid) {
            this.scenario.vm[vmIndex] = data
          }
        })
      }
      this.updateCurrentFormData()
    },
    vmNameChange(newname, oldname) {
      let hvdata = this.scenario.hv
      if (hvdata.FEATURES.hasOwnProperty('IVSHMEM')) {
        for (let key in hvdata.FEATURES.IVSHMEM.IVSHMEM_REGION) {
          let region = hvdata.FEATURES.IVSHMEM.IVSHMEM_REGION[key]
          for (let key1 in region.IVSHMEM_VMS.IVSHMEM_VM) {
            let ivshmem_vm = region.IVSHMEM_VMS.IVSHMEM_VM[key1];
            if (ivshmem_vm.VM_NAME === oldname) {
              ivshmem_vm.VM_NAME = newname
            }
          }
        }
      }
      if (hvdata.hasOwnProperty('vuart_connections')) {
        for (let key in hvdata.vuart_connections.vuart_connection) {
          let connection = hvdata.vuart_connections.vuart_connection[key]
          for (let key1 in connection.endpoint) {
            let ep = connection.endpoint[key1];
            if (ep.vm_name === oldname) {
              ep.vm_name = newname
            }
          }
        }
      }
    },
    assignVMID() {
      let vm_priority = {
        'PRE_LAUNCHED_VM': 0,
        'SERVICE_VM': 1,
        'POST_LAUNCHED_VM': 2
      }
      this.scenario.vm.sort((a, b) => {
        return vm_priority[a['load_order']] - vm_priority[b['load_order']]
      }).map((vmConfig, vmIndex) => {
        vmConfig['@id'] = vmIndex
      })
    },
    confirmVmName() {
      let vmNameArr = []
      let errorFlag = false
      for (let i = 0; i < this.scenario.vm.length; i++) {
        vmNameArr.push(this.scenario.vm[i].name)
      }
      vmNameArr.sort()
      console.log(vmNameArr)
      for (let i = 0; i < vmNameArr.length - 1; i++) {
        if (vmNameArr[i] === vmNameArr[i + 1]) {
          alert("ERROR\n" + `Multiple VMs have the same name: ${vmNameArr[i]}.` +
              "Your configuration cannot be saved." + "Make sure each VM has a unique name.");
          errorFlag = true
        }
      }
      return errorFlag
    },
    saveScenario() {
      if (_.isEmpty(this.scenario.vm)) {
        alert("Please add at least one VM")
        return
      }
      let errorFlag = false
      errorFlag = this.confirmVmName()
      this.assignVMID()
      let msg = ["Settings validated\n",
        "scenario xml saved\n",
        "launch scripts generated\n"];
      let errmsg = ["Settings validate failed\n",
        "scenario xml save failed\n",
        "launch scripts generate failed\n"];
      let stepDone = 0
      let totalMsg = msg.length // msg and errMsg must be same length.
      let needSaveLaunchScript = false

      let scenarioXMLData = configurator.convertScenarioToXML(
          {
            // simple deep copy
            "acrn-config": JSON.parse(JSON.stringify(this.scenario))
          }
      );
      console.log(scenarioXMLData)
      // get scenario Defaults
      let scenarioWithDefault = configurator.pythonObject.populateDefaultValues(scenarioXMLData)
      console.log(scenarioWithDefault)
      // write defaults to frontend
      this.scenario = scenarioWithDefault.json['acrn-config']
      this.updateCurrentFormData()
      // get scenario XML with defaults
      scenarioXMLData = scenarioWithDefault.xml
      if (!errorFlag) {
        this.scenario.vm.map((vmConfig) => {
          if (vmConfig['load_order'] === 'POST_LAUNCHED_VM') {
            needSaveLaunchScript = true
          }
        })
        if (!needSaveLaunchScript) {
            totalMsg = totalMsg - 1 // remove the 'launch script' related mssage.
        }
        // begin verify and write down
        console.log("validate settings...")
        try {
          this.errors = configurator.pythonObject.validateScenario(this.board.content, scenarioXMLData)
          if (this.errors.length !== 0) {
            throw "validation failed"
          }
          console.log("validation ok")
          stepDone = 1
          this.cleanLaunchScript()
          configurator.writeFile(this.WorkingFolder + 'scenario.xml', scenarioXMLData)
          .then(()=> {
            stepDone = 2
            if (needSaveLaunchScript) {
              let launchScripts = configurator.pythonObject.generateLaunchScript(this.board.content, scenarioXMLData)
              for (let filename in launchScripts) {
                configurator.writeFile(this.WorkingFolder + filename, launchScripts[filename])
              }
              stepDone = 3
            }
          })
          .then(() => {
            alert(`${msg.slice(0,stepDone).join('')} \nAll files successfully saved to your working folder ${this.WorkingFolder}`)
          })
        } catch(err) {
          console.log("error" + err)
          let outmsg = ''
          for (var i = 0; i < stepDone; i++)
            outmsg += msg[i]
          for (i = stepDone; i < totalMsg; i++)
            outmsg += errmsg[i]
          alert(`${outmsg} \n Please check your configuration`)
        }
      }
    }
  }
}
</script>

<style scoped>
.SaveButton {
  position: absolute;
  right: 1rem;
  top: 64px;
  z-index: 5;
}

</style>
