<div align="center">

# 🔊 VoX DoN 3 🔊

<img src="assets/icon0.png" alt="VoX DoN 3 — app icon" width="460"/>

### `VoX DoN 3.elf` — PS5 all-in-one payload (Host + Installer in one ELF)

---

</div>

## 🎯 What is this?

**VoX DoN 3 Host** is a PS5 payload that fuses **two jobs into a single ELF**:

- **Host** — a self-contained local web host (HTTPS on `127.0.0.1:18181`) built right into the payload. Your PS5 becomes the host itself — no RPi, no middle PC.
- **Installer** — installs the **VoX DoN 3** app (icon above) onto your home screen with the App Metadata (AppCache + AppHost) ready for instant launch.

> This build ships only the payloads that are actually part of the weaponized flow:
> `elfldr` + `kexp` + `pldmgr` (the Payload Manager). `kstuff` and `nanodns`
> are **not** bundled — everything needed runs from the single fused ELF.

## 🛠️ Build

Requirements: **FreeBSD x86-64** + `ps5-payload-sdk` (the standard PS5 payload toolchain).

```sh
export PS5_PAYLOAD_SDK=/path/to/ps5-payload-sdk
make       # produces VoX DoN 3.elf
```

This rebuilds the same ELF shipped in the Release, from source.

## 🚀 Usage on PS5

1. Copy `VoX DoN 3.elf` into your payload folder (e.g. `payloads/`).
2. Load it from your PS5 exploit flow as usual — it starts the local host and installs the **VoX DoN 3** app in one go.

Done — find **VoX DoN 3** (with this icon) on your home screen.

## 📦 Contents

```
VoX DoN 3.elf                  ← the all-in-one fused payload (host + installer + everything)
assets/
  icon0.png                    ← the app icon (also shown at the top of this page)
  param.json                   ← PS5 app metadata (title name, app host URL, entitlement)
frontend/
  installer-page/              ← the landing page used right after install
  pointer/                     ← the app pointer page that redirects into the cached app dir
host/                          ← the web content served on 127.0.0.1:18181 (cached via AppCache)
  slopkit/                     ← the exploit UI (poops.html / poops.js, rop/syscall engine)
  payloads/                    ← the payloads: elfldr, kexp, pldmgr_v0.5.0
src/ , include/                ← full C source (HTTP server, inflate, installer, launcher, notification)
tools/ , Makefile              ← build tools that regenerate version + file registry
README.md                      ← this page
```

## 📦 Releases

Latest ELF (ready to copy & run):

```
VoX DoN 3.elf
```

Available from the [Releases](https://github.com/VoXDon3/VoX-DoN-3-Host-Installer.elf/releases) tab.

---

<div align="center">

*VoX DoN 3 — host + installer, all in one.* 🎮

</div>