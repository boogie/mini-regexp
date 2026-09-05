# Precompiled pattern

Parses `key=value` configuration lines with product A: the pattern is compiled on the
workstation and only its bytecode ships.

```sh
node tools/re_compile.js '^(\w+)=(\d+)$' setting_re > examples/settings/setting_re.h

cc -std=c99 -Isrc -Iexamples/settings -o /tmp/settings \
   examples/settings/main.c src/re.c && /tmp/settings
```

```
timeout=30   key=timeout value=30
retries=5    key=retries value=5
name=abc     no match (0)
x=1          key=x value=1
```

Cost on a 32-bit target: 60 bytes of flash for the bytecode, 24 bytes of RAM for the
captures, 512 bytes for the workspace at `DEPTH = 16`. Nothing is allocated.

`RE_BUDGET` means the step budget ran out — raise `STEPS`. `RE_SPACE` means the workspace
did — raise `DEPTH`. Neither can produce a wrong answer, so both are safe to tune downwards
until you see the code appear.
