<template>
  <div class="p-2 w-100 d-flex align-items-center TabBox">
    <div class="d-inline-block p-3 pt-2 Tab position-relative"
         :class="{Active:activeVMID===-1}"
         @click="active(-1)">
      <div class="position-absolute" style="right: 3px;top:5px;" v-if="errors.hasOwnProperty(-1)">
        <Icon size="18px" color="red" style="cursor: pointer;">
          <ExclamationCircle/>
        </Icon>
      </div>
      <div style="font-size: 22px;">Hypervisor</div>
      <div>Global Settings</div>
    </div>
    <div class="TabSplitter"></div>

    <div class="d-inline-block p-3 pt-2 Tab position-relative" :class="{Active:activeVMID===pre.id}"
         :key="pre.id" v-for="pre in vms.PRE_LAUNCHED_VM" @click="active(pre.id)">
      <div class="position-absolute" style="right: 3px;top:5px;" v-if="errors.hasOwnProperty(pre.id)">
        <Icon size="18px" color="red" style="cursor: pointer;">
          <ExclamationCircle/>
        </Icon>
      </div>
      <div style="font-size:22px">
        {{ pre.name }}
      </div>
      <div>Pre-Launched</div>
    </div>

    <div class="d-inline-block p-3 pt-2 TabAdd" @click="addVM('PRE_LAUNCHED_VM')">
      <div style="font-size: 22px;">
        <img src="/src/assets/images/Plus.svg" alt="Add Pre-launched VM">
      </div>
      <div>Pre-launched VM</div>
    </div>


    <div class="d-inline-block p-3 pt-2 Tab position-relative" :class="{Active:activeVMID===service.id}"
         :key="service.id" v-for="service in vms.SERVICE_VM"
         @click="active(service.id)">
      <div class="position-absolute" style="right: 3px;top:5px;" v-if="errors.hasOwnProperty(service.id)">
        <Icon size="18px" color="red" style="cursor: pointer;">
          <ExclamationCircle/>
        </Icon>
      </div>
      <div style="font-size:22px">
        {{ service.name }}
      </div>
      <div>ServiceVM</div>
    </div>


    <div class="d-inline-block p-3 pt-2 Tab position-relative" :class="{Active:activeVMID===post.id}"
         :key="post.id" v-for="post in vms.POST_LAUNCHED_VM"
         @click="active(post.id)">
      <div class="position-absolute" style="right: 3px;top:5px;" v-if="errors.hasOwnProperty(post.id)">
        <Icon size="18px" color="red" style="cursor: pointer;">
          <ExclamationCircle/>
        </Icon>
      </div>
      <div style="font-size:22px">
        {{ post.name }}
      </div>
      <div>Post-Launched</div>
    </div>

    <div class="d-inline-block p-3 pt-2 TabAdd" @click="addVM('POST_LAUNCHED_VM')">
      <div style="font-size: 22px;">
        <img src="/src/assets/images/Plus.svg" alt="Add Post-launched VM">
      </div>
      <div>Post-launched VM</div>
    </div>
  </div>
</template>

<script>
import {Icon} from "@vicons/utils";
import {ExclamationCircle} from "@vicons/fa";

export default {
  name: "TabBox",
  components: {Icon, ExclamationCircle},
  props: {
    scenario: {
      type: Object
    },
    errors: {
      type: Object
    },
    activeVMID: {type: Number}
  },
  emits: ['tabActive', 'addVM'],
  computed: {
    vms() {
      let vmInfo = {
        PRE_LAUNCHED_VM: [],
        SERVICE_VM: [],
        POST_LAUNCHED_VM: []
      }
      // debugger
      this.scenario.vm.map((vmConfig) => {
        vmInfo[vmConfig['load_order']].push({
          id: vmConfig['@id'],
          name: vmConfig.name,
        })
      })
      return vmInfo;
    }
  },
  methods: {
    active(vmid) {
      this.$emit('tabActive', vmid)
    },
    addVM(load_order) {
      this.$emit('addVM', load_order)
    }
  }
}
</script>

<style scoped>

.TabBox {
  display: flex;
  user-select: none;
  border: 1px solid #007B81;
  box-sizing: border-box;
  border-radius: 5px;

  overflow-x: auto;
}

.TabBox .Tab {
  background: #E1F2EF;
  border-radius: 5px;
  color: #007B81;
  border: 1px solid #007B81;
  margin: 0 0.5rem 0 0;
  cursor: pointer;
  white-space: nowrap;
}

.TabBox .Tab.Active {
  background: #007B81;
  color: white;
}

.TabAdd {
  cursor: pointer;
  white-space: nowrap;
}

.TabSplitter {
  height: 74px;
  width: 0;
  border-right: 1px solid #C4C4C4;
  margin: 0 0.5rem;
}

</style>