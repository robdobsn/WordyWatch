# Word Clock Grace

A customizable word clock web application inspired by [QlockTwo](https://www.qlocktwo.com/en-us) that displays time using military time format in words.

## Features

- **Military Time Format**: Displays time like "eleven hundred hours", "eleven oh five hours", etc.
- **Customizable Layouts**: JSON-based layout system supporting both horizontal and vertical word orientations
- **Font Customization**: Adjust font family, weight, size, letter spacing, and line height
- **Time Controls**: Manual time setting with spinners/dropdowns or automatic current time display
- **5-minute Intervals**: Time is displayed to the nearest 5 minutes
- **Responsive Design**: Built with Tailwind CSS for modern, responsive UI

## Military Time Examples

- `00:00` → "zero hundred hours"
- `00:05` → "zero oh five hours"  
- `11:00` → "eleven hundred hours"
- `11:05` → "eleven oh five hours"
- `23:55` → "twenty three fifty five hours"

## Available Layouts

1. **Military Standard**: Traditional horizontal layout with all hour and minute words
2. **Compact Vertical**: Mixed horizontal and vertical orientations in a compact grid

## Technology Stack

- **React 18** with TypeScript
- **Vite** for fast development and building
- **Tailwind CSS** for styling
- **JSON-based** layout configuration system

## Getting Started

### Prerequisites

- Node.js (version 16 or higher)
- npm or yarn

### Installation

1. Clone the repository:
   ```bash
   git clone <repository-url>
   cd WordClockGrace
   ```

2. Install dependencies:
   ```bash
   npm install
   ```

3. Start the development server:
   ```bash
   npm run dev
   ```

4. Open your browser and navigate to `http://localhost:5173`

### Building for Production

```bash
npm run build
```

The built files will be in the `dist` directory.

## Creating Custom Layouts

Layouts are defined in JSON files in the `public/layouts/` directory. Each layout file should follow this structure:

```json
{
  "name": "Layout Name",
  "description": "Layout description",
  "gridWidth": 18,
  "gridHeight": 16,
  "words": [
    {
      "word": "ZERO",
      "startRow": 0,
      "startCol": 0,
      "direction": "horizontal",
      "category": "hour"
    }
  ]
}
```

### Word Properties

- `word`: The word to display (uppercase)
- `startRow`: Starting row position (0-based)
- `startCol`: Starting column position (0-based)  
- `direction`: Either "horizontal" or "vertical"
- `category`: Word category ("hour", "minute", "military", "connector")

### Required Words

Your layout should include these words for proper military time display:

**Hours**: ZERO, ONE, TWO, THREE, FOUR, FIVE, SIX, SEVEN, EIGHT, NINE, TEN, ELEVEN, TWELVE, THIRTEEN, FOURTEEN, FIFTEEN, SIXTEEN, SEVENTEEN, EIGHTEEN, NINETEEN, TWENTY, TWENTYONE, TWENTYTWO, TWENTYTHREE

**Minutes**: OH, FIVE, TEN, FIFTEEN, TWENTY, THIRTY, FORTY, FIFTY

**Military**: HUNDRED, HOURS

## Customization Options

### Font Settings
- **Font Family**: Choose from various typefaces (monospace, sans-serif, serif)
- **Font Weight**: Adjust from thin (100) to black (900)
- **Font Size**: Scale from 8px to 72px
- **Letter Spacing**: Fine-tune character spacing
- **Line Height**: Adjust vertical spacing between rows

### Time Settings
- **Manual Time**: Use spinners and dropdowns to set specific times
- **Current Time**: Toggle to display real-time clock
- **5-minute Intervals**: All times round to nearest 5-minute mark

## Project Structure

```
src/
├── components/          # React components
│   ├── ClockDisplay.tsx    # Main clock display with letter grid
│   ├── FontControls.tsx    # Font customization controls
│   └── TimeControls.tsx    # Time input controls
├── hooks/              # Custom React hooks
│   ├── useCurrentTime.ts   # Real-time clock hook
│   └── useLayout.ts        # Layout loading and management
├── types/              # TypeScript type definitions
│   └── layout.ts           # Layout and settings interfaces
├── utils/              # Utility functions
│   └── militaryTime.ts     # Military time conversion logic
├── App.tsx             # Main application component
├── main.tsx           # Application entry point
└── index.css          # Global styles and Tailwind imports

public/
└── layouts/           # JSON layout definitions
    ├── military-standard.json
    └── compact-vertical.json
```

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

This project is open source and available under the [MIT License](LICENSE).

## Acknowledgments

- Inspired by [QlockTwo](https://www.qlocktwo.com/en-us) word clocks
- Built with modern web technologies for experimentation and learning

