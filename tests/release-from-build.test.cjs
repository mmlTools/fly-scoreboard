const { test } = require('node:test');
const assert = require('node:assert/strict');
const { validateTag, findBuild } = require('../.github/scripts/release-from-build.cjs');

const sha = 'abc1234567890abcdef1234567890abcdef1234567';
const spec = { name: 'fly-scoreboard', version: '5.1.0' };
const variants = ['windows-x64', 'macos-universal', 'ubuntu-26.04-x86_64'];
const artifacts = () => variants.map(variant => ({
  name: `${spec.name}-${spec.version}-${variant}-${sha.slice(0, 9)}`,
  expired: false, size_in_bytes: 100,
}));
const run = (id, overrides = {}) => ({
  id, head_sha: sha, event: 'workflow_dispatch', conclusion: 'success', ...overrides,
});
function api(runs, packages) {
  const listRuns = Symbol('runs');
  const listArtifacts = Symbol('artifacts');
  return {
    rest: { actions: { listWorkflowRuns: listRuns, listWorkflowRunArtifacts: listArtifacts } },
    paginate: async (method, params) => {
      if (method === listRuns) {
        assert.equal(params.workflow_id, 'dispatch.yaml');
        assert.equal(params.head_sha, sha);
        assert.equal(params.event, 'workflow_dispatch');
        assert.equal(params.status, 'success');
        return [...runs];
      }
      return packages[params.run_id] || [];
    },
  };
}

test('release tag must match the package version, with optional v prefix', () => {
  validateTag('5.1.0', '5.1.0');
  validateTag('v5.1.0', '5.1.0');
  validateTag('v5.1.0-rc1', '5.1.0-rc1');
  for (const tag of ['5.0.1', '5.1.0-extra', 'v5.1.0.1', 'anything']) {
    assert.throws(() => validateTag(tag, '5.1.0'), /must match/);
  }
});

test('chooses the newest successful manual build for the exact tagged commit', async () => {
  const result = await findBuild(api([run(1), run(3), run(2)], {
    1: artifacts(), 2: artifacts(), 3: artifacts(),
  }), {}, sha, spec);
  assert.equal(result.id, 3);
});

test('rejects nearby commits, failed runs and automatic builds', async () => {
  const runs = [run(1, { head_sha: 'other' }), run(2, { conclusion: 'failure' }),
    run(3, { event: 'push' })];
  await assert.rejects(findBuild(api(runs, { 1: artifacts(), 2: artifacts(), 3: artifacts() }), {}, sha, spec),
    /No successful manual build/);
});

test('skips expired, incomplete and incorrectly versioned packages', async () => {
  const expired = artifacts();
  expired[0].expired = true;
  const wrongVersion = artifacts().map(artifact => ({ ...artifact, name: artifact.name.replace('5.1.0', '5.0.1') }));
  const result = await findBuild(api([run(1), run(2), run(3), run(4)], {
    1: artifacts(), 2: artifacts().slice(0, 2), 3: expired, 4: wrongVersion,
  }), {}, sha, spec);
  assert.equal(result.id, 1);
});

test('fails clearly when packages are missing, without triggering a rebuild', async () => {
  await assert.rejects(findBuild(api([run(1)], {}), {}, sha, spec), /Run "Build release packages"/);
});
