import { promises as fs } from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const DATA_DIR = path.resolve(__dirname, '../../user-data/state');

export async function readState<T>(key: string, fallback: T): Promise<T> {
  try {
    const data = await fs.readFile(path.join(DATA_DIR, `${key}.json`), 'utf-8');
    return JSON.parse(data);
  } catch {
    return fallback;
  }
}

export async function writeState<T>(key: string, value: T): Promise<void> {
  await fs.mkdir(DATA_DIR, { recursive: true });
  await fs.writeFile(path.join(DATA_DIR, `${key}.json`), JSON.stringify(value, null, 2));
}