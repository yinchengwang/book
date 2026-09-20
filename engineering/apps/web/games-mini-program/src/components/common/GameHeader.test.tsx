/** @vitest-environment jsdom */
import { describe, it, expect, vi } from "vitest";
import { render } from "@testing-library/react";
import { GameHeader } from "./GameHeader";

describe("GameHeader", () => {
  it("渲染标题和返回按钮", () => {
    const { getByText } = render(
      <GameHeader title="贪吃蛇" onBack={vi.fn()} />,
    );
    expect(getByText("贪吃蛇")).toBeTruthy();
  });
});
