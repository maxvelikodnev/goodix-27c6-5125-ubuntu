#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Run ONLY in a disposable Ubuntu container, without host devices or systemd.
set -eu
test -f /.dockerenv || { echo 'Disposable Docker container required.' >&2; exit 1; }
test ! -d /run/systemd/system || { echo 'Refusing a real systemd host.' >&2; exit 1; }
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y --no-install-recommends "$1"
test "$(goodix-ubuntu status)" = disabled
test ! -e /etc/systemd/system/fprintd.service.d/92-goodix-ubuntu-package.conf
# Replace systemctl only inside this disposable container. No daemon/USB runs.
cat > /usr/bin/systemctl <<'MOCK'
#!/bin/sh
echo "$*" >> /tmp/mock-systemctl.log
case "$1" in
 show)
  case "$4" in
   FragmentPath) echo /usr/lib/systemd/system/fprintd.service ;;
   DropInPaths) test ! -e /etc/systemd/system/fprintd.service.d/92-goodix-ubuntu-package.conf || echo /etc/systemd/system/fprintd.service.d/92-goodix-ubuntu-package.conf ;;
   Environment) test ! -e /etc/systemd/system/fprintd.service.d/92-goodix-ubuntu-package.conf || echo LD_LIBRARY_PATH=/usr/lib/goodix-5125-ubuntu ;;
  esac
 ;;
 is-active) echo active ;;
esac
exit 0
MOCK
chmod 755 /usr/bin/systemctl
mkdir -p /run/systemd/system /var/lib/fprint/goodix-5125-state-v2
chmod 700 /var/lib/fprint/goodix-5125-state-v2
echo synthetic > /var/lib/fprint/goodix-5125-state-v2/preserve
goodix-ubuntu enable
test "$(goodix-ubuntu status)" = enabled
if dpkg -i "$1" >/tmp/enabled-upgrade.log 2>&1; then
 echo 'Enabled upgrade unexpectedly succeeded' >&2
 exit 1
fi
grep -q 'disable before' /tmp/enabled-upgrade.log
test "$(dpkg-query -W -f='${db:Status-Status}' goodix-5125-ubuntu)" = installed
goodix-ubuntu disable
dpkg -i "$1"
goodix-ubuntu enable
apt-get remove -y goodix-5125-ubuntu
test ! -e /etc/systemd/system/fprintd.service.d/92-goodix-ubuntu-package.conf
test -f /var/lib/fprint/goodix-5125-state-v2/preserve
test -f /usr/libexec/fprintd
dpkg --purge goodix-5125-ubuntu
test -f /var/lib/fprint/goodix-5125-state-v2/preserve
echo PACKAGE_LIFECYCLE_SMOKE=PASS
