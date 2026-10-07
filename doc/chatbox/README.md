# ChatBox.nu notes

This fork is Undernet's GNUworld with the ChatBox.nu changes on branch `chatbox`:

- Nick protection, ported from Seven's gnuworld-enhanced: `SET NICKNAME`, `SET AUTONICK`, `SCAN NICK`
  (see the commit message for details). Settings `nick_protection` and `nick_protection_maxlen` in
  `cservice.conf`.
- The nick protection needs an ircd that lets SVSNICK past its nick flood limit. ChatBox.nu runs
  `mbino/nefarious2`, which has that patch (`s_user.c`).
- Channel applications on IRC, ported from gnuworld-enhanced: `REGISTER <#channel>` steps (REALNAME,
  DESCRIPTION, SUPPORTERS) when `required_supporters` is above 0, and `ACCEPT`, `REJECT`, `CANCEL`,
  `OBJECT`. With `required_supporters = 0`, `REGISTER <#channel>` registers right away. Settings
  `min_days_before_reg`, `min_days_before_support` and `max_concurrent_supports` in `cservice.conf`.
- No IP restrictions required: `IPR_DEFAULT_REJECT` is off in `mod.cservice/cservice_config.h`, so
  admins without IP restriction entries can log in from any address.

## Build on AlmaLinux 9

GNUworld needs C++20; GCC 11 from AlmaLinux 9 fails on it, so build with gcc-toolset-13. The result
runs with the system's libstdc++, the toolset is only needed to build.

```
dnf install gcc-toolset-13-gcc-c++ autoconf automake libtool libtool-ltdl-devel make \
    boost-devel pcre-devel pcre2-devel libcurl-devel openssl-devel libpq-devel liboath-devel
./autogen.sh
CC=/opt/rh/gcc-toolset-13/root/usr/bin/gcc CXX=/opt/rh/gcc-toolset-13/root/usr/bin/g++ \
    ./configure --prefix=$HOME/gnuworld --with-pgconfig=/usr/bin/pg_config --with-libssl-lib=/usr/lib64 \
    --enable-modules=cservice,ccontrol,dronescan,openchanfix
make && make install
```

## Moving from gnuworld-enhanced

1. Back up all five databases.
2. `psql -h 127.0.0.1 -U gnuworld -d cservice -X -v ON_ERROR_STOP=1 -f doc/chatbox/migrate-from-gnuworld-enhanced.sql`
   (cservice, local_db and ccontrol). It checks that the tables whose layout changes are empty.
3. Start GNUworld: the modules' own migrations (`migrations/<module>`, e.g. 16 for dronescan and 5 for
   openchanfix) are applied automatically on startup.

## Configuration

Start from the example files in `bin/` and carry the old values over. Points that differ from
gnuworld-enhanced:

- Every key the modules read is required; a missing key stops GNUworld at startup.
- Values are parsed strictly: `10;` is an error, write `10`.
- `GNUWorld.example.conf` has the `module =` lines commented out and `tls = yes` for the uplink. Add
  the module lines; set `tls = no` unless the ircd's services link uses TLS.
- `openchanfix.example.conf` here has the `daysamples`, `bonusPointsPerDay` and
  `bonusMaxDaysBeforeDecay` keys that the module requires (Undernet's copy only has them in
  `mod.openchanfix/chanfix.example.conf.in`).
