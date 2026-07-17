## ADDED Requirements

### Requirement: SPA navigation triggers page rendering

When a page is loaded via SPA navigation (fetch + DOM swap), any rendering logic that normally waits for `DOMContentLoaded` SHALL still execute and produce the same visual output as a direct page load.

#### Scenario: learn.html renders after SPA nav
- **WHEN** user clicks the "学习" tab in the top navigation bar
- **THEN** `learn.html` content renders fully (knowledge tree, search, category filter) without requiring a full page reload

#### Scenario: grok.html renders after SPA nav
- **WHEN** user clicks the "图解" tab in the top navigation bar
- **THEN** `grok.html` renders with the illustrated content without requiring a full page reload

#### Scenario: Direct URL access still works
- **WHEN** user opens `learn.html` or `grok.html` directly via URL bar or browser refresh
- **THEN** page renders identically to before this change

### Requirement: External scripts execute in correct order

All external JavaScript dependencies MUST be loaded and executed before any inline script on the target page, regardless of whether the page is loaded directly or via SPA navigation.

#### Scenario: tech.js and static.js load before init()
- **WHEN** `learn.html` is loaded via SPA navigation
- **THEN** `C_TECH_DATA`, `CPP_TECH_DATA`, `CATEGORIES` (etc.) are defined before `init()` executes

#### Scenario: learn-deep-bundle.js loads before render()
- **WHEN** `learn.html` or `grok.html` is loaded via SPA navigation
- **THEN** `window.LEARN_DEEP_CONTENT` is defined before the page's rendering function executes

### Requirement: DOMContentLoaded dispatch does not cause duplicate rendering

The fix SHALL NOT cause a page to render twice when the user navigates to it multiple times via SPA tabs.

#### Scenario: Repeated navigation does not duplicate content
- **WHEN** user navigates to "学习" tab, then "首页" tab, then "学习" tab again
- **THEN** each navigation produces a clean, single render of the page (no duplicate DOM elements or repeated API calls)
