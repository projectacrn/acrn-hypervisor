<template>
  <div class="px-3 py-2 row">
    <div class="border-end-sm py-1 col-sm">
      <div class="py-4 text-center">
        <OverwriteMessage v-model:show-modal="showOverwriteMessage" :usingWorkingFolder="WorkingFolder"
                          @overWrite="overWriteScenario"/>
        <NewScenario v-model:show-modal="showCreateScenario" @newScenario="newScenario"/>
        <button type="button" class="btn btn-primary btn-lg" @click="checkAndCreateScenario">
          Create Scenario…
        </button>
      </div>
    </div>
    <div class="py-1 ps-sm-5 col-sm">
      <form class=""><b class="d-block pb-3" style="letter-spacing: -0.29px;">Recently used scenarios:</b>
        <table>
          <tbody>
          <tr>
            <td><select class="d-inline form-select" v-model="currentSelectedScenario">
              <option :value="his" v-for="his in scenarioHistory">
                {{ his }}
              </option>
            </select><input type="file" style="display: none;"></td>
            <td>
              <a class="ps-3 text-nowrap" style="cursor: pointer;" @click="openScenarioFileSelectDialog">
                Browse for scenario file…
              </a>
            </td>
          </tr>
          <tr>
            <td>
              <div class="py-4 text-right">
                <button type="button" class="wel-btn btn btn-primary btn-lg" @click="checkAndLoadScenario(false)">
                  Import Scenario
                </button>
              </div>
            </td>
            <td></td>
          </tr>
          </tbody>
        </table>
      </form>
    </div>
  </div>
</template>

<script>
import OverwriteMessage from "./Scenario/OverwriteMessage";
import configurator from "../../lib/acrn";
import NewScenario from "./Scenario/NewScenario.vue";
import _ from "lodash";

export default {
  name: "Scenario",
  components: {NewScenario, OverwriteMessage},
  emits: ['scenarioUpdate'],
  data() {
    return {
      clickedButton: "",
      scenarioHistory: [],
      currentSelectedScenario: '',
      showCreateScenario: false,
      showOverwriteMessage: false,
      loadRequestID: 0,
      historyRequestID: 0
    }
  },
  props: {
    WorkingFolder: {
      type: String
    },
    scenario: {
      type: Object
    }
  },
  mounted() {
    //get init scenario if it exist, add to history
    this.getExistScenarioPath()
        .then((filePath) => {
          if (filePath.length > 0) {
            configurator.addHistory('Scenario', filePath)
                .then(() => {
                  this.getScenarioHistory().then(() => {
                    if (_.isEmpty(this.scenario)) {
                      this.loadScenario(true)
                    }
                  })
                })
          }
        })
    this.getScenarioHistory()
  },
  beforeUnmount() {
    this.loadRequestID++
    this.historyRequestID++
  },

  methods: {
    checkAndCreateScenario() {
      this.clickedButton = "createScenario"
      if (!_.isEmpty(this.scenario)) {
        this.showOverwriteMessage = true
      } else this.showCreateScenario = true
    },
    newScenario(data) {
      this.loadRequestID++
      this.$emit('scenarioUpdate', data)
    },
    checkAndLoadScenario(auto = false) {
      this.clickedButton = "loadScenario"
      if (!_.isEmpty(this.scenario)) {
        this.showOverwriteMessage = true
      } else this.loadScenario(auto)
    },
    overWriteScenario(auto = false) {
      if (this.clickedButton === "createScenario") {
        this.showCreateScenario = true
      }
      if (this.clickedButton === "loadScenario") {
        this.loadScenario(auto)
      }
    },
    loadScenario(auto) {
      const scenarioPath = this.currentSelectedScenario
      if (scenarioPath.length === 0) {
        return Promise.resolve()
      }
      const requestID = ++this.loadRequestID
      return configurator.loadScenario(scenarioPath)
          .then((scenarioConfig) => {
            if (requestID !== this.loadRequestID || scenarioPath !== this.currentSelectedScenario) {
              return
            }
            this.$emit('scenarioUpdate', scenarioConfig['acrn-config'])
            if (!auto) {
              alert(`Scenario ${scenarioPath} loaded success!`)
            }
          }).catch((err) => {
            if (requestID !== this.loadRequestID) {
              return
            }
            console.log(err)
            alert(`Loading ${scenarioPath} failed: ${err}`)
          })
    },
    getExistScenarioPath() {
      // only return filename when using exist configuration.
      return configurator.readDir(this.WorkingFolder, false)
          .then((res) => {
            let scenarioPath = ''
            res.forEach((filepath) => {
              if (filepath.path.split(/[\\/]/).pop() === 'scenario.xml') {
                scenarioPath = filepath.path
              }
            })
            // only return the last vaild boardPath
            return scenarioPath
          })
    },
    openScenarioFileSelectDialog() {
      configurator.openDialog({
        title: "Open Existing Scenario XML",
        directory: false,
        multiple: false
      }).then((dirPath) => {
        if (dirPath) {
          configurator.addHistory("Scenario", dirPath).then(() => {
            this.getScenarioHistory()
          })
        }
      })
    },
    getScenarioHistory() {
      const requestID = ++this.historyRequestID
      return configurator.getHistory("Scenario")
          .then((scenarioHistory) => {
            if (requestID !== this.historyRequestID) {
              return
            }
            this.scenarioHistory = scenarioHistory
            if (this.scenarioHistory.length > 0) {
              this.currentSelectedScenario = this.scenarioHistory[0]
            }
          })
    }
  }
}
</script>

<style scoped>

</style>
