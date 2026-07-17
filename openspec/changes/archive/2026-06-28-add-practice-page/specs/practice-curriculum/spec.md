## ADDED Requirements

### Requirement: Practice curriculum data structure

The system SHALL provide a `PRACTICE_CATEGORIES` global array containing all practice problems organized by knowledge category. Each category SHALL include an `items` array with individual problem entries. The system SHALL include both LeetCode and 牛客 problems, mixed within the same knowledge category. The data SHALL only include Easy and Medium difficulty problems.

#### Scenario: Data file loads correctly
- **WHEN** `data/app/practice-data.js` is loaded in the browser
- **THEN** a global `PRACTICE_CATEGORIES` array SHALL be available with at least 20 categories

#### Scenario: Category structure is valid
- **WHEN** iterating `PRACTICE_CATEGORIES`
- **THEN** each category SHALL have `id`, `title`, `phase`, `order`, `icon`, `desc`, and `items` fields

#### Scenario: Problem structure is valid
- **WHEN** iterating items within a category
- **THEN** each item SHALL have `title`, `platform`, `problemId`, `difficulty`, and `tags` fields

#### Scenario: Only Easy and Medium difficulty
- **WHEN** checking all problem difficulties across all categories
- **THEN** no item SHALL have `difficulty` set to `"hard"`

#### Scenario: Easy problems precede Medium problems
- **WHEN** examining items within a category
- **THEN** all Easy items SHALL appear before any Medium items

### Requirement: Phase-based category grouping

Categories SHALL be organized into 6 learning phases (1-6), each representing a progressive level of difficulty and conceptual depth.

#### Scenario: Phase value is valid
- **WHEN** checking any category's `phase` field
- **THEN** it SHALL be an integer between 1 and 6

#### Scenario: Categories ordered by phase and order
- **WHEN** rendering the sidebar navigation
- **THEN** categories SHALL be grouped by phase, and within each phase sorted by `order` ascending

### Requirement: Page displays practice problem lists

The system SHALL provide a `practice.html` page that displays practice problems organized by knowledge category, with a phase-collapsible sidebar navigation and problem list with completion checkboxes.

#### Scenario: Page loads with sidebar navigation
- **WHEN** `practice.html` is loaded
- **THEN** left sidebar SHALL display phase-collapsible category list, right panel SHALL show problems from the first category

#### Scenario: Click category to show problems
- **WHEN** user clicks a category name in the sidebar
- **THEN** the right panel SHALL display that category's problems with Easy problems first, followed by Medium problems

#### Scenario: Phase collapsible behavior
- **WHEN** user clicks a phase header in the sidebar
- **THEN** the categories within that phase SHALL expand or collapse

#### Scenario: Problem item shows metadata
- **WHEN** a problem is displayed in the list
- **THEN** it SHALL show: title, difficulty badge (Easy/Medium), platform indicator (LeetCode/牛客), problem number, and tags

### Requirement: Completion tracking with checkboxes

Each problem SHALL have a checkbox that the user can toggle to mark it as completed. The completion state SHALL persist across page reloads using localStorage.

#### Scenario: Toggle checkbox
- **WHEN** user clicks a checkbox next to a problem
- **THEN** the checkbox state SHALL toggle and the new state SHALL be saved to localStorage

#### Scenario: State persists on reload
- **WHEN** user reloads `practice.html`
- **THEN** previously toggled checkboxes SHALL retain their state

#### Scenario: Progress display updates
- **WHEN** a checkbox is toggled
- **THEN** the progress indicator (completed/total) for the current category SHALL update accordingly

#### Scenario: Global progress tracking
- **WHEN** any checkbox state changes
- **THEN** a global progress summary (total completed across all categories) SHALL update

### Requirement: Page registered in navigation

The practice page SHALL be registered in the project's unified navigation component so users can discover and access it from any other page.

#### Scenario: Navigation entry exists
- **WHEN** any page with `initGlobalNav()` loads
- **THEN** the navigation bar SHALL include a "刷题计划" entry linking to `practice.html`

#### Scenario: Active state on practice page
- **WHEN** `practice.html` loads with `initGlobalNav("practice")`
- **THEN** the "刷题计划" navigation item SHALL display the active state
