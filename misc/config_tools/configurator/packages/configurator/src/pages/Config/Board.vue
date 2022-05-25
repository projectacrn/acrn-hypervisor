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
              <div class="py-1 col-sm">{{ this.imported ? this.board.BIOS_INFO : '' }}</div>
              <div class="py-1 col-sm">{{ this.imported ? this.board.BASE_BOARD_INFO : '' }}</div>
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
import NewBoard from "./NewBoard.vue";

export default {
  name: "Board",
  components: { NewBoard },
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
      boardHistory: [],
      currentSelectedBoard: '',
      showBoardOverwrite: false,
      newFilePath: '',
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
  computed: {
    imported() {
      return !_.isEmpty(this.board)
    }
  },
  methods: {
    importBoardButton() {
      if (this.imported) {
        this.openBoardFileSelectDialog()
        .then((filePath) => {
          if (filePath.length > 0) {
            if (this.currentSelectedBoard.length > 0) {
                if (filePath != this.currentSelectedBoard) {
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
    importBoard(useNewFile = false) {
      let filepath = ''
      if (useNewFile) {
        filepath = this.newFilePath
      } else {
        filepath = this.currentSelectedBoard
      }
      if (filepath.length > 0) {
        configurator.loadBoard(filepath)
            .then(({scenarioJSONSchema, boardInfo}) => {
              this.$emit('boardUpdate', boardInfo, scenarioJSONSchema);
              let boardFileNewPath = this.WorkingFolder + boardInfo.name;
              // Todo: use rust command writeBoard to fix bugs.
              configurator.writeFile(boardFileNewPath, boardInfo.content)
                  .then(() => configurator.addHistory('Board', boardFileNewPath))
                  .then(() => this.getBoardHistory())
            })
            .catch((err)=> {
              alert(`Loading ${filepath} failed: ${err}`)
              console.log(err)
            })
      }
    },
    getExistBoardPath() {
    // only return filename when using exist configuration.
    return configurator.readDir(this.WorkingFolder, false)
      .then((res) => {
        let boardPath = ''
        res.map((filepath) => {
          if (filepath.path.search('\\.board\\.xml') != -1) {
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
      this.openBoardFileSelectDialog()
         .then((filePath) => configurator.addHistory('Board', filePath))
         .then(() => this.getBoardHistory())
    },
    getBoardHistory() {
      return configurator.getHistory("Board")
          .then((boardHistory) => {
            this.boardHistory = boardHistory;
            if (this.boardHistory.length > 0) {
              this.currentSelectedBoard = this.boardHistory[0]
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
