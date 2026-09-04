/// <reference types="vite/client" />

// Ambient module declarations for Vite's `?raw` query.
// Without this, `import x from '...js?raw'` is typed as `unknown`.

declare module '*?raw' {
  const src: string;
  export default src;
}