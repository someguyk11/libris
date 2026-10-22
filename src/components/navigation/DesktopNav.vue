<template>
  <nav class="desktop-nav">
    <div class="nav-brand">
      <slot name="brand">
        <span class="logo">
          Libris
        </span>
      </slot>
    </div>

    <div class="nav-links">
      <button
        v-for="item in items"
        :key="item.name"
        class="nav-link"
        :class="{ active: active === item.name }"
        @click="$emit('change', item.name)"
      >
        {{ item.label }}
      </button>
    </div>

    <div class="nav-actions">
      <slot name="actions">
        <button class="profile-button">
          Profile
        </button>
      </slot>
    </div>
  </nav>
</template>

<script setup>
defineProps({
  active: {
    type: String,
    default: "explore"
  },

  items: {
    type: Array,
    default: () => [
      {
        name: "explore",
        label: "Explore"
      },
      {
        name: "library",
        label: "Library"
      },
      {
        name: "rooms",
        label: "Reading Rooms"
      },
      {
        name: "collections",
        label: "Collections"
      }
    ]
  }
})

defineEmits([
  "change"
])
</script>

<style scoped>
.desktop-nav {
  width: 100%;

  height: 72px;

  display: flex;
  align-items: center;
  justify-content: space-between;

  padding: 0 2rem;

  background: rgba(15,15,15,0.75);
  backdrop-filter: blur(18px);

  border-bottom: 1px solid rgba(255,255,255,0.08);
}

.logo {
  font-size: 1.4rem;
  font-weight: 700;
  letter-spacing: -0.04em;
}

.nav-links {
  display: flex;
  align-items: center;
  gap: 2rem;
}

.nav-link {
  position: relative;

  border: none;
  background: transparent;

  cursor: pointer;

  font-size: 0.95rem;

  opacity: 0.6;

  transition:
    opacity 0.2s ease;
}

.nav-link:hover {
  opacity: 1;
}

.nav-link.active {
  opacity: 1;
}

.nav-link.active::after {
  content: "";

  position: absolute;

  bottom: -8px;
  left: 50%;

  width: 18px;
  height: 2px;

  transform: translateX(-50%);

  border-radius: 99px;
}

.nav-actions {
  display: flex;
  align-items: center;
}

.profile-button {
  border: none;
  background: transparent;

  cursor: pointer;

  opacity: 0.75;
}

.profile-button:hover {
  opacity: 1;
}
</style>