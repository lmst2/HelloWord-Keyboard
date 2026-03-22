/** @type {import('tailwindcss').Config} */
export default {
  content: ["./index.html", "./src/**/*.{js,ts,jsx,tsx}"],
  theme: {
    extend: {
      colors: {
        hw: {
          bg: "#0f0f13",
          surface: "#1a1a24",
          border: "#2a2a3a",
          accent: "#6366f1",
          "accent-light": "#818cf8",
          text: "#e2e2f0",
          "text-dim": "#8888a0",
          success: "#22c55e",
          warning: "#f59e0b",
          error: "#ef4444",
        },
      },
    },
  },
  plugins: [],
};
