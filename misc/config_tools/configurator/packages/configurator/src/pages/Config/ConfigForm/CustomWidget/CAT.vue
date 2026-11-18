<template>
  <div class="py-3">
    <b>Memory Isolation for Performance</b>

    <div class="d-flex gap-2 flex-column">
      <text>Intel Resource Director Technology</text>
      <!-- Recreate the controls after programmatic changes: WebKit can retain
         the checked state of a Bootstrap checkbox whose click was prevented. -->
      <b-form-checkbox
          :key="'RDT-'+RDT_ENABLED"
          :model-value="RDT_ENABLED" :value="'y'" :uncheckedValue="'n'"
          @click="(event)=>checkboxController('RDT_ENABLED',event)">
        <n-popover trigger="hover" placement="top-start">
          <template #trigger>
            <IconInfo/>
          </template>
          <span v-html="this.RDTType.properties.RDT_ENABLED.description"></span>
        </n-popover>
        Cache Allocation Technology (requires CPU Affinity configuration in each desired VM)
      </b-form-checkbox>
      <div class="d-flex flex-column gap-2 ps-3 pb-3">
        <b-form-checkbox
            :key="'CDP-'+CDP_ENABLED"
            :model-value="CDP_ENABLED" :value="'y'" :uncheckedValue="'n'"
            :disabled="VCAT_ENABLED==='y'"
            @click="(event)=>checkboxController('CDP_ENABLED',event)">
          <n-popover trigger="hover" placement="top-start">
            <template #trigger>
              <IconInfo/>
            </template>
            <span v-html="this.RDTType.properties.CDP_ENABLED.description"></span>
          </n-popover>
          Code and Data Prioritization
        </b-form-checkbox>
        <b-form-checkbox
            :key="'VCAT-'+VCAT_ENABLED"
            :model-value="VCAT_ENABLED" :value="'y'" :uncheckedValue="'n'"
            :disabled="CDP_ENABLED==='y'"
            @click="(event)=>checkboxController('VCAT_ENABLED',event)">
          <n-popover trigger="hover" placement="top-start">
            <template #trigger>
              <IconInfo/>
            </template>
            <span v-html="this.RDTType.properties.VCAT_ENABLED.description"></span>
          </n-popover>
          Virtual Cache Allocation Technology (VCAT)
        </b-form-checkbox>
      </div>
    </div>
    <!-- begin CAT Table -->
    <div class="py-3" v-if="CAT_INFO.errorMsg">
      <span v-html="CAT_INFO.errorMsg"></span>
    </div>
    <div class="py-4" v-for="(CACHE_ALLOCATION,index) in CAT_INFO.regions" v-if="RDT_ENABLED==='y'">
      <p v-if="showInstruction(CACHE_ALLOCATION.level,index) ">
        Drag the ends of the boxes to cover the cache chunks you want to allocate to specific VMs. If you have a
        real-time
        VM,ensure its cache chunks do not overlap with any other VM's cache chunks.
      </p>
      <div class="d-flex justify-content-between py-2 align-items-center">
        <text>
          L{{ CACHE_ALLOCATION.level }} Cache Allocation Technology {{
            CAT_INFO.summary[CACHE_ALLOCATION.level].count > 1 ? ' Module ' + CAT_INFO.summary[CACHE_ALLOCATION.level.toString()][CACHE_ALLOCATION.id] : ''
          }}
          (requires CPU affinity to cores {{
            Math.min(...CACHE_ALLOCATION.processors)
          }}~{{ Math.max(...CACHE_ALLOCATION.processors) }} in each desired VM)
        </text>
        <b-button @click="setDefaultClosMask(CACHE_ALLOCATION)">
          Apply basic real-time defaults
        </b-button>
      </div>
      <div class="d-flex">
        <div class="leftTitle">
          <!--left title-->
          <div style="min-height: 36px">
            <!-- for align right -->
          </div>
          <div v-for="(POLICY,index) in CACHE_ALLOCATION.data.RTCore">
            <div v-if="index===0">Real-time</div>
            <text>
              {{ POLICY.VM }} vCPU {{ POLICY.VCPU }}{{ POLICY.TYPE === 'Unified' ? '' : "_" + POLICY.TYPE }}
            </text>
          </div>
          <div v-for="(POLICY,index) in CACHE_ALLOCATION.data.Standard">
            <div v-if="index===0">Standard</div>
            <text>
              {{ POLICY.VM }} vCPU {{ POLICY.VCPU }}{{ POLICY.TYPE === 'Unified' ? '' : "_" + POLICY.TYPE }}
            </text>
          </div>
          <div v-for="(POLICY,index) in CACHE_ALLOCATION.data.VCAT">
            <div v-if="index===0">Virtual CAT</div>
            <text>
              {{ POLICY.VM }}
            </text>
          </div>
        </div>
        <div class="flex-grow-1">
          <div class="d-flex rightTitle">
            <!--right title-->
            <div v-for="n in CACHE_ALLOCATION.capacity_mask_length" class="chunkTitle"
                 :style="{width: (100/(CACHE_ALLOCATION.capacity_mask_length))+'%'}"
            >
              chunk{{ CACHE_ALLOCATION.capacity_mask_length - n }}
            </div>
          </div>
          <div>
            <!--right table-->
            <div v-for="(POLICY,index) in CACHE_ALLOCATION.data.RTCore">
              <div class="policyDisabledBlock"
                   v-if="index===0"></div>
              <HexBlockRangeSelector
                  :model-value="POLICY.CLOS_MASK"
                  @update:model-value="setClosMask(CACHE_ALLOCATION, POLICY, $event)"
                  :isVcat="false"
                  :max="CACHE_ALLOCATION.capacity_mask_length"
              />
            </div>
            <div v-for="(POLICY,index) in CACHE_ALLOCATION.data.Standard">
              <div class="policyDisabledBlock"
                   v-if="index===0"></div>
              <HexBlockRangeSelector
                  :model-value="POLICY.CLOS_MASK"
                  @update:model-value="setClosMask(CACHE_ALLOCATION, POLICY, $event)"
                  :isVcat="false"
                  :max="CACHE_ALLOCATION.capacity_mask_length"
              />
            </div>
            <div v-for="(POLICY,index) in CACHE_ALLOCATION.data.VCAT">
              <div class="policyDisabledBlock"
                   v-if="index===0"></div>
              <HexBlockRangeSelector
                  :model-value="POLICY.CLOS_MASK"
                  @update:model-value="setClosMask(CACHE_ALLOCATION, POLICY, $event)"
                  :isVcat="true"
                  :max="CACHE_ALLOCATION.capacity_mask_length"
              />
            </div>
          </div>
          <div class="pt-4 ps-2">
            Note: each chunk is {{ CACHE_ALLOCATION.cache_size / CACHE_ALLOCATION.capacity_mask_length / 1024 }}KB
          </div>
        </div>

      </div>
    </div>
  </div>
</template>

<script>
import _ from "lodash";
import {vueUtils, fieldProps} from "@lljj/vue3-form-naive";
import HexBlockRangeSelector from "./CAT/HexBlockRangeSelector.vue";
import IconInfo from '@lljj/vjsf-utils/icons/IconInfo.vue';
import {mapStores} from "pinia";
import {confirm} from "@tauri-apps/plugin-dialog";
import {useScenarioStore} from "../../../../stores/scenario";

const wait = ms => new Promise(resolve => setTimeout(resolve, ms));


// noinspection JSUnresolvedVariable
export default {
  name: "CAT",
  components: {HexBlockRangeSelector, IconInfo},
  props: {
    ...fieldProps
  },
  computed: {
    ...mapStores(useScenarioStore),
    RDT_ENABLED() {
      return this.CAT_INFO.switches.RDT_ENABLED
    },
    CDP_ENABLED() {
      return this.CAT_INFO.switches.CDP_ENABLED
    },
    VCAT_ENABLED() {
      return this.CAT_INFO.switches.VCAT_ENABLED
    },
  },
  data() {
    return {
      // Read-only view model derived from the form and Scenario. The form
      // remains the editable source of truth; replacing this snapshot makes every
      // programmatic switch change explicitly visible to the template.
      CAT_INFO: {
        errorMsg: null,
        regions: [],
        summary: {},
        switches: {RDT_ENABLED: 'n', CDP_ENABLED: 'n', VCAT_ENABLED: 'n'},
      },
      RDTType: this.rootSchema.definitions['RDTType'],
      InstructionLocation: {},
      catOperationRevision: 0,
    }
  },
  watch: {
    // Mask edits and external Scenario changes also invalidate the view model.
    'scenarioStore.revision'() {
      this.updateCatInfo()
    },
  },
  created() {
    this.updateCatInfo()
  },
  beforeUnmount() {
    // Delayed enables and native confirmations must not write into a form
    // that has already been replaced by another VM or parameter mode.
    this.catOperationRevision++
  },
  methods: {
    showInstruction(level, index) {
      if (!(level in this.InstructionLocation)) {
        this.InstructionLocation[level] = index
      }
      return this.InstructionLocation[level]===index
    },
    boardUpdate() {
      this.updateCatInfo()
    },
    checkboxController(name, event) {
      // prevent default event
      event.preventDefault()
      event.stopPropagation()
      let oldValue = this.formDataProxy(name);
      let newValue = oldValue === 'y' ? 'n' : 'y';
      const operationRevision = ++this.catOperationRevision
      if (newValue === 'y') {
        wait(50).then(() => {
          if (operationRevision === this.catOperationRevision) {
            this.formDataProxy(name, newValue, true);
          }
        })
        return
      }

      // newValue === 'n'
      // show confirm dialog to confirm disable CAT feature
      let message = "Deselecting Cache Allocation Technology, " +
          "Code and Data Prioritization, or Virtual Cache Allocation Technology will remove any prior configurations. " +
          "To see your current configuration, go to the Hypervisor’s Advanced tab, " +
          "Memory Isolation for Performance section.\n\n" +
          "Are you sure you want to continue?";
      // noinspection JSUnresolvedFunction
      confirm(message)
          .then((confirmed) => {
            if (operationRevision === this.catOperationRevision) {
              this.formDataProxy(name, confirmed ? newValue : oldValue, true)
            }
          })
    },
    formDataProxy(name, data = null, update = false) {
      let path = {
        'RDT_ENABLED': 'FEATURES.RDT.RDT_ENABLED',
        'CDP_ENABLED': 'FEATURES.RDT.CDP_ENABLED',
        'VCAT_ENABLED': 'FEATURES.RDT.VCAT_ENABLED',
      }[name]

      // check parent node exists
      let oldValue = vueUtils.getPathVal(this.rootFormData, path);
      if (oldValue === undefined) {
        let t = path.split('.');
        let parentPath = t.splice(0, t.length - 1).join('.');
        if (!vueUtils.getPathVal(this.rootFormData, parentPath)) {
          vueUtils.setPathVal(this.rootFormData, parentPath, {});
        }
        // set to checkbox default value
        vueUtils.setPathVal(this.rootFormData, path, 'n');
      }
      // if data is not empty, set value
      if (data !== null) {
        vueUtils.setPathVal(this.rootFormData, path, data)

        // if data is not empty, set value as expected and update CAT_INFO
        if (update) {
          switch (name) {
            case 'RDT_ENABLED':
              if (data === 'n') {
                this.formDataProxy('CDP_ENABLED', 'n');
                this.formDataProxy('VCAT_ENABLED', 'n');
              }
              break;
            case 'CDP_ENABLED':
              if (data === 'y') {
                this.formDataProxy('RDT_ENABLED', 'y');
                this.formDataProxy('VCAT_ENABLED', 'n');
              }
              break;
            case 'VCAT_ENABLED':
              if (data === 'y') {
                this.formDataProxy('RDT_ENABLED', 'y');
                this.formDataProxy('CDP_ENABLED', 'n');
              }
              break;
          }
          // do update
          this.updateCatInfo()
        }
      }
      return vueUtils.getPathVal(this.rootFormData, path)
    },
    setDefaultClosMask(CACHE_REGION) {
      if (CACHE_REGION.data.RTCore.length === 0) {
        return;
      }
      if (CACHE_REGION.capacity_mask_length < (CACHE_REGION.data.RTCore.length + 1)) {
        alert('Can\'t generate default settings for this region(due to too many realtime cpu)')
        return;
      }
      for (let policyIndex = 0; policyIndex < CACHE_REGION.data.RTCore.length; policyIndex++) {
        const policy = CACHE_REGION.data.RTCore[policyIndex]
        this.setClosMask(CACHE_REGION, policy, '0x' + parseInt(
            '0'.repeat(policyIndex) + '1' + '0'.repeat(CACHE_REGION.capacity_mask_length - policyIndex - 1),
            2).toString(16))
      }

      for (let policyIndex = 0; policyIndex < CACHE_REGION.data.Standard.length; policyIndex++) {
        const policy = CACHE_REGION.data.Standard[policyIndex]
        this.setClosMask(CACHE_REGION, policy, '0x' + parseInt(
            '0'.repeat(CACHE_REGION.data.RTCore.length) + '1'.repeat(CACHE_REGION.capacity_mask_length - CACHE_REGION.data.RTCore.length),
            2).toString(16));
      }
    },
    setClosMask(CACHE_REGION, POLICY, CLOS_MASK) {
      this.scenarioStore.setClosMask(CACHE_REGION, POLICY, CLOS_MASK)
    },
    updateCatInfo() {
      this.scenarioStore.correctCatSwitches()
      const view = this.scenarioStore.getCatView() || {errorMsg: null, regions: [], summary: {}}
      this.CAT_INFO = {
        ...view,
        switches: {
          RDT_ENABLED: this.formDataProxy('RDT_ENABLED'),
          CDP_ENABLED: this.formDataProxy('CDP_ENABLED'),
          VCAT_ENABLED: this.formDataProxy('VCAT_ENABLED'),
        },
      }
      this.InstructionLocation={}
    }
  }
}
</script>

<style scoped>
.leftTitle {
  min-width: 180px;
  white-space: nowrap;
  padding-right: 1rem;
}

.leftTitle div {
  line-height: 33px;
}

.leftTitle div div {
  font-weight: 800;
}

.chunkTitle {
  border: 1px solid white;
  padding: 7px 0 0;
  min-height: 36px;
}

.rightTitle {
  background: #4A6AAA;
  color: white;
  text-align: center;
  font-size: 12px;
}

.policyDisabledBlock {
  height: 33px;
  width: 100%;
  background: #bfbfbf
}

/*noinspection CssUnusedSymbol*/
.form-check {
  display: flex;
  align-items: center;
  gap: 8px;
}
</style>
