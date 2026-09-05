# Tech Debt / 降级清单

## MVP-6 未完成项

- **sudoku_hint** (I-8)：`wasm-src/sudoku.c` 中的 `_sudoku_hint_js` 已实现，但 `binding.c` 未导出、`bindings.ts` 未绑定、UI 未触达。MVP-6 降级为"提示功能暂时不可用"，计划在后续 epic 补齐 binding 三处 + Sudoku UI hint 按钮。
