# ush

ush executes a sequence of commands similarly to the shell. Each process
is forked and executed and connected using pipes. Unlike a shell, it does
nothing else. This makes ush appropriate for uses where running a shell
is challenging or when security is critical.

## Examples

Forward log messages that match an expression in grep.

```bash
ush -o /dev/tcp/localhost/9999 nc -kl localhost \| grep ERROR
```

```bash
# add host names to hosts.txt
dsh -Mcf hosts.txt ush -o somefile.txt echo "some stuff"
```

## Building and Installing

Should be just a matter of:

```bash
make # builds ush in source directory
make install # installs ush binary and man page
```

Setting the `PREFIX` variable will change the build install location. By
default, this value is `/usr/local`:

```bash
make clean
make PREFIX=/opt/bin
make PREFIX=/opt/bin install
```
