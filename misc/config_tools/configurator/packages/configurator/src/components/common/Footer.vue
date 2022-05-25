<template>
  <div class="pt-3">
    <p class="text-center text-secondary">
      © Copyright Project ACRN™, a Series of LF Projects, LLC.
      <text v-if="version">- Version {{ version }}</text>
    </p>
  </div>
</template>

<script>
export default {
  name: "Footer",
  data: () => {
    return {
      version: ''
    }
  },
  mounted() {
    this.version = packageVersion
  }
}
</script>

<style scoped>

</style>
