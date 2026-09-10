#requires -Version 7.0
param([string]$Repository=$env:GITHUB_REPOSITORY,[switch]$CheckOnly)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
Set-Location $root
$config=Get-Content extension.json -Raw | ConvertFrom-Json
$release=Invoke-RestMethod "https://api.github.com/repos/$($config.upstream_repository)/releases/latest" -Headers @{'User-Agent'='rime-skins-upstream-check'}
$tag=$release.tag_name
if ($tag -notmatch '^\d+\.\d+\.\d+$') { throw 'Unexpected stable tag; review manually.' }
if ([version]$tag -le [version]$config.upstream_tag) { Write-Output 'No newer stable upstream release.'; exit }
Write-Output "Upstream $tag is newer than tested base $($config.upstream_tag)."
if ($CheckOnly) { exit }
if (!$Repository) { throw 'Target repository is required.' }
if (git status --porcelain) { throw 'Use a clean checkout for upstream integration.' }
$branch="upstream/$tag"
$existing=& gh pr list --repo $Repository --state all --head $branch --json number --jq 'length'
if ($LASTEXITCODE) { throw 'Cannot inspect existing PRs.' }
if ([int]$existing -gt 0) { Write-Output 'Update PR already exists.'; exit }
git fetch --no-tags "https://github.com/$($config.upstream_repository).git" "refs/tags/$tag"
if ($LASTEXITCODE) { throw 'Upstream fetch failed.' }
$upstreamCommit=git rev-parse 'FETCH_HEAD^{commit}'
git switch -c $branch
if ($LASTEXITCODE) { throw 'Branch creation failed.' }
git merge --no-commit --no-ff $upstreamCommit
if ($LASTEXITCODE) {
 $conflicts=git diff --name-only --diff-filter=U
 git merge --abort
 $body="Upstream $tag could not be merged automatically.`n`nConflicting files:`n"+($conflicts -join "`n")
 $path=Join-Path $env:RUNNER_TEMP 'upstream-conflict.md'
 [IO.File]::WriteAllText($path,$body,[Text.UTF8Encoding]::new($false))
 $issues=& gh issue list --repo $Repository --search "in:title Upstream $tag merge conflict" --state open --json number --jq 'length'
 if ([int]$issues -eq 0) { & gh issue create --repo $Repository --title "Upstream $tag merge conflict" --body-file $path }
 if ($LASTEXITCODE) { throw 'Could not report upstream conflict.' }
 exit
}
$config.upstream_tag=$tag
$config.upstream_commit=$upstreamCommit
$config | ConvertTo-Json | Set-Content extension.json -Encoding utf8
git add extension.json
git commit -m "Merge upstream Weasel $tag for compatibility review"
if ($LASTEXITCODE) { throw 'Merge commit failed.' }
git push origin "HEAD:refs/heads/$branch"
if ($LASTEXITCODE) { throw 'Update branch push failed.' }
$body="Updates the tested upstream baseline candidate to $tag. This is a draft: review conflicts, dependencies, native builds, installation rollback, real TSF input and cold-start/memory measurements before merging. No release is published automatically."
$path=Join-Path $env:RUNNER_TEMP 'upstream-pr.md'
[IO.File]::WriteAllText($path,$body,[Text.UTF8Encoding]::new($false))
& gh pr create --repo $Repository --head $branch --base $env:BASE_BRANCH --draft --title "Update upstream Weasel to $tag" --body-file $path
if ($LASTEXITCODE) { throw 'PR creation failed.' }
& gh workflow run skin-checks.yml --repo $Repository --ref $branch
if ($LASTEXITCODE) { throw 'Update PR exists but its checks could not be dispatched.' }
