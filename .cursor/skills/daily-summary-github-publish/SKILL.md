---
name: daily-summary-github-publish
description: Publish the latest project version to GitHub after the daily development summary is completed. Use when the user asks to upload/update/push today's version, or says daily summary is done and wants to sync code to GitHub.
---

# Daily Summary GitHub Publish

## Instructions

When the user has finished daily development summary and asks to upload the new version to GitHub, follow this workflow:

1. Confirm publish intent
   - Treat phrases like “总结完了上传 GitHub”, “把今天版本推上去”, “同步到 GitHub” as explicit permission to publish.
   - If intent is ambiguous, ask one short confirmation question before pushing.

2. Inspect repository state
   - Run `git status --short` to collect changed and untracked files.
   - Run `git branch --show-current` to confirm current branch.
   - Run `git remote -v` to confirm GitHub remote exists.

3. Build a daily commit message
   - Prefer date-aware message format:
     - `chore(daily): sync progress for YYYY.MM.DD`
   - Optional body line can mention the daily summary context.
   - Keep message concise and neutral.

4. Stage and commit safely
   - Stage only relevant project files.
   - Do not include secrets (`.env`, key files, credentials dumps).
   - If no file changes exist, report “nothing to commit” and stop.
   - Commit using a heredoc style message.

5. Push to GitHub
   - If branch has upstream: `git push`
   - If no upstream: `git push -u origin <current-branch>`
   - If push is rejected, report exact cause and suggest next action (pull/rebase or conflict resolution).

6. Return publish result
   - Report branch name.
   - Report commit hash and commit title.
   - Report whether push succeeded.
   - If available, provide repository/commit URL.

## Output Template

Use this concise response format:

```text
已完成今日版本上传。
- 分支：<branch>
- 提交：<short-hash> <commit-title>
- 推送：成功/失败（原因）
- 远程：<url 或 origin 名称>
```

## Examples

### Example Trigger

User says:
- “今天开发总结写完了，帮我把新版本传 GitHub。”

Assistant behavior:
- Runs status/branch/remote checks.
- Creates daily commit.
- Pushes to current branch.
- Returns hash + push result summary.

### Example Commit Message

```text
chore(daily): sync progress for 2026.04.17

Update code and notes after today's development summary.
```
