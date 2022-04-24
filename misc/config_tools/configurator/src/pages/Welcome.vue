<template>
  <div id="body" class="container-fluid">
    <Banner>
      <div class="banner-text ms-4 ps-3 py-4">
        ACRN Configurator helps you set up and customize your ACRN hypervisor and VMs.
      </div>
    </Banner>

    <div class="container-xxl">
      <div class="row py-4">
        <div class="d-flex justify-content-center border-end-sm col-sm">
          <div class="py-3 py-sm-0 px-3" style="width: 100%;">
            <NewConfiguration/>
          </div>
        </div>

        <div class="d-flex justify-content-center col-sm">
          <div class="py-3 py-sm-0 px-3" style="width: 100%">
            <UseExisting/>
          </div>
        </div>

      </div>

    </div>

  </div>
</template>

<script>
import Banner from "../components/common/Banner.vue";
import NewConfiguration from "./Welcome/NewConfiguration.vue";
import UseExisting from "./Welcome/UseExisting.vue";

export default {
  name: "Welcome",
  components: {Banner, NewConfiguration, UseExisting}
}
</script>

<style scoped>

</style>