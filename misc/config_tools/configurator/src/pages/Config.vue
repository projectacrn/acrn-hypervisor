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

    <b-accordion-item visible>
      <template #title>
        <div class="p-1 ps-3 fs-4">1. Import a board configuration file</div>
      </template>
      <Board v-model:WorkingFolder="WorkingFolder" v-model:board="board" v-model:schemas="schemas"
             @boardUpdate="boardUpdate"/>
    </b-accordion-item>

    <Banner/>

    <b-accordion-item>
      <template #title>
        <div class="d-flex flex-column">
          <div class="p-1 ps-3 fs-4">2. Create new or import an existing scenario</div>
          <div class="py-2" style="letter-spacing: 0.49px;">
            Current scenario: {{ scenarioHaveData ? 'scenario.xml' : 'none selected' }}
          </div>
        </div>
      </template>
      <Scenario :scenario="scenario" @scenarioUpdate="scenarioUpdate"/>
    </b-accordion-item>
    <Banner/>

    <b-accordion-item visible>
      <template #title>
        <div class="p-1 ps-3 d-flex w-100 justify-content-between align-items-center">
          <div class="fs-4">3. Configure settings for scenario and launch scripts</div>
          <button type="button" class="btn btn-primary btn-lg" @click="saveScenario">
            Save Scenario and Launch Scripts
          </button>
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
          <div class="px-4" style="color: red" v-for="error in errors">{{ error.message }}</div>
        </div>
        <div class="p-4">
          <ConfigForm
              :currentActiveVMID="activeVMID"
              :currentFormSchema="currentFormSchema"
              :currentFormData="currentFormData"

              @deleteVM="deleteVM"
              @scenarioConfigFormDataUpdate="scenarioConfigFormDataUpdate"
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
import ConfigForm from "./Config/ConfigForm/ConfigForm.vue";

import configurator from "../lib/acrn";

export default {
  name: "Config",
  components: {ConfigForm, TabBox, Scenario, Icon, Board, Banner, AngleLeft},
  props: ['WorkingFolder'],
  mounted() {
    this.updateCurrentFormSchema()
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
      errors: []
    }
  },
  computed: {
    scenarioHaveData() {
      return !_.isEmpty(this.scenario);
    }
  },
  methods: {
    back() {
      this.$router.back()
    },
    boardUpdate(boardInfo, scenarioJSONSchema) {
      this.board = boardInfo;
      this.schemas = scenarioJSONSchema
      this.updateCurrentFormSchema()
    },
    updateCurrentFormSchema() {
      console.log(this.schemas)
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
      this.scenario = scenarioData;
      this.updateCurrentFormSchema()
      this.updateCurrentFormData()
    },
    updateCurrentFormData() {
      console.log(this.scenario)
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
      this.scenario.vm.map((vmConfig, vmIndex) => {
        if (vmConfig['@id'] === this.activeVMID) {
          currentVMIndex = vmIndex
        }
      })
      this.scenario.vm.splice(currentVMIndex, 1);
      this.updateCurrentFormSchema()
      this.updateCurrentFormData()
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
    saveScenario() {
      this.assignVMID()
      let msg = ["Scenario xml saved\n",
        ".xml settings validated\n",
        "launch scripts generated\n"];
      let errmsg = ["Scenario xml save failed\n",
        ".xml settings validate failed\n",
        "launch scripts generate failed\n"];
      let step = 0
      let scenarioXMLData = configurator.convertScenarioToXML(
          {
            // simple deep copy
            "acrn-config": JSON.parse(JSON.stringify(this.scenario))
          }
      );
      configurator.writeFile(this.WorkingFolder + 'scenario.xml', scenarioXMLData)
        .then(() => {
          step = 1
          configurator.pythonObject.validateScenario(this.board.content, scenarioXMLData)
        })
        .then(() => {
            step = 2
            let launchScripts = configurator.pythonObject.generateLaunchScript(this.board.content, scenarioXMLData)
            for (let filename in launchScripts) {
              configurator.writeFile(this.WorkingFolder + filename, launchScripts[filename])
            }
        })
        .then(() => {
            alert(`${msg.join('')} \n All files successfully saved to your working folder ${this.WorkingFolder}`)
        })
        .catch((err) => {
          console.log(err)
          let outmsg = ''
          for (var i = 0; i < step; i++)
            outmsg += msg[i]
          for (i = step; i < 3; i++)
            outmsg += errmsg[i]
          alert(`${outmsg} \n Please check your configuration`)
        })
    }
  }
}
</script>

<style scoped>

</style>
