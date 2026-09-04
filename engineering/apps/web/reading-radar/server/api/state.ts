import { Router } from 'express';
import { readState, writeState, validateKey } from '../storage/jsonStore';

export const stateRouter = Router();

stateRouter.get('/:key', async (req, res) => {
  try {
    const { key } = req.params;
    if (!validateKey(key)) return res.status(400).json({ error: 'Invalid key' });
    const value = await readState(key, null);
    res.json(value);
  } catch (err) {
    console.error('[api/state GET] read failed:', err);
    res.status(500).json({ error: 'read failed' });
  }
});

stateRouter.put('/:key', async (req, res) => {
  try {
    const { key } = req.params;
    if (!validateKey(key)) return res.status(400).json({ error: 'Invalid key' });
    await writeState(key, req.body);
    res.json({ ok: true });
  } catch (err) {
    console.error('[api/state PUT] write failed:', err);
    res.status(500).json({ error: 'write failed' });
  }
});
