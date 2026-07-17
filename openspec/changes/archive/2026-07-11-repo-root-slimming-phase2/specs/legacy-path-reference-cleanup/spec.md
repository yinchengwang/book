## ADDED Requirements

### Requirement: Active legacy path references are eliminated
Phase 2 SHALL remove active references to Phase 1 legacy root paths from current code, scripts, build configuration, CI, tests, smoke commands, and user-facing documentation.

#### Scenario: Active reference is found
- **WHEN** a scan finds an active reference to a deleted legacy root path
- **THEN** the reference is updated to the canonical `engineering/`, `learning/`, `scripts/`, `third_part/`, `reference/`, or other approved path before deletion proceeds

#### Scenario: Current documentation mentions legacy path as usable
- **WHEN** current documentation describes a legacy root path as a usable entrypoint
- **THEN** Phase 2 updates the documentation to point only to the canonical path

### Requirement: Historical references are explicitly bounded
Phase 2 SHALL distinguish allowed historical references from active references so that old paths can remain in history without being treated as supported entrypoints.

#### Scenario: Historical reference is retained
- **WHEN** an old path appears in OpenSpec history, archive records, migration logs, commit notes, or compatibility migration explanation
- **THEN** the reference is allowed only if it is clearly historical and does not instruct users or scripts to use the old path

#### Scenario: Ambiguous reference is found
- **WHEN** a legacy path reference is ambiguous between active usage and historical context
- **THEN** Phase 2 MUST either rewrite it to the canonical path or mark it explicitly as historical before completion

### Requirement: Final validation includes reference scan report
Phase 2 SHALL produce or record a final scan result showing that old root paths are no longer active entrypoints.

#### Scenario: Final validation runs
- **WHEN** Phase 2 final validation is executed
- **THEN** it includes old-path reference scanning, double-track build/CTest, root dispatcher build, key smoke checks, artifact location checks, and final root whitelist verification
