import { mkdir, readdir, copyFile } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const src = path.join(root, "src");
const dist = path.join(root, "dist");

await mkdir(dist, { recursive: true });
for (const entry of await readdir(src, { withFileTypes: true })) {
  if (entry.isFile() && entry.name.endsWith(".mjs")) {
    await copyFile(path.join(src, entry.name), path.join(dist, entry.name));
  }
}

console.log(`worker build copied ${src} -> ${dist}`);
