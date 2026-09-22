import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

// During `npm run dev`, the UI runs on its own port and proxies API + video
// stream calls to the C++ server on 127.0.0.1:8765. In production the C++
// server serves the built files from ui/dist, so these paths are same-origin.
export default defineConfig({
  plugins: [react()],
  server: {
    proxy: {
      "/api": "http://127.0.0.1:8765",
      "/stream.mjpg": "http://127.0.0.1:8765",
    },
  },
  build: { outDir: "dist" },
});
