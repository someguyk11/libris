<template>
<div class='input-wrap'><input :placeholder='placeholder' v-model='model'/></div>
</template>
<script setup>
import {computed} from 'vue'
const p=defineProps({modelValue:String,placeholder:String})
const e=defineEmits(['update:modelValue'])
const model=computed({get:()=>p.modelValue,set:v=>e('update:modelValue',v)})
</script>
<style scoped>
.input-wrap{background:#121222;border:1px solid rgba(255,255,255,.08);border-radius:16px;padding:.8rem 1rem}
input{width:100%;background:transparent;border:none;outline:none;color:#F8F8FA}
</style>