## ADDED Requirements

### Requirement: File output layout
The system SHALL write generated markdown files to the correct learn-deep directory structure, mirroring existing conventions.

#### Scenario: New article for knowledge point
- **WHEN** a knowledge point has no existing article (item not in `LEARN_DEEP_CONTENT` and no `.md` file in `data/learn-deep/`)
- **THEN** the system SHALL create a new markdown file at `data/learn-deep/<item.id>.md`
- **AND** the file SHALL be placed at the root of `data/learn-deep/` (not in subdirectories) to match the existing `fetch` convention: `fetch("data/learn-deep/" + item.id + ".md")`

#### Scenario: Supplement existing article
- **WHEN** a knowledge point already has an article at `data/learn-deep/<stack>/<item.id>.md` or is bundled in `LEARN_DEEP_CONTENT`
- **THEN** the system SHALL append the supplementary DL chapter to the existing file
- **AND** the original content SHALL NOT be modified — only appended at the end
- **AND** the append position SHALL be before any trailing blank lines

#### Scenario: Backup before modification
- **WHEN** the system modifies any existing `.md` file
- **THEN** it SHALL create a backup at `data/learn-deep/.backup/<item-id>.md.original` before writing
- **AND** the backup SHALL contain the original file content verbatim

### Requirement: Bundle update
Every new or modified article MUST be registered in the `LEARN_DEEP_CONTENT` bundle so that learn.html can render it even under `file://` protocol.

#### Scenario: Bundle regeneration
- **WHEN** all articles have been written/supplemented
- **THEN** the system SHALL rebuild `data/app/learn-deep-bundle.js` by concatenating all `.md` files from `data/learn-deep/` into a single `window.LEARN_DEEP_CONTENT = {...}` object
- **AND** the bundle SHALL use the item ID (`data/learn-deep/<id>.md` → `LEARN_DEEP_CONTENT["<id>"]`) as the key
- **AND** existing bundle entries SHALL be preserved (updated with the now-supplemented version)
- **AND** the bundle SHALL be minified to reduce file size (remove unnecessary whitespace)

#### Scenario: Bundle structure
- **WHEN** the bundle is loaded by `learn.html`
- **THEN** `window.LEARN_DEEP_CONTENT` SHALL be a flat object mapping string item IDs to markdown content strings
- **AND** `LEARN_DEEP_CONTENT[item.id]` SHALL return the full markdown content for that item

### Requirement: Verification
The system SHALL verify that all generated content is correctly integrated before marking the change as complete.

#### Scenario: Coverage check
- **WHEN** integration is complete
- **THEN** ALL 300 knowledge points from `ITEMS_REGISTRY` SHALL have a corresponding entry in `LEARN_DEEP_CONTENT`
- **AND** a script SHALL iterate over all 300 items and confirm `LEARN_DEEP_CONTENT[item.id]` exists and is non-empty

#### Scenario: File count check
- **WHEN** integration is complete
- **THEN** the number of `.md` files in `data/learn-deep/` SHALL be exactly 300
- **AND** the system SHALL log any missing files for manual attention
- **AND** the system SHALL log any files missing their corresponding `LEARN_DEEP_CONTENT` entry

#### Scenario: Render smoke test
- **WHEN** verification runs
- **THEN** the system SHALL load `data/app/learn-deep-bundle.js` in Node.js and confirm `Object.keys(LEARN_DEEP_CONTENT).length === 300`
- **AND** the system SHALL sample-check 5 random entries that their content starts with a markdown heading (`# `)

#### Scenario: Git status check
- **WHEN** the change is ready for commit
- **THEN** the system SHALL confirm that no pre-existing `.md` files were deleted (only added or appended)
- **AND** the system SHALL confirm that only files under `data/learn-deep/` and `data/app/learn-deep-bundle.js` were modified
