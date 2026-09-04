import { Router } from 'express';
import { readState, writeState } from '../storage/jsonStore';

export const stateRouter = Router();

stateRouter.get('/:key', async (req, res) => {
  const value = await readState(req.params.key, null);
  res.json(value);
});

stateRouter.put('/:key', async (req, res) => {
  await writeState(req.params.key, req.body);
  res.json({ ok: true });
});