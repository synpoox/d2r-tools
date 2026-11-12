# Issue tracker: GitHub

Issues and specs for this repo live as GitHub issues in `synpoox/d2r-tools`. Use the `gh` CLI for all operations. Creating or commenting on issues happens only when the owner asks (see `AGENTS.md`).

## Conventions

- **Create an issue**: `gh issue create --title "..." --body "..."`. Use a heredoc for multi-line bodies.
- **Read an issue**: `gh issue view <number> --json number,title,body,labels,comments`.
- **List issues**: `gh issue list --state open --json number,title,body,labels,comments --jq '[.[] | {number, title, body, labels: [.labels[].name], comments: [.comments[].body]}]'` with appropriate `--label` and `--state` filters.
- **Comment on an issue**: `gh issue comment <number> --body "..."`
- **Apply / remove labels**: `gh issue edit <number> --add-label "..."` / `--remove-label "..."`. The role-to-label mapping is in [triage-labels.md](triage-labels.md).
- **Close**: `gh issue close <number> --comment "..."`

Infer the repo from `git remote -v`; `gh` does this automatically when run inside a clone.

## Sub-issues

Link each child to its parent with GitHub's sub-issues, not only in a comment: `gh issue create --parent <parent> ...`, or `gh issue edit <parent> --add-sub-issue <child>` afterwards (`gh` 2.94+). Older `gh`: `gh api --method POST repos/synpoox/d2r-tools/issues/<parent>/sub_issues -F sub_issue_id=<child-db-id>`, where `<child-db-id>` is `gh api repos/synpoox/d2r-tools/issues/<child> --jq .id` (the database id, not the `#number`). A `Part of #<parent>` line alone does not make the link.

## Blockers

Use GitHub's native issue dependencies: `gh api --method POST repos/synpoox/d2r-tools/issues/<child>/dependencies/blocked_by -F issue_id=<blocker-db-id>`, with the blocker's database id as above. A ticket is unblocked when every blocker is closed. Where dependencies are unavailable, put a `Blocked by: #<n>, #<n>` line at the top of the child body.

## Pull requests link their issue

Open a PR body with the issue it solves: `Closes #<n>` when merging finishes the issue, `Part of #<n>` when it does not. GitHub shares one number space across issues and PRs, so a bare `#42` may be either: try `gh pr view 42`, then `gh issue view 42`.

## When a skill says "publish to the issue tracker"

Create a GitHub issue. "Fetch the relevant ticket" means **Read an issue** above.
