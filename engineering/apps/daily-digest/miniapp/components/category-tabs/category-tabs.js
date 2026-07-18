Component({
  properties: {
    tabs: { type: Array, value: [] },
    activeCategory: { type: String, value: '' },
  },
  methods: {
    onChange(e) {
      this.triggerEvent('change', { key: e.currentTarget.dataset.key })
    },
  },
})
