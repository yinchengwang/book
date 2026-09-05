# Shared Component Dependencies

Cross-project shared components (`engineering/apps/games/web/shared/web/src/`)
have implicit host-project requirements. Any project that imports from
`@shared/...` MUST install the deps that the shared module assumes are
available in the consumer's `node_modules`.

## Current Inventory

| Component | Required Host Deps |
|-----------|-------------------|
| `ui/Card.tsx` | none |
| `components/Markdown.tsx` | `react-markdown@^9`, `remark-gfm@^4`, `@tailwindcss/typography@^0.5` |

## How It Works Today

- `games-web` is the canonical source of truth for shared modules. Its
  `package.json` owns `react-markdown`, `remark-gfm`, and
  `@tailwindcss/typography`.
- Consumers (e.g. `reading-radar`) install the **same** deps directly in
  their own `package.json`. The shared module never declares them.
- This works because Vite resolves `@shared/components/Markdown` to the
  source file under `games-web/shared/web/src/`, but Vite does **not**
  transitively pull `games-web`'s `package.json` deps. Each consumer
  project must list them.

## When Adding a New Consumer

1. Add the deps listed above to the new project's `package.json`.
2. Add `@tailwindcss/typography` to the project's Tailwind config if it
   uses `Markdown` (plugin import + `plugins: [typography]`).
3. If a new shared component requires additional deps, update this
   document and add them to **every** consumer.

## Future Work

Create a dedicated `shared/package.json` with its own deps and build
pipeline so consumers don't need to redeclare them. This will require
moving shared modules out of `games-web/` into their own workspace.

Epic: TBD