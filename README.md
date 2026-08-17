# libmcp2221a_hid

C++ host API for the Microchip **MCP2221A** over **USB HID** (64-byte reports), using **libusb-1.0** interrupt transfers. Protocol: **DS20005565D**, Section 3.0. The public API uses **C++17** with **`ConstByteSpan`** / **`MutableByteSpan`** (non-owning views) instead of raw pointer/length pairs for buffers.

## License

This project is licensed under the **GNU Lesser General Public License v3.0** (LGPL-3.0).

For the full license text, see: https://www.gnu.org/licenses/lgpl-3.0.txt

## Files

| File | Role |
|------|------|
| `mcp2221a_hid_types.hpp` | Command codes, I²C/GPIO types, `StatusSnapshot`, response views, `I2cRetryPolicy`, presets |
| `mcp2221a_hid.hpp` | `mcp2221a::hid::Device` |
| `mcp2221a_hid.cpp` | Implementation |

Public includes (CMake `target_include_directories` → `src/`):

```cpp
#include <mcp2221a/hid/mcp2221a_hid.hpp>
```

Link **libusb-1.0**. On Linux, udev rules or permissions may be required for the USB device.

### Solo build (this repo)

```bat
Build.bat fetch
Build.bat gen
```

- **fetch** downloads prebuilt libusb (MSVC) via `tools/Fetchlibusb_deps.bat` into `deps/`.
- **gen** configures and builds for `BUILD_TARGET` (default `msvc16-x86_64`), then installs to **`dist/<BUILD_TARGET>/`** (same layout as Sample):

```text
dist/<BUILD_TARGET>/
  lib/libmcp2221a_hid.lib   # or .a
  include/mcp2221a/hid/*.hpp
  libusb-1.0.dll            # Windows runtime, at dist root (like Sample)
```

- Nested consumers declare this tree in their `Description.xml` (`source` type `fs` or `git`); libusb is declared in **this** repo’s `Description.xml` and fetched recursively.

Optional dist override when nested: `MCP2221A_HID_DIST_ROOT` (fallback: `FOUNDATION_DIST_ROOT`).

---

## Naming

Aligned with Foundation-style conventions:

| Kind | Convention | Examples |
|------|------------|----------|
| **Constants** | PascalCase — no `k` prefix | `ReportSize`, `HidReportTypeOutput`, `ResetKeyByte1` |
| **Enum class** | PascalCase enumerators | `Command::I2cWriteData`, `Error::UsbTransfer` |
| **Class / struct members** | `_` prefix + camelCase | `Device::_handle`, `StatusSnapshot::_adcCounts` |
| **Private methods** | `_` prefix + camelCase | `_sendReport`, `_fillI2cSegment` |
| **File-local helpers** | camelCase (anonymous namespace) | `discoverHidInterruptEndpoints`, `writeLe16` |
| **Report index namespaces** | camelCase namespace, PascalCase constants | `statusResponseIndex::CommandEcho` |

---

## Build / usage

```cpp
#include <mcp2221a/hid/mcp2221a_hid.hpp>

using namespace mcp2221a::hid;
```

Default USB IDs: `DefaultVendorId` / `DefaultProductId` (factory MCP2221A; may differ if reprogrammed in Flash).

### I²C clock formula (MCP2221A, DS20005565D Table 3-1 Note 1)

The divider byte `D` at report index 4 (when applying a new speed) satisfies:

- `D = (12 MHz / f_SCL) − 2`  (target `f_SCL` in Hz)
- `f_SCL = 12 MHz / (D + 2)`  (inverse)

Integer division is used in code; verify with `i2cNominalClockHz()` if needed. For an arbitrary nominal rate in **kbit/s**, use **`i2cClockDividerForDataRateKbs`** (or **`I2cClockDivider::fromNominalKbs`**) instead of converting to Hz first. With one byte for `D` (0…255), achievable `f_SCL` is roughly **~47 kHz … 6 MHz**; very low targets (e.g. 10 kbit/s) would need `D > 255` and are not provided as named presets.

### I²C-bus specification (mode classes)

Per the I²C specification (e.g. NXP UM10204):

**Bidirectional bus**

- **Standard-mode (Sm):** bit rate up to **100 kbit/s**
- **Fast-mode (Fm):** bit rate up to **400 kbit/s**
- **Fast-mode Plus (Fm+):** bit rate up to **1 Mbit/s**
- **High-speed mode (Hs-mode):** bit rate up to **3.4 Mbit/s**

**Unidirectional bus**

- **Ultra Fast-mode (UFm):** bit rate up to **5 Mbit/s**

Each **`I2cSpeedPreset`** enumerator **is** the divider byte `D` (see `nominalKbs(p)` for the nominal kbit/s label). For other speeds use **`i2cClockDividerForDataRateKbs(kbs)`** (`f_SCL = 12 MHz / (D + 2)`). The **MCP2221A** data sheet still limits the practical I²C host to about **400 kbit/s**; higher targets follow the same formula for API alignment.

---

## 1. Discovery / open / close

Use **`countDevices`** first if you need to know how many boards match VID/PID. Then **`openByIndex` / `openBySN` / `close`**, matching libusb’s “open a handle” model. (Older samples sometimes said “connect”; that was only naming.)

```cpp
Device dev;
if (!dev.openByIndex()) {
    // check dev.lastError()
    return;
}
// ...
dev.close();
```

- **`countDevices(vid, pid)`** — how many matching devices are present.
- **`openByIndex(vid, pid, deviceIndex)`** — Nth device with that VID/PID (default index `0`).
- **`openBySN(vid, pid, serialNo)`** — device whose USB serial string equals `serialNo` (exact match).

---

## 2. I²C bus speed (STATUS / SET Parameters, Table 3-1)

Set the clock **before** heavy I²C traffic. Example **400 kbit/s**:

```cpp
const I2cClockDivider div{I2cSpeedPreset::FastMode_400kbs};

dev.statusSetParameters(
    CancelI2cTransfer::NoEffect,
    SetI2cSpeedTag::ApplyDivider,
    div);
```

Presets use names like `LowSpeed_50kbs`, `StandardMode_100kbs`, `FastMode_400kbs`, etc. The MCP2221A data sheet typically documents the I²C host up to **~400 kbit/s**; use higher presets only if your hardware and tests allow it.

Alternatively build from the preset with **`I2cClockDivider::fromI2cSpeedPreset(I2cSpeedPreset::FastMode_400kbs)`** (same as **`I2cClockDivider{I2cSpeedPreset::…}`**) — nominal kbit/s for display is **`nominalKbs(p)`**.

Check realized frequency: `i2cNominalClockHz(div)`.

---

## 3. Status snapshot vs `statusSetParameters`

**`getStatusSnapshot(snap)`** (optionally **`getStatusSnapshot(snap, rawReport)`** for the 64-byte IN report) is a shortcut for:

```cpp
statusSetParameters(
    CancelI2cTransfer::NoEffect,
    SetI2cSpeedTag::NoEffect,
    I2cClockDivider{});
```

followed by parsing the IN report into **`StatusSnapshot`** (I²C progress, SCL/SDA, HW/FW revision characters, three **ADC** 16-bit counts, etc.).

Use **`statusSetParameters`** directly when you need to:

- **Cancel** an ongoing I²C transfer (`CancelI2cTransfer::Cancel`),
- **Apply** a new I²C divider (`SetI2cSpeedTag::ApplyDivider` + `I2cClockDivider`).

---

## 4. I²C addressing (Table 3-20 Note 2)

8-bit address: **even** = write, **odd** = read.

```cpp
auto w = I2cClientAddress::forWrite7bit(0x48);  // 7-bit slave 0x48, write
auto r = I2cClientAddress::forRead7bit(0x48);   // same slave, read
```

---

## 5. I²C write

```cpp
std::array<std::uint8_t, 2> data{0x00, 0x12};
dev.i2cWrite(w, data); // ConstByteSpan (implicit from std::array / std::vector)
```

Long payloads are split into 60-byte HID chunks automatically.

Variants:

- **`i2cWriteRepeatedStart`** — Repeated START on the bus (Table 3-22).
- **`i2cWriteNoStop`** — No STOP after write (Table 3-24); use before a read with repeated START.

---

## 6. I²C read (two HID steps)

1. **`i2cRead`** / **`i2cReadRepeatedStart`** — start the read on the wire.  
2. **`i2cGetData`** — fetch bytes from the device buffer (Table 3-30 / 3-31).

Or use the combined helper:

```cpp
std::array<std::uint8_t, 4> buf{}; // fixed buffer: size must be ≥ read length
dev.i2cReadComplete(r, static_cast<std::uint16_t>(buf.size()), buf);
```

Or let the library resize a **`std::vector`** to the read length before transferring:

```cpp
std::vector<std::uint8_t> buf;
dev.i2cReadComplete(r, 4, buf); // buf is resized to 4 bytes
```

If the engine often returns **busy (0x01)**, use the overload with **`I2cRetryPolicy`**:

```cpp
const I2cClockDivider div{I2cSpeedPreset::FastMode_400kbs};
const I2cRetryPolicy policy = I2cRetryPolicy::forI2cRead(div, static_cast<std::uint16_t>(buf.size()));

dev.i2cReadComplete(r, static_cast<std::uint16_t>(buf.size()), buf, policy);
```

---

## 7. Register read (write pointer, then read data) — SMBus-style

Typical pattern: **write without STOP**, **read with repeated START**, then **get data** (possibly multiple times). The library wraps this:

```cpp
std::array<std::uint8_t, 1> reg{0x00};
std::array<std::uint8_t, 2> out{};
std::size_t n = 0;

const I2cClockDivider div{I2cSpeedPreset::StandardMode_100kbs};

dev.i2cWriteThenRead(w, reg, r, 2, out, n, div);
```

The overload **with** `I2cClockDivider` applies a retry policy on the **read** phase derived from that divider and length. The overload **without** uses a single attempt on the read HID command.

---

## 8. GPIO (Tables 3-32, 3-34)

```cpp
std::array<GpioPinState, 4> pins{};
dev.getGpioValues(pins);

// set outputs: fill std::array<GpioPinCommandBlock, 4> per Table 3-32
dev.setGpioOutputs(command_blocks);
```

---

## 9. Flash / password (Tables 3-3 … 3-18)

- **`readFlash` / `writeFlash`** — sub‑codes in `ReadFlashSubcode` / `WriteFlashSubcode`; payload layout depends on the sub‑command (see data sheet).  
- **`sendFlashPassword`** — 8-byte password when Flash is password-protected.

---

## 10. SRAM settings (Table 3-36 / 3-38)

Run-time chip/GP settings (DAC, references, clock on GP, GP designation, etc.): **`setSramSettings`** (bytes 2–11 of the report) and **`getSramSettings`** (raw 64-byte report for parsing).

---

## 11. Reset (Table 3-40)

```cpp
dev.resetChip();  // OUT only; device may re-enumerate
```

---

## 12. Typed responses (`mcp2221a_hid_types.hpp`)

- **`statusSnapshotFromReport(raw)`** — fill `StatusSnapshot` from a 64-byte IN buffer.  
- **`StatusResponseView`**, **`I2cEngineResponseView`**, **`I2cGetDataResponseView`** — non-owning accessors over `const RawReport&`.

---

## 13. Errors

On failure, **`Device::lastError()`** returns `Error::` values (`UsbTransfer`, `ProtocolMismatch`, `IncompleteRead`, etc.). Use **`clearLastError()`** if you want to reset the stored code.

---

## Reference

Microchip **MCP2221A** data sheet: **DS20005565D**, Section 3.0 *USB HID Communication*.
