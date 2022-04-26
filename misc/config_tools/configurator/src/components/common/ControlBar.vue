<template>
  <nav class="navbar navbar-expand navbar-dark bg-navbar" data-tauri-drag-region="true">
    <div data-tauri-drag-region="true" class="container-fluid">
      <span class="navbar-brand">
      <img alt="ACRN"
           src="/src/assets/images/ACRN_Logo.svg"
           height="38"
           data-tauri-drag-region="true"
           class="d-inline-block align-self-center mx-3">
        <text class="d-inline align-bottom logo-text" data-tauri-drag-region="true">Configurator</text>
      </span>
      <div class="controlButtons d-flex justify-content-between align-items-center" data-tauri-drag-region="true">
        <Icon @click="openDevtools" size="20px" color="white">
          <Cog/>
        </Icon>
        <Icon @click="minus" size="20px" color="white">
          <Minus/>
        </Icon>
        <Icon @click="maximize" size="20px" color="white">
          <WindowMaximizeRegular/>
        </Icon>
        <Icon @click="close" size="20px" color="white">
          <Times/>
        </Icon>
      </div>
    </div>
  </nav>
  <slot></slot>
</template>

<script>
import {Icon} from '@vicons/utils'
import {Minus, WindowMaximizeRegular, Times, Cog} from '@vicons/fa'
import {appWindow} from "@tauri-apps/api/window";
import {invoke} from "@tauri-apps/api";


export default {
  name: "ControlBar",
  components: {Icon, Minus, WindowMaximizeRegular, Times, Cog},
  methods: {
    minus() {
      appWindow.minimize()
    },
    maximize() {
      appWindow.isMaximized().then((isMaximize) => {
        if (isMaximize) {
          appWindow.unmaximize()
        } else {
          appWindow.maximize()
        }
      })
    },
    close: () => {
      appWindow.close()
    },
    openDevtools: () => {
      invoke('open_devtools', {});
    }
  }
}
</script>


<style scoped>
.navbar-expand {
  height: 80px;
}

.bg-navbar {
  height: 80px;
  background: #007B81;
  box-shadow: 0 2px 4px rgba(0, 0, 0, 0.15);
  -webkit-user-select: none;
  -webkit-app-region: drag;
}

.logo-text {
  cursor: default;
  font-family: Roboto;
  font-style: normal;
  font-weight: bold;
  font-size: 26px;
  line-height: 30px;
  letter-spacing: 0.04em;
  text-transform: uppercase;

  color: #9ADFD1;
}

.controlButtons {
  margin-right: 30px;
  width: 9rem;
  filter: drop-shadow(3px 3px 2px rgb(0 0 0 / 0.4));
}


.xicon:hover {
  opacity: 0.7;
}

</style>