<template>
  <form class="">
    <b class="py-2" style="letter-spacing: 0.49px;">Start a new configuration</b>
    <p class="py-3 mb-2" style="max-width: 465px; letter-spacing: -0.143px;">
      ACRN Configurator saves your scenario and configuration files into a working folder.
    </p>
    <label class="d-block pb-2" style="letter-spacing: -0.29px;">
      Select the working folder
    </label>
    <table>
      <tbody>
      <tr>
        <td style="width: 100%;">
          <input type="text" class="d-inline form-control"
                 :placeholder="defaultWorkingFolder"
                 v-model="WorkingFolder">
        </td>
        <td><a class="ps-3 text-nowrap" href="#" @click="openFolder">Browse for folder…</a></td>
      </tr>
      <tr>
        <td>
          <div class="py-4 text-right">
            <button
                type="button" class="wel-btn btn btn-primary btn-lg"
                @click="usingWorkingFolder"
            >
              Use this Folder
            </button>
          </div>
        </td>
        <td></td>
      </tr>
      </tbody>
    </table>
  </form>
</template>

<script>
import configurator from "../../lib/acrn";

export default {
  name: "NewConfiguration",
  data: () => {
    const defaultWorkingFolder = [
      window.systemInfo.homeDir, "acrn-work", "MyConfiguration"
    ].join(window.systemInfo.pathSplit) + window.systemInfo.pathSplit;
    let WorkingFolder = defaultWorkingFolder;
    return {
      defaultWorkingFolder,
      WorkingFolder
    }
  },
  methods: {
    nextPage(folderPath) {
      this.$router.push({name: 'Config', params: {WorkingFolder: folderPath}})
    },
    usingWorkingFolder() {
      let folderPath = this.WorkingFolder.length ? this.WorkingFolder : this.defaultWorkingFolder;
      if (folderPath[0] === '~') {
        folderPath = window.systemInfo.homeDir + window.systemInfo.pathSplit + folderPath.substring(1)
      }
      this.WorkingFolder = folderPath;

      configurator.readDir(folderPath, false)
          .then((files) => {
            console.log("Directory exists.", files)
            if (files.length > 0) {
              confirm("Directory exists, overwrite it?")
                  .then((r) => {
                    if (r) this.nextPage(folderPath)
                  })
            } else {
              this.nextPage(folderPath)
            }
          })
          .catch((error) => {
            console.log(error)
            configurator.creatDir(folderPath)
                .then(() => {
                  console.log('Directory created successfully!');
                  this.nextPage(folderPath)
                })
                .catch((err) => {
                  console.error(err);
                })
          })
    },
    openFolder() {
      configurator.openDialog({
        title: "Start new configurator",
        directory: true,
        multiple: false
      }).then((folderPath) => {
        if (folderPath.length > 0)
          this.WorkingFolder = folderPath
      })
    }
  }
}
</script>

<style scoped>

</style>