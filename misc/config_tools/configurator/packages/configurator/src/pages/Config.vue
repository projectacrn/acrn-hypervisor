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
      <Board :WorkingFolder="WorkingFolder" :board="board" :schemas="schemas"
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
        <button type="button" :disabled="!scenarioHaveData || saving" class="wel-btn btn btn-primary btn-lg SaveButton"
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
import {mapStores} from 'pinia';
import Banner from '../components/common/Banner.vue';
import Board from "./Config/Board.vue";
import Scenario from "./Config/Scenario.vue";
import TabBox from "./Config/ConfigForm/TabBox.vue";
import ConfigForm from "./Config/ConfigForm.vue";
import SaveScenario from "./Config/Scenario/SaveScenario.vue";

import configurator from "../lib/acrn";
import {mapErrors} from "../domain/validation/mapErrors";
import {useScenarioStore} from "../stores/scenario";
import {useValidationStore} from "../stores/validation";

export default {
  name: "Config",
  components: {SaveScenario,ConfigForm, TabBox, Scenario, Icon, Board, Banner, AngleLeft},
  props: {
    WorkingFolder: {type: String},
    isNewConfig: {type: String}
  },
  mounted() {
    this.scenarioStore.reset()
    this.validationStore.clear()
    this.showFlag = this.isNewConfig === 'true'
  },
  data() {
    return {
      currentBoardFile: '',
      currentBoardManu: '',
      CurrentBoardProd: '',
      showFlag: false,
      totalMsg: "",
      showTotalMessageFlag: false,
      saving: false
    }
  },
  computed: {
    ...mapStores(useScenarioStore, useValidationStore),
    scenario() {
      return this.scenarioStore.doc || {}
    },
    board() {
      return this.scenarioStore.board || {}
    },
    schemas() {
      return this.scenarioStore.schemas || {}
    },
    activeVMID: {
      get() {
        return this.scenarioStore.activeVmId
      },
      set(vmid) {
        this.scenarioStore.activeVmId = vmid
      }
    },
    currentFormSchema() {
      return this.scenarioStore.activeSchema || {}
    },
    currentFormData() {
      return this.scenarioStore.activeFormData
    },
    errors() {
      return this.validationStore.errors
    },
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
      this.scenarioStore.loadBoard(boardInfo, scenarioJSONSchema)
      this.updateCurrentBoardInfo()
    },
    async scenarioUpdate(scenarioData) {
      if (this.scenarioStore.doc !== scenarioData) {
        await this.scenarioStore.importScenario(scenarioData)
      }
      this.showFlag = false;
    },
    switchTab(tabVMID) {
      this.scenarioStore.setActiveVm(tabVMID)
      if (this.$refs.ConfigForm) {
        this.$refs.ConfigForm.currentFormMode = 'BasicConfigType'
      }
    },
    addVM(load_order) {
      this.scenarioStore.addVm(load_order)
      if (this.$refs.ConfigForm) {
        this.$refs.ConfigForm.currentFormMode = 'BasicConfigType'
      }
      return this.scenarioStore.validateCurrent()
    },
    async deleteVM() {
      const vmConfigCurrent = this.scenarioStore.activeVm
      if (vmConfigCurrent === null) {
        return
      }
      const isServiceVM = vmConfigCurrent['load_order'] === 'SERVICE_VM'
      let msg
      if (isServiceVM) {
        msg = "Post-launched VMs require the Service VM. If you proceed, all post-launched VMs and their settings will also be deleted. Are you sure you want to proceed?"
      } else {
        let vmName = vmConfigCurrent.name
        msg = `Delete this virtual machine? ${vmName}\n\nThe associated launch script will also be deleted if it exists.`
      }
      const confirmed = await confirm(msg)
      if (confirmed) {
        this.scenarioStore.removeVm()
      }
    },
    scenarioConfigFormDataUpdate(vmid, data) {
      this.scenarioStore.updateActiveForm(vmid, data)
    },
    vmNameChange(newname, oldname) {
      this.scenarioStore.renameVm(oldname, newname)
    },
    updateCpuAffinity(vmid) {
      const vm = this.scenario.vm.find(vmConfig => vmConfig['@id'] === vmid)
      if (vm) {
        this.scenarioStore.updateActiveForm(vmid, vm)
      }
    },
    confirmVmName() {
      let vmNameArr = []
      for (let i = 0; i < this.scenario.vm.length; i++) {
        vmNameArr.push(this.scenario.vm[i].name)
      }
      vmNameArr.sort()
      for (let i = 0; i < vmNameArr.length - 1; i++) {
        if (vmNameArr[i] === vmNameArr[i + 1]) {
          return `Multiple VMs have the same name: ${vmNameArr[i]}. ` +
              "Make sure each VM has a unique name before generating configuration artifacts."
        }
      }
      return false
    },
    showSaveError(message) {
      this.totalMsg = `Scenario xml save failed\n\n${message}\n\nPlease check your configuration`
      this.showTotalMessageFlag = true
      return this.totalMsg
    },
    scenarioToXML(scenarioData) {
      return configurator.convertScenarioToXML({"acrn-config": scenarioData})
    },
    translateErrors(errors, scenarioData) {
      return mapErrors(errors, scenarioData)
    },
    formatSaveReport(report) {
      const msg = [
        "Scenario xml saved\n",
        "Settings validated\n",
        "Document config_summary.rst generated\n",
        "Launch scripts generated\n"
      ]
      const errmsg = [
        "Scenario xml save failed\n",
        "Settings validate failed\n",
        "Document config_summary.rst generation failed\n",
        "Launch scripts generation failed\n"
      ]
      const status = id => report.steps.find(step => step.id === id)?.status
      let stepDone = 0
      if (status('writeScenario') === 'ok') {
        stepDone = 1
      }
      if (stepDone === 1 && status('validate') === 'ok') {
        stepDone = 2
      }
      if (stepDone === 2 && status('configSummary') === 'ok') {
        stepDone = 3
      }
      if (stepDone === 3 && report.needSaveLaunchScript &&
          status('launchScripts') === 'ok' && status('pruneStaleScripts') === 'ok') {
        stepDone = 4
      }

      if (report.success) {
        return `${msg.slice(0, stepDone).join('')} \nAll files successfully saved to your working folder ${this.WorkingFolder}`
      }

      console.log("error" + report.error)
      const totalMsgLength = report.needSaveLaunchScript ? msg.length : msg.length - 1
      let outmsg = ''
      for (let i = 0; i < stepDone; i++) {
        outmsg += msg[i]
      }
      for (let i = stepDone; i < totalMsgLength; i++) {
        outmsg += errmsg[i]
      }
      const detail = report.error instanceof Error && report.error.message !== "validation failed"
          ? `\n${report.error.message}\n`
          : ''
      return `${outmsg}${detail} \n Please check your configuration`
    },
    async saveScenario() {
      if (this.saving) {
        return this.totalMsg
      }
      if (_.isEmpty(this.scenario.vm)) {
        return this.showSaveError("Please add at least one VM")
      }
      const vmNameError = this.confirmVmName()

      this.saving = true
      this.showTotalMessageFlag = false
      try {
        const report = await this.scenarioStore.save(this.WorkingFolder, vmNameError)
        this.totalMsg = this.formatSaveReport(report)
      } finally {
        this.saving = false
        this.showTotalMessageFlag = true
      }
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
