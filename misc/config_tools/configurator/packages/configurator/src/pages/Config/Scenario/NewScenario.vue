<template>
  <b-modal id="newScenarioModal" size="xl" title="Create a New Scenario" fade no-close-on-backdrop
           v-model="showModal"
           @cancel="cancel"
           @abort="cancel"
           @close="cancel"
           @ok="createNewScenario"
           @submit="createNewScenario"
           @hidden="cancel"
  >
    <template #header-close>
      <Icon @click="cancel">
        <Times/>
      </Icon>
    </template>

    <b-form class="p-4">
      <div class="form-group p-4 bg-gray" style="border:1px solid #D7D7D7;">

        <div class="form-group pb-4">
          <p class="d-flex justify-content-between">
            <b>Choose a scenario type:</b>
            <a class="fs-6"
               target="_blank"
               :href="'https://projectacrn.github.io/'+version+'/introduction/index.html#static-configuration-based-on-scenarios'">
              Learn about scenarios
            </a>
          </p>

          <b-form-radio class="mb-3" v-model="scenarioTemplate" value="shared">
            Shared <i>(Post-launched VMs only)</i>
          </b-form-radio>

          <b-form-radio class="mb-3" v-model="scenarioTemplate" value="partitioned">
            Partitioned <i>(Pre-launched VMs only)</i>
          </b-form-radio>

          <b-form-radio class="mb-3" v-model="scenarioTemplate" value="hybrid">
            Hybrid <i>(Both pre-launched and post-launched VMs)</i>
          </b-form-radio>

        </div>

        <b>VM Configuration</b>
        <p class="mt-3">This system’s board supports a maximum of 8 VMs.</p>
        <p>
          How many of each VM type do you want in your scenario? (You can change these later.)
        </p>

        <div style="max-width:332px"
             class="px-4 ms-2 vmNum d-flex justify-content-between align-items-center flex-wrap align-content-between">
          <b v-if="scenarioTemplate!=='shared'">Pre-launch VMs:</b>
          <b-form-input v-if="scenarioTemplate!=='shared'" type="number" min="0" max="8" v-model="preLaunch"/>
          <b v-if="scenarioTemplate!=='partitioned'">Service VM:</b>
          <b-form-input v-if="scenarioTemplate!=='partitioned'" disabled type="number" model-value="1"/>
          <b v-if="scenarioTemplate!=='partitioned'">Post-launch VMs:</b>
          <b-form-input v-if="scenarioTemplate!=='partitioned'" type="number" min="0" max="8" v-model="postLaunch"/>
        </div>
      </div>
    </b-form>
  </b-modal>
</template>

<script>
import {Icon} from '@vicons/utils';
import {Times} from "@vicons/fa";
import configurator from "../../../lib/acrn";

export default {
  name: "NewScenario",
  components: {Icon, Times},
  props: {
    showModal: {
      type: Boolean,
      default: false
    },
    scenario: {
      type: Object
    }
  },
  data() {
    return {
      version: branchVersion,
      scenarioTemplate: "shared",
      preLaunch: 1,
      postLaunch: 1
    }
  },
  methods: {
    cancel() {
      this.$emit('update:showModal', false)
    },
    createNewScenario() {
      this.$emit('update:showModal', false)

      // get vm num
      let pre = 0
      let service = 0
      let post = 0
      if (this.scenarioTemplate === 'shared') {
        service = 1;
        post = this.postLaunch
      } else if (this.scenarioTemplate === 'partitioned') {
        pre = this.preLaunch
      } else if (this.scenarioTemplate === 'hybrid') {
        pre = this.preLaunch;
        service = 1;
        post = this.postLaunch
      } else {
        alert(`BUG caught, scenarioTemplate ${this.scenarioTemplate} Not found!`);
        return;
      }

      let newScenario = configurator.createNewScenario(pre, service, post);
      this.$emit('newScenario', newScenario);
    }
  }

}
</script>

<style scoped>
.vmNum input {
  max-width: 69px;
  text-align: center;
}

.vmNum b {
  padding: 0 0;
}

.vmNum b, .vmNum input {
  flex: 0 1 50%;
}
</style>