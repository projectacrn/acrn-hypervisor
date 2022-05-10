<template>
  <div class="py-3 py-sm-0 px-3" style="width: 100%;">
    <form class="">
      <b class="py-2">Use an existing configuration</b>
      <p class="py-3 mb-0 mb-sm-4" style="max-width: 462px; letter-spacing: -0.3px;">
        Open a working folder to retrieve an existing configuration.
      </p>
      <label class="d-block py-2" style="letter-spacing: -0.29px;">
        Select the working folder
      </label>
      <table>
        <tbody>
        <tr>
          <td>
            <select class="d-inline form-select" v-model="currentSelected">
              <option v-for="history in historyWorkingFolder" :value="history">{{ history }}</option>
            </select>
          </td>
          <td><a class="ps-3 text-nowrap" style="cursor: pointer" @click="openFolder">Browse for folder…</a></td>
        </tr>
        <tr>
          <td>
            <div class="py-4 text-right">
              <button
                  type="button" class="wel-btn btn btn-primary btn-lg"
                  @click="usingWorkingFolder"
                  :disabled="!(currentSelected.length>0)">
                Open Folder
              </button>
            </div>
          </td>
          <td></td>
        </tr>
        </tbody>
      </table>
    </form>
  </div>
</template>

<script>
import configurator from "../../lib/acrn";

export default {
  name: "UseExisting",
  data() {
    return {
      historyWorkingFolder: [],
      currentSelected: ''
    }
  },
  mounted() {
    this.getHistory()
  },
  methods: {
    nextPage(folderPath) {
      this.$router.push({name: 'Config', params: {WorkingFolder: folderPath, isNewConfig:false}})
    },
    usingWorkingFolder() {
      this.nextPage(this.currentSelected, true)
    },
    getHistory() {
      configurator.getHistory("WorkingFolder")
          .then(
              (historyWorkingFolder) => {
                this.historyWorkingFolder = historyWorkingFolder;
                if (this.historyWorkingFolder.length > 0) {
                  this.currentSelected = this.historyWorkingFolder[0]
                }
              }
          )
    },
    openFolder() {
      configurator.openDialog({
        title: 'Open Working Folder',
        directory: true,
        multiple: false
      }).then(
          (folderPath) => configurator.addHistory("WorkingFolder", folderPath + window.systemInfo.pathSplit)
      ).then(() => {
        this.getHistory()
      })
    }
  }
}
</script>

<style scoped>

</style>