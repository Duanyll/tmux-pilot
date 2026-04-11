# tmux-exec Test Plan

Manual test suite for `tmux-exec`. All tests use a dedicated tmux session.

## Setup

```bash
tmux kill-session -t test 2>/dev/null
tmux new-session -d -s test -x 120 -y 30
rm -rf /tmp/tmux-pilot/test*
P="test:0.0"
tmux-exec -t $P --check > /dev/null   # initialize state
```

## 1. Basic shell commands

### 1a. Simple echo

```bash
tmux-exec -t $P "echo hello world"
```

Expected:
- `status: command_completed | exit_code=0`
- Output mode: `incremental`
- Output: `hello world`

### 1b. Non-zero exit code

```bash
tmux-exec -t $P "false"
```

Expected:
- `status: command_completed | exit_code=1`
- `--- no output ---`
- Process exit code: 1

### 1c. Multi-command chain

```bash
tmux-exec -t $P "echo a && echo b && echo c"
```

Expected:
- `status: command_completed | exit_code=0`
- Output: three lines `a`, `b`, `c`

### 1d. Command with delay

```bash
tmux-exec -t $P "sleep 2 && echo done"
```

Expected:
- `duration` ~2.0s
- `status: command_completed | exit_code=0`
- Output: `done`

## 2. Output modes

### 2a. Output flood

```bash
tmux-exec -t $P "seq 1 10000"
```

Expected:
- `status: output_flood`
- Output mode: `capture-pane` (shows tail of visible output)
- Hint mentions `--scroll`

### 2b. Scrollback review

```bash
tmux-exec -t $P --scroll 50
```

Expected:
- `status: ok`
- Shows last 50 lines of scrollback history

### 2c. Check mode

```bash
tmux-exec -t $P --check
```

Expected:
- `status: ok`
- Shows current visible pane content
- No command sent

## 3. Interactive prompt detection

### 3a. read -p prompt

```bash
tmux-exec -t $P 'bash -c "read -p \"Name: \" x && echo got: \$x"'
```

Expected:
- `status: interactive_prompt`
- `duration` ~0.2s (fast detection)
- Output shows `Name:` on last line

### 3b. Send input after interactive prompt

```bash
tmux-exec -t $P "alice"
```

Expected:
- `mode=monitor` (auto-switched from shell, no marker appended)
- `status: prompt_detected`
- Output includes `got: alice`

### 3c. sudo password prompt (requires sudo configured)

```bash
tmux-exec -t $P "sudo ls /root"
```

Expected:
- `status: interactive_prompt` if password required
- `status: command_completed` if passwordless sudo

## 4. Pane change guard

### 4a. Guard triggers on manual input

```bash
tmux-exec -t $P "echo baseline"          # establish state
tmux send-keys -t $P "echo manual" Enter  # user types something
sleep 0.5
tmux-exec -t $P "echo should warn"
```

Expected:
- `status: pane_changed`
- Output shows current pane content including "manual" output
- Hint: "use --force to bypass"

### 4b. Force bypass

```bash
tmux-exec -t $P --force "echo forced"
```

Expected:
- `status: command_completed | exit_code=0`
- Output: `forced`

## 5. Prompt detection (Ctrl-C)

### 5a. Command interrupted by Ctrl-C

Run tmux-exec in background, then send Ctrl-C to the pane:

```bash
tmux-exec -t $P "sleep 999" &
BGPID=$!
sleep 2
tmux send-keys -t $P C-c
wait $BGPID
```

Expected:
- `status: prompt_detected`
- `duration` ~2.2s
- Output shows `^C` and the returned shell prompt

## 6. Non-shell environments

### 6a. Python REPL auto-detection

```bash
tmux send-keys -t $P "python3" Enter
sleep 1
tmux-exec -t $P --check > /dev/null   # save state with Python running
tmux-exec -t $P '2+2'
```

Expected:
- `mode=monitor` (auto-detected python3 as non-shell)
- `status: prompt_detected`
- Output includes `4`

### 6b. Explicit --no-shell

```bash
tmux-exec -t $P --no-shell 'print("hello")'
```

Expected:
- `mode=monitor`
- `status: prompt_detected`
- Output includes `hello`

### 6c. Exit Python REPL

```bash
tmux-exec -t $P --no-shell 'exit()'
```

Expected:
- `status: prompt_detected`
- Pane returns to shell prompt

## 7. Raw mode

### 7a. Send keystrokes

```bash
tmux-exec -t $P --raw "echo raw test"
```

Expected:
- `status: sent`
- No monitoring, returns immediately
- Hint: "use --check to inspect pane"

### 7b. No guard check in raw mode

```bash
tmux send-keys -t $P "echo change" Enter   # change pane
sleep 0.5
tmux-exec -t $P --raw "echo no guard"      # should NOT trigger guard
```

Expected:
- `status: sent` (not `pane_changed`)

## 8. Error handling

### 8a. Non-existent pane

```bash
tmux-exec -t nonexistent:0.0 "echo test"
```

Expected:
- `status: pane_error`
- Hint: "target pane not found"
- Exit code: 1

### 8b. Multi-line command rejected

```bash
printf 'line1\nline2' | xargs -I{} tmux-exec -t $P "{}"
```

Expected:
- Error on stderr: "multi-line commands are not supported"

### 8c. Mutually exclusive flags

```bash
tmux-exec -t $P --shell --no-shell "echo test"
```

Expected:
- Error on stderr: "--shell and --no-shell are mutually exclusive"

## 9. Idle timeout

### 9a. Command with no output

```bash
tmux-exec -t $P --idle-timeout 3 "sleep 30"
```

Expected:
- `status: output_stable`
- `duration` ~3s
- Hint: "no output for 3s; command may still be running"

Note: after this test, Ctrl-C the sleep in the pane before continuing:
```bash
tmux send-keys -t $P C-c
sleep 0.5
```

## Teardown

```bash
tmux kill-session -t test
rm -rf /tmp/tmux-pilot/test*
```
