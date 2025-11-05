/** @type {import('tailwindcss').Config} */
export default {
  content: [
    "./index.html",
    "./src/**/*.{js,ts,jsx,tsx}",
  ],
  theme: {
    extend: {
      fontFamily: {
        'mono': ['Monaco', 'Menlo', 'Ubuntu Mono', 'monospace'],
        'sans': ['Inter', 'system-ui', 'sans-serif'],
        'serif': ['Georgia', 'serif'],
      },
      letterSpacing: {
        'extra-wide': '0.2em',
        'ultra-wide': '0.4em',
      },
      lineHeight: {
        'extra-loose': '2.5',
        'ultra-loose': '3',
      }
    },
  },
  plugins: [],
}

