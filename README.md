# Logcie

Sinlge header-only (in development) Logging library in C

**NOTE** This library is in development and VERY unfinished. You can add "for now" to literally anything in this readme

# Example

```c
int main(void) {
  Logcie_Sink sink = {
    .level = LOGCIE_LEVEL_DEBUG,
    .sink = stdout,
    .fmt = "$f:$x: $c$L$r: $d $t (GMT $z) $c[$l]$r: $m $$stdout"
  };

  LOGCIE_DEBUG(sink, "debugguy loggy %d %s", 1, "adsf");
  LOGCIE_VERBOSE(sink, "verbosesy loggy %d", 2);
  LOGCIE_INFO(sink, "infofofo loggy %d", 3);
  LOGCIE_WARN(sink, "warnny loggy %d", 4);
  LOGCIE_ERROR(sink, "errorry loggy %d", 5);
  LOGCIE_FATAL(sink, "fatallyly loggy %d", 6);

  return 0;
}
```

```console
./test/test.c:195: DEBUG: 1970-01-01 11:00:00 (GMT +11) [debug]: debugguy loggy 1 adsf $stdout
./test/test.c:196: VERBOSE: 1970-01-01 11:00:00 (GMT +11) [verbose]: verbosesy loggy 2 $stdout
./test/test.c:197: INFO: 1970-01-01 11:00:00 (GMT +11) [info]: infofofo loggy 3 $stdout
./test/test.c:198: WARN: 1970-01-01 11:00:00 (GMT +11) [warn]: warnny loggy 4 $stdout
./test/test.c:199: ERROR: 1970-01-01 11:00:00 (GMT +11) [error]: errorry loggy 5 $stdout
./test/test.c:200: FATAL: 1970-01-01 11:00:00 (GMT +11) [fatal]: fatallyly loggy 6 $stdout
```

## Log format

 - $$ - literally symbol $
 - $m - log message
 - $f - file
 - $x - line
 - $l - log level (info, debug, warn)
 - $L - log level in upper case (INFO, DEBUG, WARN)
 - $d - current date (YYYY-MM-DD)
 - $t - current time (H:i:m)
 - $z - current time zone
 - $c - color marker of log level
 - $r - color reset

## Notes
 - If you use clang and flags `-std=c23 -pedantic` you will get warning  if you don't supply any variadic arguments.
   To suppress this warning you can use `#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"`.
