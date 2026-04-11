# tmux-pilot

Let Claude Code control interactive CLI programs through tmux panes.

tmux-pilot provides two shell scripts and a Claude Code skill that together allow Claude Code to execute commands in any tmux pane — local or remote (via SSH) — with streaming output, accurate exit codes, and clean filtered text.

## How it works

- **`tmux-exec`** — Runs a command in a target tmux pane via `send-keys`, streams filtered output through a FIFO (`pipe-pane`), and returns the remote exit code. Handles ANSI stripping, progress bar collapsing, and line deduplication.
- **`tmux-upload`** — Uploads a local file to the target pane by base64-encoding it and decoding on the remote side. Supports `sudo tee` for protected paths.
- **Claude Code skill** (`.claude/commands/tmux-pilot.md`) — Teaches Claude Code how to discover tmux panes, run commands, upload files, and handle interactive situations (sudo prompts, full-screen programs, long-running tasks).

## Install

```bash
# Clone the repo
git clone https://github.com/user/tmux-pilot.git
cd tmux-pilot

# Add the scripts to your PATH
ln -s "$(pwd)/bin/tmux-exec" ~/.local/bin/tmux-exec
ln -s "$(pwd)/bin/tmux-upload" ~/.local/bin/tmux-upload
```

The Claude Code skill is automatically available when you run Claude Code inside this repo. To make it available globally, symlink it:

```bash
mkdir -p ~/.claude/commands
ln -s "$(pwd)/.claude/commands/tmux-pilot.md" ~/.claude/commands/tmux-pilot.md
```

## Usage

1. Open an SSH session (or any interactive program) in a tmux pane
2. In Claude Code, invoke the skill: `/tmux-pilot`
3. Claude Code will discover the pane, confirm the target, and start executing commands

## Requirements

- tmux
- bash 4+
- base64 (coreutils)
