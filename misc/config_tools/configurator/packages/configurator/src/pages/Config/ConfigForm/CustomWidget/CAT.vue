<template>
  <div class="py-3">
    <b>Memory Isolation for Performance</b>

    <div class="py-3">
      <b-form-checkbox
          v-model="SSRAM_ENABLED" :value="'y'" :uncheckedValue="'n'"
          :disabled="RDT_ENABLED==='y'"
      >
        Software SRAM (for real-time apps)
      </b-form-checkbox>
    </div>
    <div class="d-flex gap-2 flex-column">
      <text>Intel Resource Director Technology</text>
      <b-form-checkbox
          v-model="RDT_ENABLED" :value="'y'" :uncheckedValue="'n'"
          :disabled="SSRAM_ENABLED==='y'"
      >
        Cache Allocation Technology (requires CPU Afinity configuration in each desired VM)
      </b-form-checkbox>
      <div class="d-flex flex-column gap-2 ps-3 pb-3">
        <b-form-checkbox
            v-model="CDP_ENABLED" :value="'y'" :uncheckedValue="'n'"
            :disabled="SSRAM_ENABLED==='y'||VCAT_ENABLED==='y'">
          Code and Data Prioritization
        </b-form-checkbox>
        <b-form-checkbox
            v-model="VCAT_ENABLED" :value="'y'" :uncheckedValue="'n'"
            :disabled="SSRAM_ENABLED==='y'||CDP_ENABLED==='y'">
          Virtual Cache Allocation Technology (VCAT)
        </b-form-checkbox>
      </div>
    </div>
    <p>
      L3 Cache Allocation Technology
      <br/>
      Drag the ends of the boxes to cover the cache chunks you want to allocate to specific VMs. If you have a real-time
      VM,ensure its cache chunks do not overlap with any other VM's cache chunks.
    </p>
    <div class="py-4" v-for="CACHE_ALLOCATION in CAT_INFO" v-if="RDT_ENABLED==='y'">
      <div class="d-flex justify-content-between py-2 align-items-center">
        <text>
          L{{ CACHE_ALLOCATION.level }} Cache Allocation Technology {{
            cat_level_region_sum[CACHE_ALLOCATION.level].count > 1 ? ' Module ' + cat_level_region_sum[CACHE_ALLOCATION.level][CACHE_ALLOCATION.id] : ''
          }}
          (pCPU : {{ Math.min(...CACHE_ALLOCATION.processors) }}~{{ Math.max(...CACHE_ALLOCATION.processors) }})
        </text>
        <b-button @click="setDefaultClosMask(CACHE_ALLOCATION)">
          Apply basic real-time defaults
        </b-button>
      </div>
      <div class="d-flex">
        <div class="leftTitle">
          <!--left title-->
          <div v-for="(POLICY,index) in CACHE_ALLOCATION.data.POLICY">
            <div v-if="index===0&&CACHE_ALLOCATION.real_time_count>0">Real-time</div>
            <div v-if="index===CACHE_ALLOCATION.real_time_count&&CACHE_ALLOCATION.cat_count>0">Standard</div>
            <div v-if="index===CACHE_ALLOCATION.cat_count">Virtual CAT</div>
            <text v-if="index<CACHE_ALLOCATION.cat_count">
              {{ POLICY.VM }} vCPU {{ POLICY.VCPU }}{{ POLICY.TYPE === 'Unified' ? '' : "_" + POLICY.TYPE }}
            </text>
            <text v-else>
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
            <div v-for="(POLICY,index) in CACHE_ALLOCATION.data.POLICY">
              <div class="policyDisabledBlock"
                   v-if="index===CACHE_ALLOCATION.real_time_count && CACHE_ALLOCATION.real_time_count>0"></div>
              <div class="policyDisabledBlock"
                   v-if="index===CACHE_ALLOCATION.cat_count && CACHE_ALLOCATION.cat_count>0"></div>
              <HexBlockRangeSelector
                  v-model="POLICY.CLOS_MASK"
                  :isVcat="index>=CACHE_ALLOCATION.cat_count"
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

function count(source, target) {
  return (source.match(new RegExp(target, 'g')) || []).length;
}

// noinspection JSUnusedLocalSymbols
export default {
  name: "CAT",
  components: {HexBlockRangeSelector},
  props: {
    ...fieldProps
  },
  mounted() {
    this.updateCatInfo()
  },
  computed: {
    VCAT_ENABLED: {
      get() {
        return vueUtils.getPathVal(this.rootFormData, 'FEATURES.RDT.VCAT_ENABLED')
      },
      set(value) {
        vueUtils.setPathVal(this.rootFormData, 'FEATURES.RDT.VCAT_ENABLED', value)
        this.updateCatInfo()
      }
    },
    SSRAM_ENABLED: {
      get() {
        return vueUtils.getPathVal(this.rootFormData, 'FEATURES.SSRAM.SSRAM_ENABLED')
      },
      set(value) {
        vueUtils.setPathVal(this.rootFormData, 'FEATURES.SSRAM.SSRAM_ENABLED', value)
      }
    },
    RDT_ENABLED: {
      get() {
        return vueUtils.getPathVal(this.rootFormData, 'FEATURES.RDT.RDT_ENABLED')
      },
      set(value) {
        vueUtils.setPathVal(this.rootFormData, 'FEATURES.RDT.RDT_ENABLED', value)
        this.updateCatInfo()
      }
    },
    CDP_ENABLED: {
      get() {
        return vueUtils.getPathVal(this.rootFormData, 'FEATURES.RDT.CDP_ENABLED')
      },
      set(value) {
        vueUtils.setPathVal(this.rootFormData, 'FEATURES.RDT.CDP_ENABLED', value)
        this.SSRAM_ENABLED = 'n'
        this.VCAT_ENABLED = 'n'
        if (this.RDT_ENABLED !== value) {
          this.RDT_ENABLED = value
        } else {
          this.updateCatInfo()
        }
      }
    }
  },
  watch: {
    CAT_INFO: {
      handler(newValue, _) {
        if (newValue === null) {
          // set formData CACHE_REGION to null
          vueUtils.setPathVal(this.rootFormData, this.curNodePath, newValue)
          return;
        }
        let data = [];
        for (let i = 0; i < newValue.length; i++) {
          data.push(newValue[i].data)
        }
        // set formData CACHE_REGION.CACHE_ALLOCATION to data
        let CACHE_REGION = {CACHE_ALLOCATION: data}
        vueUtils.setPathVal(this.rootFormData, this.curNodePath, CACHE_REGION)
      },
      deep: true
    },
  },
  data() {
    return {
      CAT_INFO: null,
      cat_level_region_sum: {}
    }
  },
  methods: {
    setDefaultClosMask(CACHE_REGION) {
      if (CACHE_REGION.capacity_mask_length < (CACHE_REGION.real_time_count + 1)) {
        alert('Can\'t generate default settings for this region(due to too many realtime cpu)')
        return;
      }
      for (let policyIndex = 0; policyIndex < CACHE_REGION.data.POLICY.length; policyIndex++) {
        if (policyIndex < CACHE_REGION.real_time_count) {
          CACHE_REGION.data.POLICY[policyIndex].CLOS_MASK = '0x' + parseInt(
              '0'.repeat(policyIndex) + '1' + '0'.repeat(CACHE_REGION.capacity_mask_length - policyIndex - 1),
              2).toString(16)
        } else {
          CACHE_REGION.data.POLICY[policyIndex].CLOS_MASK = '0x' + parseInt(
              '0'.repeat(CACHE_REGION.real_time_count) + '1'.repeat(CACHE_REGION.capacity_mask_length - CACHE_REGION.real_time_count),
              2).toString(16)
        }
      }
    },
    updateCatInfo() {
      // Intel Resource Director Tech
      // Intel Resource Director Technology (RDT) provides cache and memory bandwidth allocation features. The features can be used to improve an application's real-time performance.
      //
      //
      // FEATURES.RDT.RDT_ENABLED
      // Intel Resource Director Tech：
      //
      // FEATURES.RDT.CDP_ENABLED
      // Code and Data Prioritization：
      //
      // FEATURES.RDT.VCAT_ENABLED
      // Virtual Cache Allocation Tech：
      //
      // Software SRAM
      // Configure Software SRAM. This feature reserves memory buffers as always-cached memory to improve an application's real-time performance.
      //
      //
      // FEATURES.SSRAM.SSRAM_ENABLED
      // Software SRAM：


      // get settings from formData
      let RDT_ENABLED = this.RDT_ENABLED === 'y'
      let CDP_ENABLED = this.CDP_ENABLED === 'y'
      let VCAT_ENABLED = this.VCAT_ENABLED === 'y'

      if (!RDT_ENABLED) {
        this.CAT_INFO = null
        return
      }

      // get CAT info from board xml
      let board_cat_info = window.getBoardData().CAT_INFO;

      // noinspection JSUnusedLocalSymbols
      let board_cat_info_example = [
        {
          "id": "0x0", "level": 3, "type": "3", "cache_size": 31457280, "capacity_mask_length": 12,
          "processors": [0, 8, 1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15]
        }
      ]

      // get scenario pcpu config
      let pcpu_vms = {}
      // noinspection JSUnusedLocalSymbols
      let pcpu_vms_example = {
        0: [
          {"VM": "POST_VM_1", "VCPU": 0},
          {"VM": "POST_VM_2", "VCPU": 2}
        ],
        1: [
          {"VM": "POST_VM_1", "VCPU": 1},
          {"VM": "POST_VM_5", "VCPU": 2}
        ]
      }

      let vCats = []
      // noinspection JSUnusedLocalSymbols
      let vCatsExample = [
        {"VM": "VM_C", "VCPU": 0, "CLOS_MASK": 2}
      ]

      window.getCurrentScenarioData().vm.map((vmConfig) => {
        if (
            !vmConfig.hasOwnProperty('cpu_affinity') ||
            !vmConfig.cpu_affinity.hasOwnProperty('pcpu') ||
            !_.isArray(vmConfig.cpu_affinity.pcpu)
        ) {
          return
        }

        // noinspection JSUnresolvedVariable
        if (
            VCAT_ENABLED &&
            vmConfig.hasOwnProperty('virtual_cat_support') &&
            vmConfig.virtual_cat_support === "y"
        ) {
          // noinspection JSUnresolvedVariable
          vCats.push({"VM": vmConfig.name, "VCPU": 0, "CLOS_MASK": vmConfig.virtual_cat_number})
          // for enabled virtual_cat_support vm, it doesn't need set CAT
          return;
        }

        vmConfig.cpu_affinity.pcpu.map((pcpu, index) => {
          if (!pcpu_vms.hasOwnProperty(pcpu.pcpu_id)) {
            pcpu_vms[pcpu.pcpu_id] = {'y': [], 'n': []}
          }
          pcpu_vms[pcpu.pcpu_id][
              // old scenario may not have this attr
              pcpu.real_time_vcpu ?
                  // if it had this attr, use it
                  pcpu.real_time_vcpu :
                  // doesn't have it, auto set to no
                  'n'
              ].push({
            "VM": vmConfig.name,
            "VCPU": index
          })
        })
      })

      let scenarioHVCACHE_REGIONData = vueUtils.getPathVal(this.rootFormData, this.curNodePath);
      // noinspection JSUnusedLocalSymbols
      let scenarioHVCACHE_REGIONData_data_example = {
        "CACHE_ALLOCATION": [
          {
            "CACHE_ID": "0x8", "CACHE_LEVEL": 2, "POLICY": [
              {"VM": "POST_RT_VM1", "VCPU": 0, "TYPE": "Unified", "CLOS_MASK": "0x0fff"},
              {"VM": "POST_RT_VM1", "VCPU": 1, "TYPE": "Unified", "CLOS_MASK": "0x0fff"},
              {"VM": "VM4-RTVM2", "VCPU": 0, "TYPE": "Unified", "CLOS_MASK": "0x0fff"},
              {"VM": "VM4-RTVM2", "VCPU": 1, "TYPE": "Unified", "CLOS_MASK": "0x0fff"}
            ]
          },
          {
            "CACHE_ID": "0x9", "CACHE_LEVEL": 2, "POLICY": [
              {"VM": "VM5-RTVM3", "VCPU": 0, "TYPE": "Unified", "CLOS_MASK": "0x0fff"},
              {"VM": "VM5-RTVM3", "VCPU": 1, "TYPE": "Unified", "CLOS_MASK": "0x0fff"},
              {"VM": "VM6-RTVM4", "VCPU": 0, "TYPE": "Unified", "CLOS_MASK": "0x0fff"},
              {"VM": "VM6-RTVM4", "VCPU": 1, "TYPE": "Unified", "CLOS_MASK": "0x0fff"}
            ]
          }
        ]
      }

      // load data from scenario
      let scenario_cat_data = {}
      // noinspection JSUnresolvedVariable
      if (
          scenarioHVCACHE_REGIONData !== null &&
          scenarioHVCACHE_REGIONData.hasOwnProperty('CACHE_ALLOCATION') &&
          _.isArray(scenarioHVCACHE_REGIONData.CACHE_ALLOCATION)
      ) {
        // noinspection JSUnresolvedVariable
        scenarioHVCACHE_REGIONData.CACHE_ALLOCATION.map((cache_region) => {
          if (!scenario_cat_data.hasOwnProperty(cache_region['CACHE_LEVEL'])) {
            scenario_cat_data[cache_region['CACHE_LEVEL']] = {}
          }
          scenario_cat_data[cache_region['CACHE_LEVEL']][cache_region['CACHE_ID']] = cache_region
        })
      }

      // noinspection JSUnusedLocalSymbols
      let scenario_cat_data_example = {
        2: {
          '0x7': {
            "CACHE_ID": "0x7", "CACHE_LEVEL": 2, "POLICY": [
              {"VM": "POST_RT_VM1", "VCPU": 0, "TYPE": "Unified", "CLOS_MASK": "0x0fff"}
            ]
          },
          '0x8': {
            "CACHE_ID": "0x8", "CACHE_LEVEL": 2, "POLICY": [
              {"VM": "POST_RT_VM1", "VCPU": 0, "TYPE": "Unified", "CLOS_MASK": "0x0fff"}
            ]
          }
        },
        3: {
          '0x0': {}
        }
      }

      this.cat_level_region_sum = {}
      // noinspection JSUnusedLocalSymbols
      let cat_level_region_sum_example = {
        2: {
          count: 2,
          '0x8': 1,
          '0x9': 2
        },
        3: {
          count: 1,
          '0x0': 1
        }
      }
      board_cat_info.map((cat_region_info) => {
        // count regions for each cat level
        if (!this.cat_level_region_sum.hasOwnProperty(cat_region_info.level)) {
          this.cat_level_region_sum[cat_region_info.level] = {count: 0};
        }
        this.cat_level_region_sum[cat_region_info.level].count += 1
        this.cat_level_region_sum[cat_region_info.level][cat_region_info.id] = this.cat_level_region_sum[cat_region_info.level].count;

        // get vm cpu clos_mask from scenario
        let vmCPUClosMasks = {}
        // noinspection JSUnusedLocalSymbols
        let vmCPUClosMasks_example = {
          'VM_NAME': {
            0: {"Unified": '0xfff'},
            1: {"Code": '0xff0', "Data": '0x00f'} // CDP_ENABLED
          }
        }
        if (
            scenario_cat_data.hasOwnProperty(cat_region_info.level) &&
            scenario_cat_data[cat_region_info.level].hasOwnProperty(cat_region_info.id)
        ) {
          let current_region_scenario_cat_data = scenario_cat_data[cat_region_info.level][cat_region_info.id];
          // noinspection JSUnusedLocalSymbols
          let current_region_scenario_cat_data_example = {
            "CACHE_ID": "0x7", "CACHE_LEVEL": 2, "POLICY": [
              {"VM": "POST_RT_VM1", "VCPU": 0, "TYPE": "Unified", "CLOS_MASK": "0x0fff"}
            ]
          }
          for (let i = 0; i < current_region_scenario_cat_data.POLICY.length; i++) {
            let currentRegionScenarioPolicy = current_region_scenario_cat_data.POLICY[i]
            if (!vmCPUClosMasks.hasOwnProperty(currentRegionScenarioPolicy.VM)) {
              vmCPUClosMasks[currentRegionScenarioPolicy.VM] = {}
            }
            if (!vmCPUClosMasks[currentRegionScenarioPolicy.VM].hasOwnProperty(currentRegionScenarioPolicy.VCPU)) {
              vmCPUClosMasks[currentRegionScenarioPolicy.VM][currentRegionScenarioPolicy.VCPU] = {}
            }
            if (["Unified", "Code", "Data"].indexOf(currentRegionScenarioPolicy.TYPE) >= 0) {
              vmCPUClosMasks[currentRegionScenarioPolicy.VM][currentRegionScenarioPolicy.VCPU][currentRegionScenarioPolicy.TYPE] = currentRegionScenarioPolicy.CLOS_MASK
            }
          }
        }

        cat_region_info['data'] = {
          "CACHE_ID": cat_region_info.id,
          "CACHE_LEVEL": cat_region_info.level,
          "POLICY": []
        }

        function addCATPolicy(cpu_policies_line, line_type, vcat_mask_length = null) {
          cpu_policies_line['TYPE'] = line_type;
          let clos_mask = "0x" + parseInt('1'.repeat(
              // if vcat_mask_length is null
              vcat_mask_length === null ?
                  // filled by capacity_mask_length
                  cat_region_info.capacity_mask_length :
                  // filled by vcat_mask_length
                  vcat_mask_length
          ), 2).toString(16);
          if (
              vmCPUClosMasks.hasOwnProperty(cpu_policies_line.VM) &&
              vmCPUClosMasks[cpu_policies_line.VM].hasOwnProperty(cpu_policies_line.VCPU) &&
              vmCPUClosMasks[cpu_policies_line.VM][cpu_policies_line.VCPU].hasOwnProperty(line_type)
          ) {
            let scenario_clos_mask = vmCPUClosMasks[cpu_policies_line.VM][cpu_policies_line.VCPU][line_type];
            if (vcat_mask_length === null || count(Number.parseInt(scenario_clos_mask).toString(2), '1') === vcat_mask_length) {
              clos_mask = scenario_clos_mask
            }
          }
          cpu_policies_line['CLOS_MASK'] = clos_mask;
          cat_region_info.data.POLICY.push(cpu_policies_line)
        }

        function addPolicy(cpu_policies) {
          // noinspection JSUnusedLocalSymbols
          let cpu_policies_example = [
            {"VM": "POST_VM_1", "VCPU": 0},
            {"VM": "POST_VM_2", "VCPU": 2}
          ]
          for (let j = 0; j < cpu_policies.length; j++) {
            let cpu_policies_line = cpu_policies[j];
            if (CDP_ENABLED) {
              addCATPolicy(cpu_policies_line, "Code")
              addCATPolicy(_.cloneDeep(cpu_policies_line), "Data")
            } else {
              addCATPolicy(cpu_policies_line, "Unified")
            }
          }
          return CDP_ENABLED ? 2 * cpu_policies.length : cpu_policies.length
        }

        // add rt vm policy
        cat_region_info.real_time_count = 0
        for (let i = 0; i < cat_region_info.processors.length; i++) {
          let pcpu_id = cat_region_info.processors[i];
          let cpu_policies = _.cloneDeep(pcpu_vms[pcpu_id] ? pcpu_vms[pcpu_id]['y'] || [] : []);
          cat_region_info.real_time_count += addPolicy(cpu_policies)
        }
        // add std vm policy
        cat_region_info.cat_count = _.cloneDeep(cat_region_info.real_time_count)
        for (let i = 0; i < cat_region_info.processors.length; i++) {
          let pcpu_id = cat_region_info.processors[i];
          let cpu_policies = _.cloneDeep(pcpu_vms[pcpu_id] ? pcpu_vms[pcpu_id]['n'] || [] : []);
          cat_region_info.cat_count += addPolicy(cpu_policies)
        }

        // add cat vm policy
        if (cat_region_info.processors.indexOf(0) !== -1) {
          for (let i = 0; i < vCats.length; i++) {
            addCATPolicy(_.cloneDeep(vCats[i]), 'Unified', vCats[i].CLOS_MASK)
          }
        }

      })
      this.CAT_INFO = _.cloneDeep(board_cat_info);
      // remove cache entries that has no policy
      for (let i = 0; i < this.CAT_INFO.length; i++) {
        if (this.CAT_INFO[i].data.POLICY.length == 0) {
          this.CAT_INFO.splice(i--, 1)
        }
      }
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
  padding: 7px 0 12px;
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