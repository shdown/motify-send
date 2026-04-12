**motify-send** is *notify-send* with memory.

Like *notify-send*, it can send notifications to a notification daemon.
However, unlike *notify-send*, it is able to re-use previous notification with given
"application name". Thus it can be used to dynamically change the content of a notification.

Consider, for example, notifications about a change of screen brightness:
if the user adjusted the brightness in 5 steps, we don't want to send 5 notifications; instead, we
want to show a single notification with its content dynamically updated.

It also can close existing notifications.

It is also security-aware, hardened against TOCTOU attacks.

Requirements
===

* Any version of GCC or Clang;
* CMake 3.0+;
* GLib 2.0 and GIO 2.0 development libraries;
* pkg-config.

Building and installing
===

```bash
mkdir build && cd build
cmake ..
make
sudo make install
```

Usage
===

Send a notification
---

```bash
motify-send APPNAME SUMMARY BODY
```

It will reuse the latest notification belonging to the given `APPNAME`.

Example:
```bash
motify-send myapp "Build Complete" "Your project compiled successfully."
```

Send with options
---

```bash
motify-send -i /path/to/icon.png -u critical -t 10 myapp "Warning" "Disk space low"
```

Update an existing notification
---

By default, subsequent calls with the same `APPNAME` will replace the previous notification. Use `-n` to force a new one:

```bash
motify-send -n myapp "New Notification" "This won't replace the old one"
```

Close a notification
---

```bash
motify-send -c APPNAME
```

Example:
```bash
motify-send -c myapp
```

Options
===

| Option | Description |
|--------|-------------|
| `-i PATH` | Path to the icon file |
| `-u URGENCY` | Set urgency: `0`/`low`, `1`/`normal`, or `2`/`critical` |
| `-t SECONDS` | Set timeout in seconds (negative = no timeout) |
| `-n` | Force creation of a new notification (never replace) |
| `-c` | Close existing notification for the given APPNAME |
| `-H KEY=TYPE:VALUE` | Add a custom hint |
| `-h` | Show usage information |

Custom Hints
===

Hints allow you to attach typed metadata to notifications using the `-H KEY=TYPE:VALUE` syntax.

Supported hint types
---

| Type | Format | Example |
|------|--------|---------|
| `bool` | `true`/`false` or `0`/`1` | `-H suppress-sound=bool:true` |
| `byte` | empty, single char, or `#XX` hex | `-H hint-name=byte:#01` |
| `double` | floating-point number | `-H hint-name=double:3.14` |
| `str` | string | `-H category=str:system` |
| `i16`, `u16`, `i32`, `u32`, `i64`, `u64` | integer | `-H x=i32:42` |

For type `byte`, empty string means NUL byte (`\0`).

Application Name Rules
===

The `APPNAME` must:
- Not be empty;
- Contain only alphanumeric characters, underscores (`_`), or hyphens (`-`);
- Not start with `-`.

How It Works
===

motify-send persists notification IDs per application name using a storage file in `/tmp`.

License
===

MIT. See [LICENSE.MIT](LICENSE.MIT) for details.
