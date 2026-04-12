# tmux-pilot

Let Claude Code control interactive CLI programs through tmux panes.

tmux-pilot is a Claude Code plugin that provides scripts and a skill for executing commands in any tmux pane — local or remote (via SSH) — with intelligent output monitoring, completion detection, and structured status feedback.

## How it works

- **`tmux-exec`** — Sends commands to a target tmux pane, monitors output in real time via `pipe-pane`, and returns when the command completes, a prompt appears, or a timeout is reached. Automatically detects shell vs non-shell environments, handles interactive prompts, and provides clean filtered output with exit codes.
- **`tmux-upload`** — Uploads a local file to the target pane by base64-encoding it and decoding on the remote side. Supports `sudo tee` for protected paths.
- **Claude Code skill** (`/tmux-pilot`) — Teaches Claude Code how to discover tmux panes, run commands, upload files, and handle interactive situations.

## Install

```bash
# Clone the repo
git clone https://github.com/duanyll/tmux-pilot.git
```

Then launch Claude Code with the plugin enabled:

```bash
claude --plugin-dir /path/to/tmux-pilot
```

The plugin automatically adds `tmux-exec` and `tmux-upload` to your PATH and installs the pre-tool hook that guards against chained calls.

## Usage

1. Open an SSH session (or any interactive program) in a tmux pane
2. In Claude Code, invoke the skill: `/tmux-pilot`
3. Claude Code will discover the pane, confirm the target, and start executing commands

### Quick examples

```bash
# Check what's on screen
tmux-exec -t 0:0.0 --check

# Run a command (auto-detects shell mode)
tmux-exec -t 0:0.0 "ls -la /etc/nginx"

# Send keystrokes to an interactive program
tmux-exec -t 0:0.0 --raw "q"

# Review scrollback history
tmux-exec -t 0:0.0 --scroll 200
```

## Requirements

- tmux
- Python 3.10+
