# Interactive CLI Control via tmux

Execute commands and control interactive CLI programs through tmux panes using the `tmux-exec` wrapper. This gives you the same experience as running local Bash commands — streaming output, accurate exit codes, clean filtered text.

## Phase 1: Identify the remote environment

When starting a remote admin session, first discover tmux panes and confirm what you're connected to.

### 1a. Find SSH panes

```bash
tmux list-panes -a -F '#{session_name}:#{window_index}.#{pane_index}  [#{pane_current_command}]  #{pane_title}'
```

Look for panes where `pane_current_command` is `ssh`. The target identifier is the `session:window.pane` string (e.g., `0:0.0`).

If multiple SSH panes exist, confirm with the user which server they want before proceeding.

### 1b. Confirm hostname and user

Before running any operational commands, verify the remote context by inspecting the pane's current state:

```bash
tmux capture-pane -t <pane> -p -S -5
```

This shows the last few lines including the shell prompt, which usually reveals `user@hostname`. If the prompt isn't informative enough, run an explicit check:

```bash
tmux-exec -t <pane> "whoami && hostname && pwd"
```

Report the remote identity (user, hostname, working directory) to the user before doing anything else. This is a safety check — especially important when the remote session might have root access.

## Phase 2: Run commands with tmux-exec

Use `tmux-exec` via the Bash tool:

```bash
tmux-exec -t <pane> "<command>"
```

For example:
```bash
tmux-exec -t 0:0.0 "sudo systemctl status nginx"
```

The Bash tool's `timeout` parameter applies — set it appropriately for long commands (max 600000ms = 10 min).

### Important: single-line commands only

**tmux-exec only accepts single-line commands.** Do NOT use heredocs, multi-line strings, or newlines in the command. If you need to write a file to the remote server, use `tmux-upload` (see Phase 2b below).

For multi-command operations, chain with `&&`, `||`, or `;` on a single line:

```bash
tmux-exec -t 0:0.0 "cd /etc/nginx && sudo nginx -t && sudo systemctl reload nginx"
```

### How tmux-exec works

- Sends the command via `tmux send-keys`
- Captures output in real time via `tmux pipe-pane` + FIFO, with `capture-pane` fallback
- Filters raw PTY output: strips ANSI escapes, collapses CR-based progress bars, deduplicates consecutive lines, removes command echo
- Detects a unique completion marker to know when the command finishes
- Returns the remote command's exit code

### Quoting

The command string is sent as literal keystrokes to the remote shell. Use proper shell quoting for the remote side:

```bash
tmux-exec -t 0:0.0 "grep 'error' /var/log/syslog | tail -20"
```

## Phase 2b: Upload files with tmux-upload

To write files on the remote server, use `tmux-upload`. This tool base64-encodes a local file and decodes it on the remote side — avoiding all shell quoting and heredoc issues.

### Basic usage

First write the desired content to a local file, then upload it. Choose a sensible location for the staging file — if you're working inside a project, use a project-local path (e.g., a `tmp/` or `build/` directory) so the user can inspect it; otherwise `/tmp` is fine.

```bash
# 1. Write content locally (use the Write tool)
#    e.g., write to ./tmp/nginx-example.conf or /tmp/nginx-example.conf

# 2. Upload to remote
tmux-upload -t <pane> ./tmp/nginx-example.conf /etc/nginx/sites-available/example.conf
```

### Writing to protected paths (sudo)

Use the `-s` flag to write via `sudo tee`:

```bash
tmux-upload -t <pane> -s ./tmp/nginx-example.conf /etc/nginx/sites-available/example.conf
```

### Workflow for editing remote files

1. Read the current file:
   ```bash
   tmux-exec -t <pane> "cat /etc/some/config.conf"
   ```
2. Write the new content to a local file (using the Write tool). Prefer a project-local path when inside a project directory.
3. Upload it:
   ```bash
   tmux-upload -t <pane> -s <local-file> /etc/some/config.conf
   ```
4. Verify:
   ```bash
   tmux-exec -t <pane> "cat /etc/some/config.conf"
   ```

## Phase 3: Interactive and special situations

The user can always interact with the remote terminal directly through the tmux window. Use this to your advantage — when something requires human input, fall back to `send-keys` + `capture-pane` mode.

### sudo password prompt

If a command needs a sudo password, use this workflow:

1. Send the command via `tmux send-keys`:
   ```bash
   tmux send-keys -t <pane> "sudo apt upgrade -y" Enter
   ```
2. Tell the user: "I've sent the command. It may be waiting for your sudo password — please check the tmux window and enter it."
3. After the user confirms they've entered the password (or after a reasonable wait), poll the result:
   ```bash
   tmux capture-pane -t <pane> -p -S -30
   ```
4. Once the output shows the command has completed, resume using tmux-exec for subsequent commands.

### Full-screen / interactive programs

Programs like `vim`, `htop`, `less`, `top`, `journalctl` (pager mode) don't produce linear output and won't work through tmux-exec. Use the send-keys + capture-pane pattern:

1. Launch the program:
   ```bash
   tmux send-keys -t <pane> "htop" Enter
   ```
2. Tell the user the program is running and they can interact with it in tmux.
3. When they're done, capture the pane to see the current state or confirm they've exited:
   ```bash
   tmux capture-pane -t <pane> -p -S -20
   ```

### Long-running commands (>10 min)

For operations that may exceed the Bash tool timeout:

- **Background on remote**: `tmux-exec -t <pane> "nohup long-command > /tmp/output.log 2>&1 &"`
- **send-keys + poll**: Send the command, then periodically capture-pane to check progress:
  ```bash
  tmux send-keys -t <pane> "long-running-command" Enter
  ```
  Later:
  ```bash
  tmux capture-pane -t <pane> -p -S -50
  ```

### Checking pane state at any time

If you need to see what's currently on screen (e.g., to check if a previous command finished, or see an error message the user mentioned):

```bash
tmux capture-pane -t <pane> -p -S -30
```

This is always safe and doesn't affect the remote terminal.
