## ADDED Requirements

### Requirement: Compatibility entrypoints are removed after Phase 1 stability
Phase 2 SHALL delete the temporary root-level compatibility entrypoints that Phase 1 created or preserved after canonical paths are stable.

#### Scenario: Phase 2 deletes old root entrypoint
- **WHEN** a Phase 1 compatibility entrypoint has no active references outside allowed historical records
- **THEN** Phase 2 removes the root-level README, wrapper, alias, or clear-error entrypoint for that legacy path

#### Scenario: Active reference blocks deletion
- **WHEN** a legacy root path still has active references in code, scripts, CI, CMake, tests, smoke commands, or current documentation
- **THEN** Phase 2 MUST fix those references before deleting the compatibility entrypoint

### Requirement: Root directory reaches final whitelist state
Phase 2 SHALL make the repository root match the final shared-area whitelist without temporary legacy compatibility entries.

#### Scenario: Final root whitelist is checked
- **WHEN** Phase 2 completes deletion of compatibility entrypoints
- **THEN** the repository root contains only `engineering/`, `learning/`, `docs/`, `openspec/`, `archive/`, `cmake/`, `scripts/`, `third_part/`, `reference/`, required dot/config files, and other explicitly documented shared entries

#### Scenario: Legacy entrypoint reappears
- **WHEN** a removed legacy root entrypoint is reintroduced as a real directory, wrapper, alias, or duplicate README without a new approved change
- **THEN** the change is rejected as a violation of the final root whitelist

### Requirement: No real or compatibility double canonical remains
Phase 2 SHALL prevent old root paths from continuing as a second canonical or semi-canonical access path.

#### Scenario: Legacy path is inspected after deletion
- **WHEN** an old root path such as `apps/`, `rag/`, `sdk/`, `tools/`, `Interview/`, `notes/`, or `demo*` is inspected after Phase 2
- **THEN** it no longer exists as a usable source, wrapper, README entrypoint, alias, symlink, or directory junction

### Requirement: Deletion is batched and reversible per batch
Phase 2 SHALL delete compatibility entrypoints in small batches and preserve the Phase 1 rollback discipline.

#### Scenario: Batch deletion fails validation
- **WHEN** a Phase 2 deletion batch fails validation
- **THEN** only the current batch is reverted or repaired, and previously validated batches remain intact
