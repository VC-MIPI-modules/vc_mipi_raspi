# Installing a newer v4l2-utils on Raspberry Pi OS Bookworm

Raspberry Pi OS Bookworm ships with **v4l2-utils 1.22.1**, but the newer **1.30.1** is available in the Raspbian Trixie repository. This guide shows how to upgrade using APT pinning so only `v4l-utils` and its direct dependencies are pulled from Trixie — the rest of the system stays on Bookworm.

---

## 1. Add the Raspbian Trixie repository

Create a new sources file that adds Trixie at **low priority** (pinned to 100, below the default 500 for Bookworm):

```bash
echo "deb [arch=armhf] http://raspbian.raspberrypi.com/raspbian/ trixie main contrib non-free rpi" \
  | sudo tee /etc/apt/sources.list.d/raspbian-trixie.list
```

## 2. Pin Trixie to low priority

This ensures `apt upgrade` will never automatically pull packages from Trixie:

```bash
sudo tee /etc/apt/preferences.d/trixie-pin <<'EOF'
Package: *
Pin: release n=trixie
Pin-Priority: 100
EOF
```

## 3. Update the package lists

```bash
sudo apt-get update
```

Verify both versions are visible:

```bash
apt-cache policy v4l-utils
```

Expected output:
```
v4l-utils:
  Installed: 1.22.1-5
  Candidate: 1.22.1-5
  Version table:
     1.30.1-1 100
        100 http://raspbian.raspberrypi.com/raspbian trixie/main armhf Packages
 *** 1.22.1-5 500
        500 http://raspbian.raspberrypi.com/raspbian bookworm/main armhf Packages
```

## 4. Install v4l-utils from Trixie

Use `-t trixie` to explicitly target the Trixie version:

```bash
sudo apt-get install -t trixie v4l-utils
```

## 5. Verify

```bash
v4l2-ctl --version
media-ctl --version
```

Expected output:
```
v4l2-ctl 1.30.1
media-ctl 1.30.1
```

---

## Notes

- The pin priority of **100** means Trixie packages will **never** be selected automatically by `apt upgrade`. Only explicitly targeted installs with `-t trixie` will use them.
- The Trixie `v4l-utils` package pulls in updated `libv4l-*` libraries, which are co-installed alongside the Bookworm system libraries without conflict.
- If you later want to revert to the Bookworm version:
  ```bash
  sudo apt-get install v4l-utils=1.22.1-5
  ```
- To remove the Trixie repository entirely:
  ```bash
  sudo rm /etc/apt/sources.list.d/raspbian-trixie.list
  sudo rm /etc/apt/preferences.d/trixie-pin
  sudo apt-get update
  ```
