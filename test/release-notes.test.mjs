import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";
import { analyzeCommits } from "@semantic-release/commit-analyzer";
import { generateNotes } from "@semantic-release/release-notes-generator";

test("configured Conventional Commits preset analyzes and renders releases", async () => {
  const config = JSON.parse(
    await readFile(new URL("../.releaserc.json", import.meta.url), "utf8"),
  );
  const optionsFor = (name) =>
    config.plugins.find(([plugin]) => plugin === name)[1];
  const context = {
    cwd: process.cwd(),
    env: {},
    logger: { log() {} },
    options: {
      repositoryUrl: "https://github.com/mindflayer-vtt/mindflayer-keypad.git",
    },
    commits: [
      { hash: "a".repeat(40), message: "feat(keypad): add provisioning" },
      { hash: "b".repeat(40), message: "fix(keypad): debounce keys" },
    ],
    lastRelease: { gitTag: "v1.0.0", gitHead: "c".repeat(40) },
    nextRelease: {
      version: "1.1.0",
      gitTag: "v1.1.0",
      gitHead: "a".repeat(40),
    },
  };
  assert.equal(
    await analyzeCommits(
      optionsFor("@semantic-release/commit-analyzer"),
      context,
    ),
    "minor",
  );
  const notes = await generateNotes(
    optionsFor("@semantic-release/release-notes-generator"),
    context,
  );
  assert.match(notes, /1\.1\.0/);
  assert.match(notes, /add provisioning/);
  assert.match(notes, /debounce keys/);
  assert.match(notes, /compare\/v1\.0\.0\.\.\.v1\.1\.0/);
});
