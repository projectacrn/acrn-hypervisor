<template>
  <div>
    <b-modal id="my-modal" no-close-on-backdrop
             title="Scenario XML Overwrite" v-model="showModal"
             @cancel="cancel"
             @hidden="cancel"
             @abort="cancel"
             @close="cancel"
             @ok="overWrite"
             @submit="overWrite"
             ok-title="Overwrite">
      <div>Are you sure you want to overwrite the scenario XML file in this working folder?</div>
      <br>
      <div>if you continue, the scenario XML file in the following working folder will be overwritten:
        <br>{{ usingWorkingFolder }}
      </div>
      <br>
      <div>(You must save your configuration to generate new launch scripts associated to your new scenario XML file.)
      </div>
    </b-modal>
  </div>
</template>

<script>
export default {
  name: 'OverwriteMessage',
  props: {
    usingWorkingFolder: String,
    showModal: {
      type: Boolean,
      default: false
    },
  },
  emits: {'overWrite': null},
  data() {
    return {
      show: false,
      variants: ['primary', 'secondary', 'success', 'warning', 'danger', 'info', 'light', 'dark'],
      headerBgVariant: 'dark',
      headerTextVariant: 'light',
      bodyBgVariant: 'light',
      bodyTextVariant: 'dark',
      footerBgVariant: 'warning',
      footerTextVariant: 'dark'
    }
  },
  methods: {
    cancel() {
      this.$emit('update:showModal', false)
    },
    overWrite() {
      this.$emit('update:showModal', false)
      this.$emit("overWrite")
    },
  }
}
</script>
