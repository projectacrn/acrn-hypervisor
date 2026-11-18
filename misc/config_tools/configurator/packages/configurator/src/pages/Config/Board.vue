<template>
  <div class="px-3 py-2 row">
    <div class="border-end-sm py-1 col-sm">
      <form class="">
        <b class="py-3 d-block" style="letter-spacing: 0.49px;">Recently used board files:</b>
        <table style="width: 100%;">
          <tbody>
          <tr>
            <td>
              <select class="d-inline form-select" v-model="currentSelectedBoard" :disabled="imported">
                <option v-for="board in boardHistory" :value="board">
                  {{ board }}
                </option>
              </select>
            </td>
            <td>
              <a class="ps-3 text-nowrap" style="cursor: pointer;"
                 @click="browseForFile">
                {{ !this.imported ? 'Browse for file…' : '' }}
              </a>
            </td>
          </tr>
          <tr>
            <td>
              <div class="py-4 text-right">
                <NewBoard v-model:show-modal="showBoardOverwrite"/>
                <button type="button" class="wel-btn btn btn-primary btn-lg"
                        :disabled="importing"
                        @click="importBoardButton">
                  {{ this.imported ? 'Use a Different Board…' : 'Import Board File' }}
                </button>
              </div>
            </td>
            <td></td>
          </tr>
          </tbody>
        </table>
      </form>
    </div>
    <div class="py-1 ps-sm-5 col-sm">
      <div class="card">
        <div class="card-header">
          {{
            this.imported ? "Current Board: " + this.WorkingFolder + this.board.name : 'No board information has been imported yet.'
          }}
        </div>
        <div class="card-body">
          <div class="card-text text-pre-line">
            <div class="px-3 py-2 row">
              <div class="py-1 col-sm">{{ this.imported ? this.board.BASE_BOARD_INFO : '' }}</div>
              <div class="py-1 col-sm">{{ this.imported ? this.board.BIOS_INFO : '' }}</div>
            </div>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<script>
import _ from 'lodash'
import configurator from "../../lib/acrn";
import {mapStores} from "pinia";
import {useWorkspaceStore} from "../../stores/workspace";
import NewBoard from "./NewBoard.vue";

export default {
  name: "Board",
  components: {NewBoard},
  props: {
    WorkingFolder: {
      type: String
    },
    board: {
      type: Object,
      default: {}
    },
    schemas: {
      type: Object,
      default: {}
    }
  },
  emits: ['boardUpdate'],
  data() {
    return {
      currentSelectedBoard: '',
      showBoardOverwrite: false,
      newFilePath: '',
      importing: false,
    }
  },
  mounted() {
    //get init board if it exist, add to history
    this.getExistBoardPath()
        .then((filePath) => {
          if (filePath.length > 0) {
            console.log("add exist board to history!")
            configurator.addHistory('Board', filePath)
                .then(() => {
                  this.getBoardHistory()
                      .then(() => {
                        this.importBoard()
                      })
                })
          }
        })
    this.getBoardHistory()

    // Todo: auto load board
  },
  beforeUnmount() {
    this.workspaceStore.invalidate('Board')
  },
  computed: {
    ...mapStores(useWorkspaceStore),
    boardHistory() {
      return this.workspaceStore.boardHistory
    },
    imported() {
      this.reformatBoardInfo()
      return !_.isEmpty(this.board)
    }
  },
  methods: {
    importBoardButton() {
      if (this.imported) {
        this.openBoardFileSelectDialog()
            .then((filePath) => {
              if (filePath && filePath.length > 0) {
                if (this.currentSelectedBoard.length > 0) {
                  if (filePath !== this.currentSelectedBoard) {
                    this.showBoardOverwrite = true;
                  }
                }
              }
              this.newFilePath = filePath
            })
      } else {
        this.importBoard()
      }
    },
    async importBoard(useNewFile = false) {
      if (this.importing) {
        return
      }
      let filepath = useNewFile ? this.newFilePath : this.currentSelectedBoard
      if (filepath.length === 0) {
        return
      }

      this.importing = true
      try {
        const {scenarioJSONSchema, boardInfo} = await configurator.loadBoard(filepath)
        const boardFileNewPath = this.WorkingFolder + boardInfo.name
        const oldBoardPath = this.currentSelectedBoard

        await configurator.writeFile(boardFileNewPath, boardInfo.content)
        if (useNewFile && oldBoardPath && oldBoardPath !== boardFileNewPath &&
            oldBoardPath.startsWith(this.WorkingFolder)) {
          await configurator.removeFile(oldBoardPath)
        }
        try {
          await configurator.addHistory('Board', boardFileNewPath)
          await this.getBoardHistory()
        } catch (historyError) {
          console.warn(`Board imported, but its history could not be saved: ${historyError}`)
        }
        this.$emit('boardUpdate', boardInfo, scenarioJSONSchema)
      } catch (err) {
        alert(`Loading ${filepath} failed: ${err}`)
        console.log(err)
      } finally {
        this.importing = false
      }
    },
    reformatBoardInfo() {
      // move BIOS Revision info into the top line
      if (this.board.BIOS_INFO != null) {
        let biosinfo = this.board.BIOS_INFO.split('\n')
        let revindex, infoindex = -1
        for (let [index, element] of biosinfo.entries()) {
          if (element.includes("Revision")) {
            revindex = index
          }
          // add a ":" in line "BIOS Information"
          if (element.includes("Information")) {
            infoindex = index
            if (!element.endsWith(':')) {
              biosinfo[index] = element + ":"
            }
          }
        }
        if (revindex !== -1 && infoindex !== -1) {
          let temp = biosinfo[revindex]
          biosinfo.splice(revindex, 1)
          biosinfo.splice(infoindex + 1, 0, temp)
        }
        this.board.BIOS_INFO = biosinfo.join('\n')
      }
      // add a ":" in line "Base Board Information"
      if (this.board.BASE_BOARD_INFO != null) {
        let boardinfo = this.board.BASE_BOARD_INFO.split('\n')
        for (let [index, element] of boardinfo.entries()) {
          if (element.includes("Information")) {
            if (!element.endsWith(':')) {
              boardinfo[index] = element + ":"
            }
          }
        }
        this.board.BASE_BOARD_INFO = boardinfo.join('\n')
      }
    },
    getExistBoardPath() {
      // only return filename when using exist configuration.
      return configurator.readDir(this.WorkingFolder, false)
          .then((res) => {
            let boardPath = ''
            res.map((filepath) => {
              if (filepath.path.search('\\.board\\.xml') !== -1) {
                boardPath = filepath.path
              }
            })
            // only return the last vaild boardPath
            return boardPath
          })
    },
    openBoardFileSelectDialog() {
      return configurator.openDialog({
        title: "Open Board XML",
        directory: false,
        multiple: false,
        filters: [{name: "Board XML", extensions: ['xml']}]
      })
    },
    browseForFile() {
      if (this.importing) {
        return
      }
      this.openBoardFileSelectDialog()
          .then((filePath) => {
            if (filePath) {
              return configurator.addHistory('Board', filePath)
            }
          })
          .then(() => this.getBoardHistory())
    },
    getBoardHistory() {
      return this.workspaceStore.loadHistory('Board')
          .then((boardHistory) => {
            if (boardHistory === null) {
              return
            }
            if (boardHistory.length > 0) {
              this.currentSelectedBoard = boardHistory[0]
            }
          })
    }
  }
}
</script>

<style scoped>
.card, .card-header {
  background-color: #F5F5F5;
}
</style>
