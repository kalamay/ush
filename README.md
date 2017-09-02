# ush

Micro shell command pipeline runner

ush executes a sequence of commands similarly to the shell. Each process
is forked and executed and connected using pipes. Unlike a shell, it does
nothing else. This makes ush appropriate for uses where running a shell
is challenging or when security is critical.

## Examples

Forward log messages that match an expression in grep.

```
ush -o /dev/tcp/localhost/9999 nc -kl localhost | grep ERROR
```

Getting the top 5 memory consumer across a few hosts. Proper monitoring tools
are better here, but this is just an example. But sometimes you just want to
collect some quick stats.

```
# add host names to hosts.txt
dsh -Mcf hosts.txt ush ps aux --sort -rss \| head -n 5
```

Writing a file to each host. Typically you would want to do this with a
configuration management tool, but sometime that's overkill.

```
# add host names to hosts.txt
dsh -Mcf hosts.txt ush -o somefile.txt echo "some stuff"
```

## Building
