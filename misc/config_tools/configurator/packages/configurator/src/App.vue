<script setup>
// This starter template is using Vue 3 <script setup> SFCs
// Check out https://vuejs.org/api/sfc-script-setup.html#script-setup
import ControlBar from "./components/common/ControlBar.vue";
import Footer from "./components/common/Footer.vue";
</script>

<template>
  <ControlBar>
    <div class="acrnBody">
      <router-view></router-view>
      <Footer/>
    </div>
  </ControlBar>
</template>

<style lang="scss">
@import "./assets/css/index.scss";

.acrnBody {
  height: calc(100vh - 84px);
  overflow-y: auto;
}
</style>
