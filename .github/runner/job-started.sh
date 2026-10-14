#!/usr/bin/env bash
# .github/runner/job-started.sh: refuse a job that a fork's pull request started (#26).
#
# Ported from dudleylane/OrderProcessor's scripts/runner/job-started.sh as merged for its #143
# (1367db4); the decision logic is unchanged.
#
# This repository's CI runs on a self-hosted runner on the maintainer's machine, and its jobs
# run as the maintainer's user. The repository is public, so anyone can open a pull request
# from a fork, and GitHub runs it once a maintainer approves the run (the approval policy is
# all_external_contributors). A pull_request run uses the workflow files from the pull
# request's own merge commit, so a fork can change any condition a workflow sets, or add a
# workflow of its own. This hook is out of the pull request's reach. The runner runs it from
# the host before every job, and a non-zero exit fails the job before any of its steps run.
#
# It reads the job's event payload. A payload with a pull_request object comes from a pull
# request event (pull_request, pull_request_review, ...). Such a job runs only when the pull
# request's head repository is its base repository. Any other event, such as a push or a
# manual dispatch, runs as before. The payload must be the one the runner wrote for this
# job, in its own _work directory: a job is refused when the payload is anywhere else or
# can't be parsed.
#
# This copy is the reference; the runner runs its own copy, which a pull request can't reach.
# Install it in the runner's directory, outside _work, and restart the runner when it is
# idle, since the runner reads .env when it starts:
#   install -m 755 .github/runner/job-started.sh ~/actions-runner-quantlib/job-started.sh
#   echo "ACTIONS_RUNNER_HOOK_JOB_STARTED=$HOME/actions-runner-quantlib/job-started.sh" \
#     >> ~/actions-runner-quantlib/.env
#   systemctl --user restart quantlib-runner.service
# Every job's log then shows this hook's verdict before the job's first step. To roll back,
# delete the .env line and restart the runner.
#
# Exit status: 0 runs the job, 1 refuses it.

set -u

refuse() {
    echo "job-started hook (#26): $1; refusing the job" >&2
    exit 1
}

runner_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd) || refuse "can't find the runner's directory"
expected="$runner_dir/_work/_temp/_github_workflow/event.json"
event=${GITHUB_EVENT_PATH:-}
[[ -n "$event" ]] || refuse "GITHUB_EVENT_PATH is not set"
[[ -r "$event" ]] || refuse "can't read the event payload $event"
[[ "$(realpath -e -- "$event")" == "$(realpath -e -- "$expected" 2>/dev/null)" ]] ||
    refuse "the event payload $event is not the one the runner writes, $expected"

# head.repo is null once the fork has been deleted.
verdict=$(jq -e -r --slurp '
    if length != 1 or (.[0] | type) != "object" then
        error("not one JSON object")
    else .[0] end
    | if has("pull_request") | not then
        "run: not a pull request"
    elif (.pull_request | type) != "object" then
        "refuse: a pull_request that is not an object"
    elif .pull_request.base.repo.full_name != null
         and .pull_request.head.repo.full_name == .pull_request.base.repo.full_name then
        "run: a pull request from \(.pull_request.head.repo.full_name)"
    else
        "refuse: a pull request from \(.pull_request.head.repo.full_name // "a deleted repository") into \(.pull_request.base.repo.full_name // "an unknown repository")"
    end' "$event" 2>&1) || refuse "can't parse the event payload $event: $verdict"

echo "job-started hook (#26): ${GITHUB_EVENT_NAME:-unknown} event, $verdict"
if [[ "$verdict" != "run: "* ]]; then
    echo "A fork's pull request doesn't run on this self-hosted runner (#26)." >&2
    exit 1
fi
