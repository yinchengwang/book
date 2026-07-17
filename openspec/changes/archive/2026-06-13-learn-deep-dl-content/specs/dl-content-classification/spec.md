## ADDED Requirements

### Requirement: DL relevance tiering criteria
The system SHALL define three tiers of DL relevance for classifying each knowledge point BEFORE content generation begins.

- **Strong**: Knowledge point has a direct, concrete application in deep learning (e.g., vector indexing, matrix operations, GPU programming, gradient descent, neural network architectures, attention mechanism, data loaders, loss functions). These topics appear frequently in DL papers, frameworks, or engineering practice.
- **Medium**: Knowledge point has an indirect but meaningful connection to DL (e.g., B+ tree used in DL metadata storage, memory management patterns reused in tensor allocators, concurrent programming in multi-GPU training, file systems in checkpoint storage). The connection requires explanation but is well-established.
- **Weak**: Knowledge point has only peripheral or metaphorical connection to DL (e.g., basic C syntax, Git usage, general coding standards). The article SHALL still include a brief DL bridge paragraph for completeness.

#### Scenario: Strong tier classification
- **WHEN** the system evaluates a knowledge point whose core concept is directly used in DL frameworks or algorithms (e.g., `db-hnsw`, `py-pytorch`, `ds-matrix-pow`, `c-mmap`)
- **THEN** it SHALL be classified as "strong" and SHALL receive the full article template (four sections, code examples, ~1000-1500 tokens)

#### Scenario: Medium tier classification
- **WHEN** the system evaluates a knowledge point whose concept appears in DL engineering but not as a core algorithm (e.g., `ds-btree`, `cpp-move-semantics`, `c-pthread`, `py-fileio`)
- **THEN** it SHALL be classified as "medium" and SHALL receive the concise article template (two sections, no code required, ~400-600 tokens)

#### Scenario: Weak tier classification
- **WHEN** the system evaluates a knowledge point with only peripheral DL relevance (e.g., `c-preprocessor`, `cpp-git`, `ds-sort-basic`, `py-venv`)
- **THEN** it SHALL be classified as "weak" and SHALL receive a single DL bridge paragraph (~100-200 tokens)

### Requirement: Classification output format
The classification result SHALL be a JSON object that maps each knowledge point ID to its tier and rationale.

#### Scenario: Complete classification output
- **WHEN** classification is complete
- **THEN** the system SHALL produce a JSON file at `openspec/changes/learn-deep-dl-content/classification-output.json`
- **AND** each entry SHALL contain: `itemId`, `tier` (one of "strong", "medium", "weak"), `rationale` (one-sentence justification)
- **AND** all 300 items from ITEMS_REGISTRY SHALL be present in the output

#### Scenario: Existing article detection
- **WHEN** the system classifies a knowledge point that already has a learn-deep article (present in `LEARN_DEEP_CONTENT` or `data/learn-deep/`)
- **THEN** the classification entry SHALL include an additional field `hasExistingArticle: true`
- **AND** the generation phase SHALL use the "supplement" strategy (append DL chapter) instead of creating a new article

### Requirement: Classification reviewability
The classification rules SHALL be transparent and auditable.

#### Scenario: Tier definition traceability
- **WHEN** a reviewer inspects why a specific item was classified as strong/medium/weak
- **THEN** the `rationale` field in the classification JSON SHALL provide a clear, specific justification
- **AND** the tier definition criteria (above) SHALL be applied consistently across all stacks

#### Scenario: Classification override
- **WHEN** a manually identified misclassification is found during review
- **THEN** the classification JSON SHALL be editable before the generation phase begins
- **AND** the project SHALL document that manual overrides are expected for ~5-10% of items
