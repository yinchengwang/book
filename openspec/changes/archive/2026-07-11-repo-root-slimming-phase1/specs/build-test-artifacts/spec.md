## ADDED Requirements

### Requirement: Build artifacts use build directory
All configured build trees and compiler/linker outputs SHALL be placed under `build/<project-or-track>/` rather than in source directories.

#### Scenario: Engineering track is configured
- **WHEN** the engineering track is configured for local or CI builds
- **THEN** its build tree and generated build files are placed under `build/engineering/`

#### Scenario: Learning track is configured
- **WHEN** the learning track is configured for local or CI builds
- **THEN** its build tree and generated build files are placed under `build/learning/`

#### Scenario: Root dispatcher is configured
- **WHEN** the root dispatcher build is configured with one or both tracks enabled
- **THEN** its build tree and generated build files are placed under `build/root/` or another documented `build/<project-or-track>/` subdirectory

### Requirement: Test and runtime artifacts use test-results directory
CTest logs, coverage outputs, temporary test databases, runtime logs, generated test reports, and other test/runtime artifacts SHALL be placed under `test-results/<project-or-track>/` rather than in source directories or the repository root.

#### Scenario: Engineering tests generate runtime artifacts
- **WHEN** engineering tests generate logs, databases, coverage files, reports, or other runtime outputs
- **THEN** those artifacts are written under `test-results/engineering/`

#### Scenario: Learning tests generate runtime artifacts
- **WHEN** learning tests, scaffolds, or smoke checks generate logs, reports, or temporary outputs
- **THEN** those artifacts are written under `test-results/learning/`

#### Scenario: Root-level smoke checks generate artifacts
- **WHEN** root-level dispatch or cross-track smoke checks generate outputs
- **THEN** those artifacts are written under `test-results/root/` or another documented `test-results/<project-or-track>/` subdirectory

### Requirement: Test binaries do not pollute source directories
Test executable outputs SHALL NOT be written into source directories as part of the normal build/test workflow.

#### Scenario: CMake creates test executable targets
- **WHEN** CMake configures test executable output directories
- **THEN** runtime outputs are placed under the active build tree or another documented `build/<project-or-track>/` location, not under `test/`, `engineering/test/`, `learning/`, or the repository root source tree

### Requirement: Ignore rules cover generated artifacts
The repository SHALL ignore generated build and test/runtime artifact directories while keeping source fixtures and compatibility README files trackable.

#### Scenario: Generated artifacts are created
- **WHEN** commands create files under `build/` or `test-results/`
- **THEN** Git ignore rules prevent generated artifacts from appearing as untracked source changes

#### Scenario: Source fixtures are migrated
- **WHEN** reusable test fixtures are moved under a source-owned fixture directory
- **THEN** those fixture files remain trackable and are not hidden by broad artifact ignore rules

### Requirement: Phase 1 validation checks artifact locations
Each Phase 1 migration batch SHALL verify both build/test success and artifact placement.

#### Scenario: Batch verification runs
- **WHEN** a Phase 1 migration batch is completed
- **THEN** verification includes engineering build/CTest, learning build/CTest, root dispatcher build, old-path reference scan, key smoke checks, and confirmation that generated artifacts land in `build/` or `test-results/`
