<template>
  <b-modal title="Board XML overwrite" fade no-close-on-backdrop
           v-model="showModal"
           @cancel="cancel"
           @abort="cancel"
           @close="cancel"
           @ok="overWrite"
           @submit="overWrite"
           @hidden="cancel"
  >
  <div>
    <p><b>Are you sure you want to overwrite the board XML file in this working folder?</b></p>
    <p>If you continue, the board XML file in the following working folder will be overwritten:</p>
    <p>{{this.$parent.currentSelectedBoard}}</p>
    <p>(You may need to change your configuration settings to be compatible with the new board XML file.)</p>
  </div>
  </b-modal>
</template>

<script>
  export default {
    name: "NewBoard",
    props: {
      showModal: {
        type: Boolean,
        default: false
      },
    },
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
        this.$parent.importBoard(true)
      },
    }
  }
</script>
