# Mynewt integration

Add `src/re.c`, `src/re.h`, `matcher.c`, `matcher.h` and the generated pattern header to the
application package. `command_re.h` was generated with:

```
node tools/re_compile.js '^(?:get|set) [a-z]+$' command_re
```

Store one matcher state in each task context or another task-owned static object:

```c
static struct command_matcher command_regex;

int result = command_match(&command_regex, input);
```

The example has no user capture groups, so each choice point uses four pointers. Its depth of 64
therefore reserves 1024 bytes on nRF52, in BSS rather than on the Mynewt task stack. Reduce or
increase `COMMAND_RE_WORKSPACE_DEPTH` for the application's maximum command length and expected
backtracking. Do not share one state between concurrent calls without synchronization.

`RE_BUDGET` means the step limit was reached. `RE_SPACE` means the task's matcher workspace was
too small; neither condition corrupts the task stack.
