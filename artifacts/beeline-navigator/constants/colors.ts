/**
 * Semantic design tokens for the mobile app.
 *
 * These tokens mirror the naming conventions used in web artifacts (index.css)
 * so that multi-artifact projects share a cohesive visual identity.
 *
 * Replace the placeholder values below with values that match the project's
 * brand. If a sibling web artifact exists, read its index.css and convert the
 * HSL values to hex so both artifacts use the same palette.
 *
 * To add dark mode, add a `dark` key with the same token names.
 * The useColors() hook will automatically pick it up.
 */

const colors = {
  light: {
    // Legacy aliases (kept for backward compatibility)
    text: '#17251f',
    tint: '#d58b19',

    // Core surfaces
    background: '#f4f5ef',
    foreground: '#17251f',

    // Cards / elevated surfaces
    card: '#ffffff',
    cardForeground: '#17251f',

    // Primary action color (buttons, links, active states)
    primary: '#d58b19',
    primaryForeground: '#17251f',

    // Secondary / less-emphasis interactive surfaces
    secondary: '#e8ece4',
    secondaryForeground: '#283a31',

    // Muted / subdued elements (dividers, timestamps, placeholders)
    muted: '#e8ece4',
    mutedForeground: '#6d7b72',

    // Accent highlights (badges, selected items, focus rings)
    accent: '#e0eadf',
    accentForeground: '#244e36',

    // Destructive actions (delete, error states)
    destructive: '#b84b3d',
    destructiveForeground: '#ffffff',

    // Borders and input outlines
    border: '#dce2d9',
    input: '#dce2d9',
  },

  // Border radius (in px). Sync from the sibling web artifact's --radius
  // CSS variable. This value applies to cards, buttons, inputs, and modals.
  dark: {
    text: '#f1f4ed',
    tint: '#edb34d',
    background: '#111a16',
    foreground: '#f1f4ed',
    card: '#1a2720',
    cardForeground: '#f1f4ed',
    primary: '#edb34d',
    primaryForeground: '#19231c',
    secondary: '#25342b',
    secondaryForeground: '#e1e9df',
    muted: '#25342b',
    mutedForeground: '#9aa99d',
    accent: '#293d30',
    accentForeground: '#b9d9bf',
    destructive: '#e06c5d',
    destructiveForeground: '#ffffff',
    border: '#304037',
    input: '#304037',
  },
  radius: 18,
};

export default colors;
