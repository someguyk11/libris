import { fileURLToPath, URL } from 'node:url'
import { defineConfig, loadEnv } from 'vite'
import vue from '@vitejs/plugin-vue'

// https://vite.dev/config/
export default defineConfig(({ mode }) => ({
  plugins: [vue()],
  server: {
    // Backend virtualenvs and firmware build outputs are not frontend inputs.
    watch: { ignored: ['**/backend/.venv/**', '**/hardware/**/.pio/**', '**/backend/__pycache__/**'] },
    proxy: {
      '/api': {
        target: loadEnv(mode, process.cwd(), '').LIBRIS_BACKEND_URL || 'http://127.0.0.1:8000',
        changeOrigin: true,
        rewrite: path => path.replace(/^\/api/, ''),
      },
    },
  },
  resolve: {
    alias: {
      '@': fileURLToPath(new URL('./src', import.meta.url)),
    },
  },
}))
