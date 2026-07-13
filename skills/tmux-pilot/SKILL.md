---
name: tmux-pilot
description: "Interactive CLI Control via tmux — execute commands, handle interactive prompts, upload files to remote systems through tmux panes"
---

# Interactive CLI Control via tmux

Execute commands and control interactive CLI programs through tmux panes using the `tmux-exec` wrapper. It handles output monitoring, completion detection, and interactive prompt detection automatically — you get structured status information so you always know what happened.

## Phase 1: Identify the remote environment

When starting a remote admin session, first discover tmux panes and confirm what you're connected to.

### 1a. Find SSH panes

```bash
tmux list-panes -a -F '#{session_name}:#{window_index}.#{pane_index}  [#{pane_current_command}]  #{pane_title}'
```

Look for panes where `pane_current_command` is `ssh`. The target identifier is the `session:window.pane` string (e.g., `0:0.0`).

If multiple SSH panes exist, confirm with the user which server they want before proceeding.

### 1b. Confirm hostname and user

Inspect the pane's current state:

```bash
tmux-exec -t <pane> --check
```

This shows the current visible content including the shell prompt, which usually reveals `user@hostname`. If the prompt isn't informative enough, run an explicit check:

```bash
tmux-exec -t <pane> "whoami && hostname && pwd"
```

Report the remote identity (user, hostname, working directory) to the user before doing anything else.

## Phase 2: Run commands with tmux-exec

Use `tmux-exec` via the Bash tool:

```bash
tmux-exec -t <pane> "<command>"
```

For example:
```bash
tmux-exec -t 0:0.0 "sudo systemctl status nginx"
```

### Understanding the output

tmux-exec prints a single `[tmux-exec]` status line followed by any output lines:

```
[tmux-exec] target=0:0.0 mode=shell status=command_completed exit=0 duration=3.2s lines=5 src=stream
...actual output...
```

If there is no output, only the status line is printed. If a hint is relevant, it is appended after a `|` on the same line.

**Status values and what to do:**

| Status | Meaning | Next action |
|--------|---------|-------------|
| `command_completed` | Command finished (exit code available) | Continue with next command |
| `prompt_detected` | Shell prompt reappeared (likely finished) | Continue normally |
| `interactive_prompt` | Pane is waiting for user input | Send input or tell the user |
| `output_stable` | No output for idle_timeout seconds | Check pane state, command may still be running |
| `output_flood` | Too much output, returned early | Use `--scroll N` to review history |
| `timeout` | Hit the total timeout limit | Command is still running; use `--check` later |
| `pane_changed` | Pane content changed since last call | Review the output, use `--force` if safe |
| `pane_error` | Pane not found or tmux error | Check the target pane identifier |

### Options reference

| Option | Description |
|--------|-------------|
| `-t TARGET` | Target pane (required, e.g., `0:0.0` or `%5`) |
| `--check` | Return current pane state without sending anything |
| `--raw` | Pass following args through to `tmux send-keys` verbatim (see Phase 3) |
| `--force` | Bypass the pane-changed guard |
| `--shell` | Force shell mode (append printf marker for exit code) |
| `--no-shell` | Force non-shell mode (no marker) |
| `--scroll N` | Return N lines of scrollback history |
| `--timeout N` | Total timeout in seconds (default: 110) |
| `--idle-timeout N` | Seconds of no output before returning (default: 10) |

### Shell mode vs monitor mode

tmux-exec auto-detects whether the pane is running a shell (bash, zsh, ssh, etc.) or something else (python, gdb, etc.):

- **Shell mode** (auto for shell/ssh): Appends a printf marker to reliably detect completion and capture exit code.
- **Monitor mode** (auto for non-shell): Relies on prompt detection and output stabilization. No exit code.

Override with `--shell` or `--no-shell` when auto-detection is wrong.

When the last call returned `interactive_prompt`, the next call automatically switches to monitor mode — it sends your input directly to the waiting program without a marker.

### Important: single-line commands only

**tmux-exec only accepts single-line commands.** Do NOT use heredocs, multi-line strings, or newlines. For multi-command operations, chain with `&&`, `||`, or `;`:

```bash
tmux-exec -t 0:0.0 "cd /etc/nginx && sudo nginx -t && sudo systemctl reload nginx"
```

### CRITICAL: one tmux-exec per Bash call

**Never chain or sequence multiple `tmux-exec` / `tmux-upload` invocations in a single Bash call** (e.g., `tmux-exec ... && tmux-exec ...` or `tmux-exec ...; tmux-exec ...`). tmux-exec does not always propagate exit codes reliably, so external chaining may not short-circuit as expected. Worse, if the first command hangs or the pane enters an unexpected state, the second `tmux-exec` will blindly send new keystrokes into that state.

Instead, chain the remote commands **inside** a single tmux-exec call:

```bash
# WRONG — do not do this
tmux-exec -t 0:0.0 "cd /app" && tmux-exec -t 0:0.0 "make build"

# RIGHT — chain inside the remote command
tmux-exec -t 0:0.0 "cd /app && make build"
```

Each Bash tool call should contain **at most one** `tmux-exec` or `tmux-upload` invocation.

### Quoting

The command string is sent as literal keystrokes to the remote shell. Use proper shell quoting for the remote side:

```bash
tmux-exec -t 0:0.0 "grep 'error' /var/log/syslog | tail -20"
```

## Phase 2b: Upload files with tmux-upload

To write files on the remote server, use `tmux-upload`. This tool base64-encodes a local file and decodes it on the remote side — avoiding all shell quoting and heredoc issues.

**Size limit:** Files larger than 48 KB are rejected. For larger files, use `scp` or split the file and upload chunks separately.

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

### Options reference

| Option | Description |
|--------|-------------|
| `-t TARGET` | Target pane (required, e.g., `0:0.0` or `%5`) |
| `-s` / `--sudo` | Write via `sudo tee` (for protected paths) |
| `--force` | Pass `--force` to tmux-exec (bypass pane-changed guard) |

### Workflow for editing remote files

**For simple changes** (toggling a setting, changing a value, commenting a line), use `sed` directly — no need to download/upload:

```bash
tmux-exec -t <pane> "sudo sed -i 's/worker_connections 768/worker_connections 1024/' /etc/nginx/nginx.conf"
```

**For larger rewrites** (new files, multi-section edits), use the download → edit → upload workflow:

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

tmux-exec handles most interactive situations automatically. The user can always interact with the remote terminal directly through the tmux window.

### Interactive prompts (password, y/N, etc.)

tmux-exec detects interactive prompts and returns `interactive_prompt` status. When you see this:

1. If you know the answer (e.g., "y" for a confirmation): just call tmux-exec again with the answer — it will automatically send it without a marker:
   ```bash
   tmux-exec -t <pane> "y"
   ```
2. If it needs user input (e.g., a password): tell the user to enter it in the tmux window, then use `--check` to see the result:
   ```bash
   tmux-exec -t <pane> --check
   ```
3. To cancel/abort a prompt (or any foreground process), send Ctrl-C via `--raw`:
   ```bash
   tmux-exec -t <pane> --raw C-c
   ```

### Debuggers and REPLs (pdb, lldb, gdb, python, etc.)

tmux-exec recognizes REPL and debugger prompts (`(Pdb)`, `(lldb)`, `(gdb)`, `>>>`, etc.) and returns `interactive_prompt`. Subsequent calls automatically switch to monitor mode, so you can send debugger commands directly.

```bash
# Launch lldb — disable its TUI status bar, which interferes with prompt detection
tmux-exec -t <pane> "lldb -o 'settings set show-statusline false' /path/to/binary"
# Now send debugger commands normally
tmux-exec -t <pane> "b main"
tmux-exec -t <pane> "r"
tmux-exec -t <pane> "p some_var"
tmux-exec -t <pane> "q"
```

For pdb and gdb, no special flags are needed:

```bash
tmux-exec -t <pane> "python3 -m pdb script.py"
```

After quitting the debugger, tmux-exec detects the shell prompt and automatically restores shell mode for the next call.

### `--raw`: sending keystrokes verbatim

`--raw` is a thin wrapper around `tmux send-keys`. Every positional argument after `--raw` is forwarded to `tmux send-keys` **as-is**, with no completion marker, no output monitoring, and **no auto-appended Enter**. It returns immediately; use `--check` afterward to see what happened.

This is the only way to send key names and control characters (Ctrl-C, Escape, arrow keys, etc.), and the right tool for driving full-screen programs like `vim`, `htop`, `less`, `top` that don't produce linear output.

**Key arguments follow tmux send-keys naming:**
- Control / meta: `C-c`, `C-d`, `C-l`, `M-x`
- Named keys: `Enter`, `Escape`, `Space`, `Tab`, `BSpace`, `Up`, `Down`, `Left`, `Right`, `PageUp`, `PageDown`, `Home`, `End`, `F1`..`F12`
- Anything else (quoted string): typed as literal characters

**Common patterns:**

```bash
# Send Ctrl-C to cancel a prompt or running process (no Enter!)
tmux-exec -t <pane> --raw C-c

# Press Escape
tmux-exec -t <pane> --raw Escape

# Launch a full-screen program — pass the command AND Enter as separate args
tmux-exec -t <pane> --raw htop Enter

# Send a single keystroke to the running program (htop's "quit" is 'q', no Enter)
tmux-exec -t <pane> --raw q

# Multiple keys in one call — e.g., arrow down twice, then Enter
tmux-exec -t <pane> --raw Down Down Enter

# Inspect what happened
tmux-exec -t <pane> --check
```

**Important:** If you want the pane to actually execute what you typed (e.g., run a command), you must include `Enter` as its own argument. Forgetting it is the most common mistake. Conversely, when sending a control key like `C-c`, do **not** add `Enter` — it would generate an extra blank line after the cancel.

### Pane change guard

tmux-exec tracks pane state between calls. If the pane content changed unexpectedly (user typed something, background process output), it returns `pane_changed` instead of executing. This prevents blindly sending commands into an unknown state.

When you see `pane_changed`:
1. Review the returned output to understand what changed
2. If it's safe to proceed, use `--force`:
   ```bash
   tmux-exec -t <pane> --force "<command>"
   ```

### Long-running commands

For operations that may exceed the default timeout (110s):

- **Increase timeout upfront**: `tmux-exec -t <pane> --timeout 300 "long-command"` (up to 600s with Bash tool timeout)
- **Keep waiting after a timeout**: if a previous `tmux-exec` returned `output_stable` or `timeout`, call `tmux-exec -t <pane> --check --timeout N` to wait up to N more seconds for it to finish. This is the correct way to wait — do **not** `sleep N && tmux-exec --check`.
- **Background on remote**: `tmux-exec -t <pane> "nohup long-command > /tmp/output.log 2>&1 &"`
- **Instant snapshot**: `tmux-exec -t <pane> --check` (no `--timeout`) returns the current pane immediately.

#### How `--check --timeout N` waits

When `--timeout N` is passed with `--check`, tmux-exec polls the pane and returns as soon as one of these happens:

- The marker from the previous exec appears → `status=command_completed` with exit code.
- A shell/REPL prompt reappears at the bottom of the pane → `status=prompt_detected`.
- `N` seconds elapse without either → `status=timeout` (just call `--check --timeout` again to keep waiting).

**Important:** `--check --timeout` does **not** apply an idle-timeout by default. A command that hangs silently (no output) will keep being waited on until `--timeout` is hit — which is usually what you want. Only pass `--idle-timeout M` if you explicitly want to give up after M seconds of an unchanged pane.

### Reviewing scrollback

When output was truncated (flood) or you need to see history:

```bash
tmux-exec -t <pane> --scroll 200
```
