#!/usr/bin/env bash
# Archive every comment, review, and review-comment from the 7 upstream
# PRs we filed against N64Recomp/N64Recomp + N64Recomp/N64ModernRuntime.
# Writes one markdown file per PR to the local handoff archive dir.

set -euo pipefail
ARCHIVE="F:/Projects/_personal-notes/psr-handoffs/upstream-pr-archive-2026-05-05"
mkdir -p "$ARCHIVE"

dump_pr() {
    local repo="$1"
    local num="$2"
    local out="$ARCHIVE/pr-$num-${repo//\//-}.md"

    {
        # Header + metadata + body
        gh pr view "$num" --repo "$repo" --json number,title,state,closedAt,createdAt,author,headRefName,baseRefName,url,body \
            --template '# PR #{{.number}}: {{.title}}

**Repo:** `{{`'"$repo"'`}}`
**State:** {{.state}}
**Author:** @{{.author.login}}
**Created:** {{.createdAt}}
**Closed:** {{.closedAt}}
**Head:** `{{.headRefName}}`  →  **Base:** `{{.baseRefName}}`
**URL:** {{.url}}

---

## PR body

{{.body}}

---

'

        echo "## Issue comments (top-level conversation)"
        echo ""
        local n_issue
        n_issue=$(gh api "repos/$repo/issues/$num/comments" --jq 'length')
        if [ "$n_issue" = "0" ]; then
            echo "_(none)_"
        else
            gh api "repos/$repo/issues/$num/comments" \
                --jq '.[] | "### @" + .user.login + " — " + .created_at + "\n\n" + .body + "\n\n---\n"'
        fi
        echo ""

        echo "## Review submissions (approve / request-changes / comment summary)"
        echo ""
        local n_rev
        n_rev=$(gh api "repos/$repo/pulls/$num/reviews" --jq 'length')
        if [ "$n_rev" = "0" ]; then
            echo "_(none)_"
        else
            gh api "repos/$repo/pulls/$num/reviews" \
                --jq '.[] | "### @" + .user.login + " — " + (.submitted_at // "(unsubmitted)") + " (" + .state + ")\n\n" + (.body // "_(no review body)_") + "\n\n---\n"'
        fi
        echo ""

        echo "## Inline review comments (line-level)"
        echo ""
        local n_rc
        n_rc=$(gh api "repos/$repo/pulls/$num/comments" --jq 'length')
        if [ "$n_rc" = "0" ]; then
            echo "_(none)_"
        else
            gh api "repos/$repo/pulls/$num/comments" \
                --jq '.[] | "### @" + .user.login + " — " + .created_at + "\n**File:** `" + .path + "`" + (if .line then " line " + (.line|tostring) else "" end) + (if .original_line then " (orig line " + (.original_line|tostring) + ")" else "" end) + "\n\n```diff\n" + (.diff_hunk // "") + "\n```\n\n" + .body + "\n\n---\n"'
        fi
        echo ""

        # Append raw JSON for full archival fidelity
        echo "## Raw JSON archival (for completeness)"
        echo ""
        echo "<details><summary>PR JSON</summary>"
        echo ""
        echo '```json'
        gh pr view "$num" --repo "$repo" --json number,title,state,closedAt,createdAt,author,headRefName,baseRefName,url,body,additions,deletions,changedFiles
        echo ""
        echo '```'
        echo "</details>"
        echo ""
        echo "<details><summary>Issue comments JSON</summary>"
        echo ""
        echo '```json'
        gh api "repos/$repo/issues/$num/comments"
        echo ""
        echo '```'
        echo "</details>"
        echo ""
        echo "<details><summary>Reviews JSON</summary>"
        echo ""
        echo '```json'
        gh api "repos/$repo/pulls/$num/reviews"
        echo ""
        echo '```'
        echo "</details>"
        echo ""
        echo "<details><summary>Review comments JSON</summary>"
        echo ""
        echo '```json'
        gh api "repos/$repo/pulls/$num/comments"
        echo ""
        echo '```'
        echo "</details>"
    } > "$out"
    echo "wrote: $out  (issue=$n_issue, reviews=$n_rev, inline=$n_rc)"
}

dump_pr "N64Recomp/N64Recomp"        182
dump_pr "N64Recomp/N64Recomp"        183
dump_pr "N64Recomp/N64Recomp"        184
dump_pr "N64Recomp/N64Recomp"        185
dump_pr "N64Recomp/N64ModernRuntime" 142
dump_pr "N64Recomp/N64ModernRuntime" 143
dump_pr "N64Recomp/N64ModernRuntime" 144
