'use client';

import { create } from 'zustand';

interface UIState {
  sidebarCollapsed: boolean;
  configPanelOpen: boolean;
  theme: 'light' | 'dark';
  selectedPipeline: string;
  selectedDataset: string | null;

  toggleSidebar: () => void;
  toggleConfigPanel: () => void;
  setTheme: (theme: 'light' | 'dark') => void;
  setSelectedPipeline: (type: string) => void;
  setSelectedDataset: (id: string | null) => void;
}

export const useUIStore = create<UIState>((set) => ({
  sidebarCollapsed: false,
  configPanelOpen: true,
  theme: 'light',
  selectedPipeline: 'naive',
  selectedDataset: null,

  toggleSidebar: () =>
    set((state) => ({ sidebarCollapsed: !state.sidebarCollapsed })),
  toggleConfigPanel: () =>
    set((state) => ({ configPanelOpen: !state.configPanelOpen })),
  setTheme: (theme) => set({ theme }),
  setSelectedPipeline: (type) => set({ selectedPipeline: type }),
  setSelectedDataset: (id) => set({ selectedDataset: id }),
}));