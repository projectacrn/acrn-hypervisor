<template>
  <b>Memory Isolation for Performance</b>

  <div class="py-3">
    <b-form-checkbox v-model="SSRAM_ENABLED" :value="'y'" :uncheckedValue="'n'">Software SRAM (for real-time apps)</b-form-checkbox>
  </div>
  <div class="d-flex gap-2 flex-column">
    <text>Intel Resource Director Technology</text>
    <b-form-checkbox v-model="RDT_ENABLED" :value="'y'" :uncheckedValue="'n'">
      Cache Allocation Technology (requires CPU Afinity configuration in each desired VM)
    </b-form-checkbox>
    <div class="d-flex flex-column gap-2 ps-3 pb-3">
      <b-form-checkbox v-model="CDP_ENABLED" :value="'y'" :uncheckedValue="'n'">
        Code and Data Prioritization
      </b-form-checkbox>
      <b-form-checkbox v-model="VCAT_ENABLED" :value="'y'" :uncheckedValue="'n'">
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
      <b-button>
        Apply basic real-time defaults
      </b-button>
    </div>
    <div class="d-flex">
      <div class="leftTitle">
        <!--left title-->
        <div v-for="(POLICY,index) in CACHE_ALLOCATION.data.POLICY">
          <div v-if="index===0&&CACHE_ALLOCATION.real_time_count>0">Real-time</div>
          <div v-if="index===CACHE_ALLOCATION.real_time_count">Standard</div>
          {{ POLICY.VM }} vCPU {{ POLICY.VCPU }}{{ POLICY.TYPE === 'Unified' ? '' : "_" + POLICY.TYPE }}
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
            <div style="height: 33px;width: 100%;background: #bfbfbf"
                 v-if="index===CACHE_ALLOCATION.real_time_count && CACHE_ALLOCATION.real_time_count>0"></div>
            <HexBlockRangeSelector
                v-model="POLICY.CLOS_MASK"
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
</template>

<script>

import {vueUtils, fieldProps} from "@lljj/vue3-form-naive";
import _ from "lodash";
import HexBlockRangeSelector from "./CAT/HexBlockRangeSelector.vue";


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
    VCAT_ENABLED:{
      get() {
        return vueUtils.getPathVal(this.rootFormData, 'FEATURES.RDT.VCAT_ENABLED')
      },
      set(value) {
        vueUtils.setPathVal(this.rootFormData, 'FEATURES.RDT.VCAT_ENABLED', value)
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
      }
    },
    CDP_ENABLED: {
      get() {
        return vueUtils.getPathVal(this.rootFormData, 'FEATURES.RDT.CDP_ENABLED')
      },
      set(value) {
        vueUtils.setPathVal(this.rootFormData, 'FEATURES.RDT.CDP_ENABLED', value)
        this.updateCatInfo()
      }
    }
  },
  watch: {
    CAT_INFO: {
      handler(newValue, _) {
        let data = [];
        for (let i = 0; i < newValue.length; i++) {
          data.push(newValue[i].data)
        }
        this.defaultVal.CACHE_ALLOCATION = data
      },
      deep: true
    },
  },
  data() {
    return {
      defaultVal: vueUtils.getPathVal(this.rootFormData, this.curNodePath),
      CAT_INFO: {},
      cat_level_region_sum: {}
    }
  },
  methods: {
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
      // get CAT info from board xml
      let board_cat_info = window.getBoardData().CAT_INFO;
      let CDP_ENABLED = this.CDP_ENABLED === 'y'

      // noinspection JSUnusedLocalSymbols
      let board_cat_info_example = [
        {
          "id": "0x0", "level": 3, "type": "3", "cache_size": 31457280, "capacity_mask_length": 12,
          "processors": [0, 8, 1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15]
        }
      ]

      // get scenario   pcpu config
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
      window.getCurrentScenarioData().vm.map((vmConfig) => {
        if (
            !vmConfig.hasOwnProperty('cpu_affinity') ||
            !vmConfig.cpu_affinity.hasOwnProperty('pcpu') ||
            !_.isArray(vmConfig.cpu_affinity.pcpu)
        ) {
          return
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


      // noinspection JSUnusedLocalSymbols
      let defaultVal_data_example = {
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
      if (this.defaultVal.hasOwnProperty('CACHE_ALLOCATION') && _.isArray(this.defaultVal.CACHE_ALLOCATION)) {
        // noinspection JSUnresolvedVariable
        this.defaultVal.CACHE_ALLOCATION.map((cache_region) => {
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
        if (scenario_cat_data.hasOwnProperty(cat_region_info.level) && scenario_cat_data[cat_region_info.level].hasOwnProperty(cat_region_info.id)) {
          let current_region_scenario_cat_data = scenario_cat_data[cat_region_info.level][cat_region_info.id];
          // noinspection JSUnusedLocalSymbols
          let current_region_scenario_cat_data_example = {
            "CACHE_ID": "0x7", "CACHE_LEVEL": 2, "POLICY": [
              {"VM": "POST_RT_VM1", "VCPU": 0, "TYPE": "Unified", "CLOS_MASK": "0x0fff"}
            ]
          }
          for (let i = 0; i < current_region_scenario_cat_data.POLICY.length; i++) {
            let current_policies = current_region_scenario_cat_data.POLICY[i]
            if (!vmCPUClosMasks.hasOwnProperty(current_policies.VM)) {
              vmCPUClosMasks[current_policies.VM] = {}
            }
            if (!vmCPUClosMasks[current_policies.VM].hasOwnProperty(current_policies.VCPU)) {
              vmCPUClosMasks[current_policies.VM][current_policies.VCPU] = {}
            }
            if (["Unified", "Code", "Data"].indexOf(current_policies.TYPE) > 0) {
              console.log(current_policies.TYPE, current_policies.CLOS_MASK)
              vmCPUClosMasks[current_policies.VM][current_policies.VCPU][current_policies.TYPE] = current_policies.CLOS_MASK
            }
          }
        }

        cat_region_info['data'] = {
          "CACHE_ID": cat_region_info.id,
          "CACHE_LEVEL": cat_region_info.level,
          "POLICY": []
        }

        function addCATPolicy(cpu_policies_line, line_type) {
          cpu_policies_line['TYPE'] = line_type;
          let clos_mask = "";
          if (
              vmCPUClosMasks.hasOwnProperty(cpu_policies_line.VM) &&
              vmCPUClosMasks[cpu_policies_line.VM].hasOwnProperty(cpu_policies_line.VCPU) &&
              vmCPUClosMasks[cpu_policies_line.VM][cpu_policies_line.VCPU].hasOwnProperty(line_type)
          ) {
            clos_mask = vmCPUClosMasks[cpu_policies_line.VM][cpu_policies_line.VCPU][line_type];
          } else {
            clos_mask = "0x" + parseInt('1'.repeat(cat_region_info.capacity_mask_length), 2).toString(16);
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

        cat_region_info.real_time_count = 0
        for (let i = 0; i < cat_region_info.processors.length; i++) {
          let cpu_policies = _.cloneDeep(pcpu_vms[cat_region_info.processors[i]] ? pcpu_vms[cat_region_info.processors[i]]['y'] || [] : []);
          cat_region_info.real_time_count += addPolicy(cpu_policies)
        }
        for (let i = 0; i < cat_region_info.processors.length; i++) {
          let cpu_policies = _.cloneDeep(pcpu_vms[cat_region_info.processors[i]] ? pcpu_vms[cat_region_info.processors[i]]['n'] || [] : []);
          addPolicy(cpu_policies)
        }
      })

      this.CAT_INFO = board_cat_info;
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

.form-check {
  display: flex;
  align-items: center;
  gap: 8px;
}
</style>