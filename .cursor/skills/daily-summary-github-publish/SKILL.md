---
name: daily-summary-github-publish
description: Publish project updates to GitHub after daily summary is finished, including first-time repository initialization, safe commit/push workflow, large-directory exclusion for embedded projects, and network/auth retry guidance. Use when the user asks to upload/sync/push today's version.
---

# Daily Summary GitHub Publish

## Instructions

When the user asks to upload today's work to GitHub, follow this workflow.

1. Confirm intent and target
   - Treat phrases like “总结完了上传 GitHub”, “同步到 GitHub”, “继续上传” as explicit permission.
   - Confirm repository target if missing (owner/repo), e.g. `Puzzleway/Smart_Speaker`.

2. Inspect repository state
   - Run `git status --short` to collect changed and untracked files.
   - Run `git branch --show-current` to confirm current branch.
   - Run `git remote -v` to confirm GitHub remote.
   - If current directory is not a git repo, initialize it.

3. First-time upload workflow (important)
   - If this is the first upload and repository history is huge/unhealthy:
     - Exclude heavy directories via `.gitignore` (embedded projects usually exclude `toolchain/`, `sherpa/`, `third_party/`).
     - Reinitialize history if user agrees: remove `.git`, then `git init`, `git add .`, and create a clean initial commit.
   - This avoids pushing old large blobs from previous commits.

4. Stage and commit safely
   - Stage only relevant project files.
   - Do not include secrets (`.env`, key files, credentials dumps).
   - If no file changes exist, report “nothing to commit” and stop.
   - Prefer date-aware commit message:
     - `chore(daily): sync progress for YYYY.MM.DD`
   - For clean first upload, message can be:
     - `chore: reinitialize repository with clean history`

5. Push to GitHub
   - If remote is missing, add it:
     - `git remote add origin https://github.com/<owner>/<repo>.git`
   - If branch has upstream: `git push`
   - If no upstream: `git push -u origin <current-branch>`
   - If push fails with network/TLS reset, retry and suggest:
     - `git config --global http.version HTTP/1.1`
     - retry `git push -u origin <branch>`

6. Authentication and security
   - For HTTPS push, user may need GitHub PAT.
   - Never ask user to paste token in chat.
   - If user accidentally leaks token, instruct immediate revoke/regenerate.

7. Return publish result
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
- Checks git state and remote.
- Excludes large directories if needed.
- Commits and pushes.
- Reports hash + push result.

### Example Commit Message

```text
chore(daily): sync progress for 2026.04.17

Update code and notes after today's development summary.
```

### Example First Upload (clean history)

```text
chore: reinitialize repository with clean history

Recreate git history while excluding toolchain, sherpa, and third_party for a smaller initial upload.
```
