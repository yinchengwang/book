import express from 'express';
import { stateRouter } from './api/state.js';

const app = express();
app.use(express.json({ limit: '10mb' }));
app.use('/api/state', stateRouter);

const PORT = process.env.PORT ?? 8080;
app.listen(PORT, () => console.log(`Server on http://localhost:${PORT}`));
