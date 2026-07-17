## ADDED Requirements

### Requirement: Root directory whitelist
The repository SHALL keep root-level real content limited to the dual-track entries, shared governance/build/documentation infrastructure, third-party/reference source mirrors, and required configuration files.

#### Scenario: Root contains only approved shared entries after Phase 1
- **WHEN** Phase 1 migration is complete
- **THEN** root-level real content is limited to `engineering/`, `learning/`, `docs/`, `openspec/`, `archive/`, `cmake/`, `scripts/`, `third_part/`, `reference/`, required dot/config files, and temporary compatibility entrypoints documented for Phase 2 removal

#### Scenario: Business or learning content is added
- **WHEN** new business, engineering, learning, demo, RAG, SDK, or tool content is added
- **THEN** it MUST be placed under the owning track or approved shared infrastructure, not as a new unowned root-level directory

### Requirement: Engineering assets are canonical under engineering
Engineering-owned historical root assets SHALL be migrated to `engineering/` as their canonical location.

#### Scenario: Engineering historical directories are migrated
- **WHEN** Phase 1 handles root-level `apps/`, `rag/`, `sdk/`, Docker deployment files, `include/`, or `test_data/`
- **THEN** real engineering content is moved or merged into `engineering/` and root-level entries are reduced to compatibility README files or necessary wrappers

#### Scenario: Top-level include is audited
- **WHEN** Phase 1 processes root-level `include/`
- **THEN** each file is classified and merged into `engineering/include/` or `third_part/` before root-level real header content is removed

#### Scenario: Engineering test fixtures are migrated
- **WHEN** Phase 1 processes root-level `test_data/`
- **THEN** reusable fixtures are migrated to an engineering-owned fixture location and generated runtime data is redirected to `test-results/engineering/`

### Requirement: Learning assets are canonical under learning
Learning-owned historical root assets SHALL be migrated to `learning/` as their canonical location.

#### Scenario: Learning historical directories are migrated
- **WHEN** Phase 1 handles root-level `Interview/`, `arkts/`, `blog/`, `exam/`, `notes/`, `demo/`, `demo_code/`, `demo_projects/`, article drafts, or learning misc files
- **THEN** real learning content is moved or merged into the corresponding `learning/` directory and root-level entries are reduced to compatibility README files

#### Scenario: Duplicate learning content differs
- **WHEN** a root-level learning asset differs from the existing `learning/` canonical asset
- **THEN** Phase 1 MUST merge the difference into `learning/` before replacing the root-level real content with a compatibility entrypoint

### Requirement: Tools are split by ownership
Root-level `tools/` real content SHALL be split by usage ownership instead of remaining as a root-level tool tree.

#### Scenario: Shared tool is classified
- **WHEN** a tool serves repository-wide governance, documentation, migration, or maintenance
- **THEN** it is migrated to `scripts/` or an approved documentation script location

#### Scenario: Track-specific tool is classified
- **WHEN** a tool serves only engineering or only learning workflows
- **THEN** it is migrated to `engineering/tools/` or `learning/tools/` respectively

#### Scenario: Phase 1 completes tools migration
- **WHEN** Phase 1 completes `tools/` processing
- **THEN** root-level `tools/` contains no real tool implementation and only documents the new canonical locations until Phase 2 removes the compatibility entrypoint

### Requirement: Compatibility entrypoints are temporary and lightweight
Phase 1 SHALL retain old root paths only as temporary compatibility entrypoints and SHALL NOT keep real code duplicates there.

#### Scenario: Old root directory remains after migration
- **WHEN** an old root-level directory remains after its content has been migrated
- **THEN** it contains only a README pointer and, where required, a thin wrapper or alias that forwards users to the canonical path

#### Scenario: Phase 2 deletion is prepared
- **WHEN** Phase 1 creates a compatibility entrypoint
- **THEN** the entrypoint documents that it is deprecated and scheduled for Phase 2 removal

### Requirement: Governance documents reflect canonical paths
The repository SHALL update governance and architecture documents whenever canonical root/track organization changes.

#### Scenario: Phase 1 documentation is updated
- **WHEN** Phase 1 changes directory ownership or artifact locations
- **THEN** `README.md`, `CLAUDE.md`, `AGENTS.md`, `docs/architecture/dual-track.md`, track README files, OpenSpec artifacts, and compatibility README files describe the same canonical paths and migration rules
