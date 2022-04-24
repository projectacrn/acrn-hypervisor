<template>
  <div class="pt-3">
    <p class="text-center text-secondary">
      © Copyright Project ACRN™, a Series of LF Projects, LLC.
      <text v-if="version" @click="home">- Version {{ version }}</text>
    </p>
  </div>
</template>

<script>
import {getVersion} from "@tauri-apps/api/app";


export default {
  name: "Footer",
  data: () => {
    return {
      version: ''
    }
  },
  mounted() {
    this.getAppVersion()
  },
  methods: {
    home() {
      this.$router.push('/')
    },
    getAppVersion() {
      getVersion().then((version) => {
        this.version = version
      })
    }
  }
}
</script>

<style scoped>

</style>