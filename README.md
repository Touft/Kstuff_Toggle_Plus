# Kstuff Toggle Plus

PS5 payload to automatically manage kstuff when launching/closing homebrew games.

---

## 📋 Overview

The **Kstuff Toggle Plus** payload (version 1.0) is a daemon that automatically detects homebrew game launch and exit (PPSA for PS5, CUSA for PS4), shows notifications with countdown, and **automatically disables/enables kstuff** by writing directly to the kernel `sysentvec` (both PS5 and PS4 sysentvec at the same time).

---

## 🎮 Features

- Automatic game detection via `SceSysCore.elf` + `kqueue`
- Console type detection: **PS5 (PPSA)** or **PS4 (CUSA)**
- Startup notification: `Kstuff Toggle Plus V1.0`
- Launch notification:  
  `PS5 Game detected PPSAxxxxx pausing Kstuff in 10s`
- Silent countdown
- At countdown end: disables kstuff (`0xffff` on both sysentvec)
- Exit notification:  
  `PPSAxxxxx closed, resuming Kstuff in 3s`
- At countdown end: enables kstuff (`0xdeb7` on both sysentvec)
- Debug notification showing value read before write: `was 0xdeb7` / `was 0xffff`

---

## ⚙️ Configuration

Create the file `/data/kstuff_toggle_plus/autotoggle.ini` on your PS5:

```ini
PS5_auto_disable_timer=10
PS5_auto_enable_timer=3
PS4_auto_disable_timer=15
PS4_auto_enable_timer=5
```

| Key | Description | Default |
|-----|-------------|---------|
| `PS5_auto_disable_timer` | Seconds to wait before disabling kstuff on PS5 game launch | 10 |
| `PS5_auto_enable_timer` | Seconds to wait before enabling kstuff on PS5 game exit | 3 |
| `PS4_auto_disable_timer` | Seconds to wait before disabling kstuff on PS4 game launch | 15 |
| `PS4_auto_enable_timer` | Seconds to wait before enabling kstuff on PS4 game exit | 5 |

If the file is missing, default values are used (10 seconds and 3 seconds)

---

## 📦 Available Payloads

| File | Role |
|------|------|
| `Kstuff_Toggle_Plus.elf` | Main daemon (game detection + auto kstuff on/off) |
| `Kstuff_Toggle_Plus-unload.elf` | Unloads the daemon from memory (avoids console reboot) |

---

## 🚀 Usage

1. Transfer `Kstuff_Toggle_Plus.elf` to your PS5 (via FTP, USB, or Homebrew Launcher)
2. Launch the payload → notification `Kstuff Toggle Plus V1.0` appears
3. Launch a homebrew game → you get a notification on launch, then kstuff is disabled after the timer
4. Quit the game → you get a notification, then kstuff is re-enabled after the timer
5. To stop the daemon without rebooting the console, launch `Kstuff_Toggle_Plus-unload.elf`

---

## 🔧 Supported Firmware Offsets

The payload contains `sysentvec` offsets for PS5 firmwares from 3.00 to 13.60.  
The **13.60 (PS5 Pro)** offset has been empirically validated: `KERNEL_ADDRESS_DATA_BASE + 0x0e6e5c0`.

---

## ⚠️ Warning

This payload directly modifies PS5 kernel structures (`sysentvec`).  
Usage may cause kernel panics or system instability.  
Use at your own risk.

---
