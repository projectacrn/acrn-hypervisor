<template>
  <b-modal title="ACRN Configurator" ok-only fade
           v-model="showModal"
           @ok="overWrite"
  >
    <div class="picture">
      <img v-if="isCheckPass(totalMsg)" src="/src-tauri/icons/Config_save_warningfail.png">
      <img v-else src="/src-tauri/icons/Config_save_success.png">
    </div>
    <div class="description">
      <p v-html="totalMsg"></p>
    </div>
  </b-modal>
</template>

<script>
export default {
  name: "SaveScenario",
  props: {
    totalMsg: {
      type: String
    },
    showModal: {
      type: Boolean,
      default: false
    },
  },
  data() {
    return {
      variants: ['primary', 'secondary', 'success', 'warning', 'danger', 'info', 'light', 'dark'],
      headerBgVariant: 'dark',
      headerTextVariant: 'light',
      bodyBgVariant: 'light',
      bodyTextVariant: 'dark',
      footerBgVariant: 'warning',
      footerTextVariant: 'dark',

    }
  },
  methods: {
    overWrite() {
      this.$emit('update:showModal', false)
    },
    isCheckPass(value) {
      var regexp = new RegExp("failed");
      return (value != null) && regexp.test(value);
    }
  }
}
</script>

<style scoped>
.picture img{
  float: left;
  width: 100%;
  object-fit: scale-down;
}
.picture{
  width: 30%;
}
.description{
  float: right;
  width: 65%;
  white-space: pre-line;
}

</style>