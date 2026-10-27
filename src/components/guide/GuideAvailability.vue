<template><span class="guide-availability" :class="{ unavailable: !available }"><span aria-hidden="true"></span>{{ available ? 'Available' : 'Unavailable' }}</span></template>
<script setup>defineProps({ available: Boolean })</script>
