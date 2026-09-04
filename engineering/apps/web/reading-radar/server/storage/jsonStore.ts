import { promises as fs } from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const DATA_DIR = path.resolve(__dirname, '../../user-data/state');

const KEY_PATTERN = /^[a-zA-Z0-9_-]+$/;
export function validateKey(key: string): boolean {
  if (!key || key.length > 200) return false;
  return KEY_PATTERN.test(key);
}

export async function readState<T>(key: string, fallback: T): Promise<T> {
  if (!validateKey(key)) return fallback;
  try {
    const data = await fs.readFile(path.join(DATA_DIR, `${key}.json`), 'utf-8');
    return JSON.parse(data);
  } catch {
    return fallback;
  }
}

export async function writeState<T>(key: string, value: T): Promise<void> {
  if (!validateKey(key)) throw new Error(`Invalid key: ${key}`);
  await fs.mkdir(DATA_DIR, { recursive: true });
  await fs.writeFile(path.join(DATA_DIR, `${key}.json`), JSON.stringify(value, null, 2));
}
