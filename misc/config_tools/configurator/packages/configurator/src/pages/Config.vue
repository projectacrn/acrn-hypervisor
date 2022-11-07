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
        <SaveScenario v-model:show-modal="showTotalMessageFlag" :total-msg="totalMsg"/>
        <button type="button" :disabled="!scenarioHaveData" class="wel-btn btn btn-primary btn-lg SaveButton"
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
              :errors="errors"
              @tabActive="switchTab"
              @addVM="addVM"
          />
        </div>
        <div v-if="errors.hasOwnProperty(activeVMID)">
          <div class="px-4" style="color: red" v-for="error in errors[activeVMID]">
            {{ error.message }}
          </div>
        </div>
        <div class="p-4">
          <ConfigForm
              :key="(activeVMID===-1?'HV':`VM${activeVMID}`)+'ConfigForm'"
              :currentActiveVMID="activeVMID"
              :currentFormSchema="currentFormSchema"
              :currentFormData="currentFormData"
              ref="ConfigForm"

              @deleteVM="deleteVM"
              @scenarioConfigFormDataUpdate="scenarioConfigFormDataUpdate"
              @vmNameChange="vmNameChange"
              @updateCpuAffinity="updateCpuAffinity"
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
import SaveScenario from "./Config/Scenario/SaveScenario.vue";

import configurator from "../lib/acrn";

export default {
  name: "Config",
  components: {SaveScenario,ConfigForm, TabBox, Scenario, Icon, Board, Banner, AngleLeft},
  props: {
    WorkingFolder: {type: String},
    isNewConfig: {type: String}
  },
  mounted() {
    this.updateCurrentFormSchema()
    window.getSchemaData = this.getSchemaData
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
      errors: [],
      totalMsg: "",
      showTotalMessageFlag: false,
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
      this.switchTab(-1)
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
      let scenarioXMLData = this.scenarioToXML(scenarioData)
      let all_errors = configurator.pythonObject.validateScenario(this.board.content, scenarioXMLData)
      this.errors = this.translateErrors(all_errors, scenarioData)
      this.scenario = scenarioData;
      this.showFlag = false;
      this.updateCurrentFormSchema()
      this.updateCurrentFormData()
      configurator.cat.scenarioLoaded()
      this.switchTab(-1)
    },
    getSchemaData() {
      return this.schemas
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
      this.$refs.ConfigForm.currentFormMode = 'BasicConfigType'
      this.updateCurrentFormSchema()
      this.updateCurrentFormData()
    },
    addVM(load_order) {
      let maxVMID = -1;
      let haveService = false;
      this.scenario.vm.map((vmConfig) => {
        let vmID = vmConfig['@id'];
        if (vmConfig['name'] && (/^VM(\d+)$/).test(vmConfig['name'])) {
          let temp = vmConfig['name'].replace(/\D/g, ' ')
          if (maxVMID < parseInt(temp)) {
            maxVMID = parseInt(temp)
          }
        }
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
      this.switchTab(maxVMID)
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
        let vmName = vmConfigcurrent.name
        msg = `Delete this virtual machine? ${vmName}\n\nThe associated launch script will also be deleted if it exists.`
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
      return configurator.readDir(this.WorkingFolder, false)
          .then((files) => {
            if (files.length > 0) {
              for (let i = 0; i < files.length; i++) {
                let arr = files[i].path.split('.')
                let suffix = arr[arr.length - 1]
                if (suffix === 'sh') {
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
      if (hvdata.hasOwnProperty('FEATURES') && hvdata.FEATURES.hasOwnProperty('IVSHMEM')) {
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
    updateCpuAffinity(vmid) {
      this.scenario.vm.map((vmConfig, vmIndex) => {
          if (vmConfig['@id'] === vmid && vmConfig['cpu_affinity'] != null) {
            for (let i = 0; i < vmConfig['cpu_affinity']['pcpu'].length; i++) {
              this.scenario.vm[vmIndex]['cpu_affinity']['pcpu'][i]['real_time_vcpu'] = 'n'
            }
          }
        })
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
          alert("ERROR\n" + `Multiple VMs have the same name: ${vmNameArr[i]}. ` +
              "Your configuration cannot be saved. " + "Make sure each VM has a unique name.");
          errorFlag = true
        }
      }
      return errorFlag
    },
    scenarioToXML(scenarioData) {
      return configurator.convertScenarioToXML(
          {
            // simple deep copy
            "acrn-config": JSON.parse(JSON.stringify(scenarioData))
          }
      )
    },
    applyScenarioDefaults(scenarioData) {
      let scenarioXMLData = this.scenarioToXML(scenarioData)
      // get scenario Defaults
      let scenarioWithDefault = configurator.pythonObject.populateDefaultValues(scenarioXMLData)
      scenarioWithDefault = scenarioWithDefault.json['acrn-config']

      if (scenarioWithDefault.hv.FEATURES.RDT.RDT_ENABLED === 'n') {
        delete scenarioWithDefault.hv.CACHE_REGION
      }
      return scenarioWithDefault
    },
    translateErrors(errors, scenarioData) {
      let formErrors = {}

      let translate = errorType => error => {
        error.paths.forEach(path => {
          let formPath = path.split('/')[2];
          // translate form path to scenario vmid
          let vmid = -1
          if (formPath === 'hv') {
            vmid = -1
          } else if (formPath === 'vm') {
            vmid = scenarioData.vm[0]['@id']
          } else if (/vm\[\d+]/.test(formPath)) {
            let vmIndex = /vm\[(\d+)]/.exec(formPath)[1]
            vmIndex = parseInt(vmIndex) - 1
            vmid = scenarioData.vm[vmIndex]['@id']
          }
          if (!formErrors.hasOwnProperty(vmid)) {
            formErrors[vmid] = []
          }
          if (errorType === 'semantic') {
            formErrors[vmid].push(error)
          }
          formErrors[vmid] = _.uniq(formErrors[vmid])
        })
      }

      if (errors.syntactic_errors.length > 0) {
        errors.syntactic_errors.forEach(translate('syntactic'))
      }
      if (errors.semantic_errors.length > 0) {
        errors.semantic_errors.forEach(translate('semantic'))
      }

      return formErrors
    },
    async saveScenario() {
      if (_.isEmpty(this.scenario.vm)) {
        alert("Please add at least one VM")
        return
      }
      let errorFlag = false
      errorFlag = this.confirmVmName()
      if (errorFlag) {
        return
      }
      await this.assignVMID()
      let msg = [
        "Scenario xml saved\n",
        "Settings validated\n",
        "Document config_summary.rst generated\n",
        "Launch scripts generated\n"
      ];
      let errmsg = [
        "Scenario xml save failed\n",
        "Settings validate failed\n",
        "Document config_summary.rst generation failed\n",
        "Launch scripts generation failed\n"
      ];
      let stepDone = 0
      let totalMsgLength = msg.length // msg and errMsg must be same length.
      let needSaveLaunchScript = false

      this.scenario.hv.CACHE_REGION = configurator.cat.getScenarioDataFromCAT()

      let scenarioWithDefaults = await this.applyScenarioDefaults(this.scenario)
      let scenarioXMLData = this.scenarioToXML(scenarioWithDefaults)
      this.scenario = scenarioWithDefaults

      this.scenario.vm.map((vmConfig) => {
        if (vmConfig['load_order'] === 'POST_LAUNCHED_VM') {
          needSaveLaunchScript = true
        }
      })
      if (!needSaveLaunchScript) {
        totalMsgLength = totalMsgLength - 1 // remove the 'launch script' related message.
      }
      // begin write down and verify

      configurator.writeFile(this.WorkingFolder + 'scenario.xml', scenarioXMLData)
          .then(() => {
            this.updateCurrentFormData()
          })
          .then(() => {
            // validate scenario and clean up the launch script
            stepDone = 1
            console.log("validate settings...")
            let all_errors = configurator.pythonObject.validateScenario(this.board.content, scenarioXMLData)
            // noinspection JSUnresolvedVariable
            this.errors = this.translateErrors(all_errors, this.scenario)
            // noinspection JSUnresolvedVariable
            if (all_errors.syntactic_errors.length !== 0 || all_errors.semantic_errors.length !== 0) {
              throw new Error("validation failed")
            }
            console.log("validation ok")
            stepDone = 2
            return this.cleanLaunchScript()
          })
          .then(() => {
            // generate config_summary
            let configSummary = configurator.pythonObject.generateConfigSummary(this.board.content, scenarioXMLData)
            return configurator.writeFile(this.WorkingFolder + 'config_summary.rst', configSummary)
          })
          .then(() => {
            // generate launch script
            if (needSaveLaunchScript) {
              let launchScripts = configurator.pythonObject.generateLaunchScript(this.board.content, scenarioXMLData)
              let writeDone = []
              for (let filename in launchScripts) {
                writeDone.push(configurator.writeFile(this.WorkingFolder + filename, launchScripts[filename]))
              }
              return Promise.all(writeDone)
            }
          })
          .then((result) => {
            // show success message
            if (!_.isEmpty(result)) {
              stepDone = 4
            }
            this.totalMsg = `${msg.slice(0, stepDone).join('')} \nAll files successfully saved to your working folder ${this.WorkingFolder}`
          })
          .catch((err) => {
            // show error message
            console.log("error" + err)
            let outmsg = ''
            for (var i = 0; i < stepDone; i++)
              outmsg += msg[i]
            for (i = stepDone; i < totalMsgLength; i++)
              outmsg += errmsg[i]
            this.totalMsg = `${outmsg} \n Please check your configuration`
          })

      this.showTotalMessageFlag = true
      return this.totalMsg
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
