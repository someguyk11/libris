<template>
  <section class="admin-panel admin-chart">
    <header class="admin-panel-heading"><div><h2>Circulation</h2><p class="admin-muted">Borrowing and returns, day by day</p></div><label><span class="admin-sr-only">Chart date range</span><select v-model.number="days"><option :value="7">Last 7 days</option><option :value="30">Last 30 days</option></select></label></header>
    <div class="admin-chart-controls"><button :aria-pressed="metric === 'loans'" @click="metric = 'loans'"><i></i>Borrowed <strong>{{ totals.loans }}</strong></button><button :aria-pressed="metric === 'returns'" @click="metric = 'returns'"><i class="is-return"></i>Returned <strong>{{ totals.returns }}</strong></button></div>
    <div class="admin-chart-readout" aria-live="polite">{{ selected.label }}<strong>{{ selected[metric] }} {{ metric === 'loans' ? 'borrowed' : 'returned' }}</strong></div>
    <svg class="admin-chart-svg" :style="{ '--series-color': metric === 'returns' ? '#6fcca9' : '#b982fa' }" viewBox="0 0 680 200" role="img" :aria-label="`${metric === 'loans' ? 'Books borrowed' : 'Books returned'} over the last ${days} days. Total ${totals[metric]}.`">
      <defs><linearGradient :id="gradientId" x1="0" y1="0" x2="0" y2="1"><stop stop-color="#a878f4" stop-opacity=".4" /><stop offset="1" stop-color="#a878f4" stop-opacity="0" /></linearGradient></defs>
      <g v-for="tick in 4" :key="tick" class="admin-chart-grid"><line x1="34" x2="662" :y1="y(max * (tick - 1) / 3)" :y2="y(max * (tick - 1) / 3)" /><text x="24" :y="y(max * (tick - 1) / 3) + 4" text-anchor="end">{{ Math.round(max * (tick - 1) / 3) }}</text></g>
      <g :key="`${days}-${metric}`" class="admin-chart-plot"><path :d="`${line} L 662 166 L 34 166 Z`" :fill="`url(#${gradientId})`" /><path :d="line" fill="none" stroke="#b982fa" stroke-width="2.5" />
        <g v-for="(point, index) in series" :key="point.date"><circle :cx="x(index)" :cy="y(point[metric])" :r="index === activeIndex ? 5 : 3" fill="#c394ff" /><circle :cx="x(index)" :cy="y(point[metric])" r="10" fill="transparent" tabindex="0" :aria-label="`${point.label}: ${point[metric]} ${metric}`" @pointerenter="activeIndex = index" @focus="activeIndex = index" @keydown.right.prevent="moveFocus(index + 1, $event)" @keydown.left.prevent="moveFocus(index - 1, $event)"><title>{{ point.label }}: {{ point[metric] }}</title></circle><text v-if="days === 7 || index % 5 === 0 || index === series.length - 1" :x="x(index)" y="193" :text-anchor="index === 0 ? 'start' : index === series.length - 1 ? 'end' : 'middle'" class="admin-chart-label">{{ point.label }}</text></g>
      </g>
    </svg>
    <details class="admin-chart-data"><summary>View chart data</summary><div class="admin-table-scroll"><table><caption class="admin-sr-only">Daily circulation totals</caption><thead><tr><th>Date</th><th>Borrowed</th><th>Returned</th></tr></thead><tbody><tr v-for="point in series" :key="point.date"><th>{{ new Date(point.date).toLocaleDateString() }}</th><td>{{ point.loans }}</td><td>{{ point.returns }}</td></tr></tbody></table></div></details>
  </section>
</template>
<script setup>
import { computed, ref, useId, watch } from 'vue'
import { circulationSeries } from '../adminData'
const props = defineProps({ events: { type: Array, default: () => [] } })
const days = ref(7), metric = ref('loans'), activeIndex = ref(6)
const gradientId = `chart-${useId()}`
const series = computed(() => circulationSeries(props.events, days.value))
const totals = computed(() => ({ loans: series.value.reduce((sum, point) => sum + point.loans, 0), returns: series.value.reduce((sum, point) => sum + point.returns, 0) }))
const selected = computed(() => series.value[activeIndex.value] || series.value.at(-1))
const max = computed(() => Math.ceil(Math.max(3, ...series.value.map(point => point[metric.value])) / 3) * 3)
const x = index => 34 + index * 628 / (series.value.length - 1)
const y = value => 166 - value / max.value * 140
const line = computed(() => series.value.map((point, index) => `${index ? 'L' : 'M'} ${x(index)} ${y(point[metric.value])}`).join(' '))
watch(days, value => { activeIndex.value = value - 1 })
function moveFocus(index, event) { activeIndex.value = Math.min(series.value.length - 1, Math.max(0, index)); event.target.closest('svg').querySelectorAll('circle[tabindex]')[activeIndex.value]?.focus() }
</script>
