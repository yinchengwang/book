// Package multimodal 提供 P1 多模态嵌入式 SDK 的 Go 绑定。
//
// 通过 cgo 调用 C ABI，提供向量 / 图 / 时序 / 文本四种模型的操作。
// C 库（libmmsdk.a 和 libsqlite3.a）需在 CGO_LDFLAGS 中指定路径。
package multimodal

import "testing"

// 编译期类型检查
var _ = []Model{ModelVector, ModelGraph, ModelTimeseries, ModelText}

func TestModelString(t *testing.T) {
	cases := map[Model]string{
		ModelVector:     "vector",
		ModelGraph:      "graph",
		ModelTimeseries: "timeseries",
		ModelText:       "text",
	}
	for m, expected := range cases {
		if got := m.String(); got != expected {
			t.Errorf("Model(%d).String() = %q, want %q", m, got, expected)
		}
	}
}