<template>
  <Icon size="16px" class="labelInfoIcon">
    <InfoCircle/>
  </Icon>
</template>

<script>
import {Icon} from "@vicons/utils";
import {InfoCircle} from "@vicons/fa"

export default {
  name: "IconInfo",
  components: {Icon,InfoCircle}
}
</script>

<style>
.genFormLabel{
  display: flex;
  align-items: center;
}
.labelInfoIcon{
  margin: 0 6px;
}
</style>