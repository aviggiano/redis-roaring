<template>
  <div class="contributors">
    <div class="contributor-grid">
      <div
        v-for="contributor in contributors"
        :key="contributor.login"
        class="contributor-card"
      >
        <img
          :src="contributor.avatar_url"
          :alt="contributor.login"
          class="contributor-avatar"
        />
        <div class="contributor-info">
          <a
            :href="contributor.html_url"
            target="_blank"
            class="contributor-name"
          >
            {{ contributor.login }}
          </a>
          <span class="contributor-commits"
            >{{ contributor.contributions }} commits</span
          >
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { onMounted, ref } from "vue";

const contributors = ref([]);
const GITHUB_REPO =
  import.meta.env.VITE_GITHUB_REPO || "aviggiano/redis-roaring";

onMounted(async () => {
  try {
    const response = await fetch(
      `https://api.github.com/repos/${GITHUB_REPO}/contributors`
    );
    contributors.value = await response.json();
  } catch (error) {
    console.error("Failed to fetch contributors:", error);
  }
});
</script>

<style scoped>
.contributors {
  margin: 2rem 0;
}

.contributor-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(250px, 1fr));
  gap: 1rem;
  margin-top: 1rem;
}

.contributor-card {
  display: flex;
  align-items: center;
  padding: 1rem;
  border: 1px solid var(--vp-c-border);
  border-radius: 8px;
  background: var(--vp-c-bg-soft);
  transition: all 0.2s;
}

.contributor-card:hover {
  border-color: var(--vp-c-brand);
  transform: translateY(-2px);
}

.contributor-avatar {
  width: 48px;
  height: 48px;
  border-radius: 50%;
  margin-right: 1rem;
}

.contributor-info {
  display: flex;
  flex-direction: column;
}

.contributor-name {
  font-weight: 600;
  color: var(--vp-c-text-1);
  text-decoration: none;
}

.contributor-name:hover {
  color: var(--vp-c-brand);
}

.contributor-commits {
  font-size: 0.875rem;
  color: var(--vp-c-text-2);
}
</style>
