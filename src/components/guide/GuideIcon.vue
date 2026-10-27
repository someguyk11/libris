<template>
  <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false">
    <template v-if="name === 'atlas'"><rect x="4" y="6" width="16" height="13" rx="4"/><path d="M9 3h6M12 3v3M1 10v5m22-5v5M9 22h6"/><circle cx="8.5" cy="12" r=".9" fill="currentColor"/><circle cx="15.5" cy="12" r=".9" fill="currentColor"/><path d="M9 16h6"/></template>
    <template v-else-if="name === 'search'"><circle cx="10.5" cy="10.5" r="6.5"/><path d="m16 16 5 5"/></template>
    <template v-else-if="name === 'back'"><path d="m11 5-7 7 7 7M4 12h16"/></template>
    <template v-else-if="name === 'arrow'"><path d="m13 5 7 7-7 7M20 12H4"/></template>
    <template v-else-if="name === 'close'"><path d="m6 6 12 12M6 18 18 6"/></template>
    <template v-else-if="name === 'grid'"><rect x="3" y="3" width="7" height="7" rx="1.5"/><rect x="14" y="3" width="7" height="7" rx="1.5"/><rect x="3" y="14" width="7" height="7" rx="1.5"/><rect x="14" y="14" width="7" height="7" rx="1.5"/></template>
    <template v-else-if="name === 'list'"><path d="M10 5h11M10 12h11M10 19h11M3 5h2M3 12h2M3 19h2"/></template>
    <template v-else-if="name === 'pin'"><path d="M20 10c0 6-8 12-8 12S4 16 4 10a8 8 0 1 1 16 0Z"/><circle cx="12" cy="10" r="2.5"/></template>
    <template v-else-if="name === 'battery'"><rect x="2" y="6" width="18" height="12" rx="2"/><path d="M23 10v4M6 10v4m4-4v4m4-4v4"/></template>
    <template v-else-if="name === 'shield'"><path d="m12 2 9 4v6c0 5-9 10-9 10S3 17 3 12V6Z"/><path d="m8 11 3 3 5-6"/></template>
    <template v-else-if="name === 'check'"><path d="m5 12 5 5L20 6"/></template>
    <template v-else-if="name === 'book'"><path d="M12 5v16M12 5C9 2 4 3 2 4v15c3-1 7-1 10 2 3-3 7-3 10-2V4c-2-1-7-2-10 1Z"/></template>
    <template v-else><circle cx="12" cy="12" r="9"/><path d="M12 7v6m0 4h.01"/></template>
  </svg>
</template>
<script setup>defineProps({ name: { type: String, required: true } })</script>
