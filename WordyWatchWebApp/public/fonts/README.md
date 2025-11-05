# Font Files for Vector DXF Export

To use vector font paths for precise laser cutting, place your font files in this directory.

## ⚠️ Copyright Notice

**All font files in this directory are subject to copyright by their respective owners and creators.** 

This project does not claim ownership of any fonts. Font files are provided for development and testing purposes only. Users are responsible for:
- Verifying they have appropriate licenses for any fonts they use
- Complying with font license terms (personal use, commercial use, redistribution, etc.)
- Obtaining proper licenses before commercial use or distribution

**When distributing this software, ensure you have the rights to include any fonts, or provide instructions for users to obtain fonts separately.**

## Supported Font Formats
- `.ttf` (TrueType)
- `.otf` (OpenType)
- `.woff2` (Web Open Font Format 2)

## Recommended Fonts for Laser Cutting
- **Arial** - Clean, simple outlines
- **Helvetica** - Similar to Arial, good for cutting
- **Courier New** - Monospace, consistent letter spacing
- **Open Sans** - Modern, readable
- **Roboto** - Google's clean font

## How to Add Fonts
1. Download your preferred font (ensure you have the license)
2. Place the `.ttf` or `.otf` file in this directory
3. Name it clearly (e.g., `arial.ttf`, `helvetica.ttf`)
4. The font will appear in the export dropdown

## Font File Names
The system looks for these specific filenames:
- `monaco.ttf` - Monaco font (matches Font Settings)
- `arial.ttf` - Arial font

## Adding Your Own Fonts
To add more fonts:
1. Place your `.ttf` or `.otf` file in this directory
2. Name it clearly (e.g., `myfont.ttf`)
3. The system will automatically detect and use it

## Fallback
If no local fonts are found, the system will try to load fonts from Google Fonts as a fallback.

## Vector vs Text Export
- **Vector Paths**: Creates closed polylines for each letter outline (recommended for laser cutting)
- **Text Entities**: Creates DXF TEXT entities (fallback if font loading fails)

For laser cutting, always use "Vector Paths" mode for the best results.
