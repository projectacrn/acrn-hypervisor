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
	this.version = packageVersion
  },
  methods: {
    home() {
      this.$router.push('/')
    },
  }
}
</script>

<style scoped>

</style>
