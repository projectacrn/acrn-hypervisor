<template>
  <div class="banner">
    <slot></slot>
  </div>
</template>

<script>
export default {
  name: "Banner"
}
</script>

<style scoped>

.banner {
  min-height: 50px;
  background-image: url("/src/assets/images/top_pattern.png"), linear-gradient(90.29deg, #242357 0.08%, #6ABFAE 85.19%, #69BFAD 99.72%);
  background-repeat: no-repeat;
  background-position: 50%;
}
</style>