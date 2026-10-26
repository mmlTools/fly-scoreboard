const requiredVariants = ['windows-x64', 'macos-universal', 'ubuntu-26.04-x86_64'];

function validateTag(tag, version) {
  if (!/^v?\d+\.\d+\.\d+(?:-(?:beta|rc)\d+)?$/.test(tag) || tag.replace(/^v/, '') !== version) {
    throw new Error(`Release tag ${tag} must match buildspec.json version ${version} (optional v prefix).`);
  }
}

async function findBuild(github, repo, sha, spec) {
  const runs = await github.paginate(github.rest.actions.listWorkflowRuns, {
    ...repo, workflow_id: 'dispatch.yaml', event: 'workflow_dispatch', head_sha: sha,
    status: 'success', per_page: 100,
  });
  for (const run of runs.sort((a, b) => b.id - a.id)) {
    // Check again explicitly: never release a neighboring commit or incomplete run.
    if (run.head_sha !== sha || run.event !== 'workflow_dispatch' || run.conclusion !== 'success') continue;
    const artifacts = await github.paginate(github.rest.actions.listWorkflowRunArtifacts, {
      ...repo, run_id: run.id, per_page: 100,
    });
    const prefix = `${spec.name}-${spec.version}-`;
    const suffix = `-${sha.slice(0, 9)}`;
    if (requiredVariants.every(variant => artifacts.some(artifact =>
      !artifact.expired && artifact.size_in_bytes > 0 && artifact.name === `${prefix}${variant}${suffix}`))) {
      return run;
    }
  }
  throw new Error(`No successful manual build with all unexpired packages exists for ${sha}. ` +
    'Run "Build release packages" on this exact commit, wait for success, then rerun this release job.');
}

module.exports = { validateTag, findBuild };
