#!/usr/bin/env bash
# Pre-tool hook: reject Bash calls that chain multiple tmux-exec / tmux-upload invocations.
# Input: JSON on stdin with { "tool_input": { "command": "..." } }

COMMAND=$(python3 -c "import sys,json; print(json.load(sys.stdin)['tool_input']['command'])")

# Count occurrences of tmux-exec and tmux-upload as command invocations
COUNT=$(echo "$COMMAND" | grep -oE '(tmux-exec|tmux-upload)' | wc -l)

if [ "$COUNT" -gt 1 ]; then
    echo "BLOCKED: Do not chain multiple tmux-exec/tmux-upload calls in one Bash invocation. Chain remote commands inside a single tmux-exec call instead." >&2
    exit 2
fi
