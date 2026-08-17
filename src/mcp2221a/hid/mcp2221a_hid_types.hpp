/**
 * MCP2221A USB HID protocol types (DS20005565D, Section 3.0 "USB HID Communication").
 * All command codes and report indices match the Microchip data sheet; the public API
 * avoids raw numeric literals in favor of these named types.
 *
 * Copyright(c) 2025 Pierre Quelin <pierre.quelin.1972@gmail.com>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * For the full license text, see: https://www.gnu.org/licenses/lgpl-3.0.txt
 */

#ifndef MCP2221A_HID_TYPES_HPP
#define MCP2221A_HID_TYPES_HPP

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace mcp2221a
{
namespace hid
{

/** HID interrupt reports are fixed at 64 bytes (data sheet Section 3.0). */
inline constexpr std::size_t ReportSize = 64U;

/**
 * Raw wire-format HID payload (64 bytes).
 *
 * Ways to interpret an IN report without scattering magic indices in application code:
 * 1) Parse to structs: statusSnapshotFromReport(raw), or GPIO helpers in Device.
 * 2) Non-owning views: StatusResponseView, I2cEngineResponseView, I2cGetDataResponseView (wrap const RawReport&).
 * 3) Phantom tagging (optional pattern): store RawReport and pass to the view/parser that matches the last command.
 */
using RawReport = std::array<std::uint8_t, ReportSize>;

/**
 * Non-owning byte view (C++17 replacement for std::span in this library).
 * Construct from `std::array` / `std::vector` or use `subspan` on another view.
 */
class ConstByteSpan
{
public:
    constexpr ConstByteSpan() noexcept : _data(nullptr), _size(0) {}

    constexpr ConstByteSpan(const std::uint8_t* data, std::size_t size) noexcept : _data(data), _size(size) {}

    template<std::size_t N>
    constexpr ConstByteSpan(const std::array<std::uint8_t, N>& a) noexcept : _data(a.data()), _size(N) {}

    ConstByteSpan(const std::vector<std::uint8_t>& v) noexcept
        : _data(v.empty() ? nullptr : v.data()), _size(v.size())
    {
    }

    constexpr std::size_t size() const noexcept { return _size; }
    constexpr bool        empty() const noexcept { return _size == 0; }

    constexpr const std::uint8_t* data() const noexcept { return _data; }

    ConstByteSpan subspan(std::size_t offset) const
    {
        if (offset > _size)
        {
            return {};
        }
        return ConstByteSpan(_data + offset, _size - offset);
    }

    ConstByteSpan subspan(std::size_t offset, std::size_t count) const
    {
        if (offset > _size)
        {
            return {};
        }
        return ConstByteSpan(_data + offset, std::min(count, _size - offset));
    }

private:
    const std::uint8_t* _data;
    std::size_t         _size;
};

/** Writable non-owning byte view (C++17). */
class MutableByteSpan
{
public:
    constexpr MutableByteSpan() noexcept : _data(nullptr), _size(0) {}

    constexpr MutableByteSpan(std::uint8_t* data, std::size_t size) noexcept : _data(data), _size(size) {}

    template<std::size_t N>
    constexpr MutableByteSpan(std::array<std::uint8_t, N>& a) noexcept : _data(a.data()), _size(N) {}

    MutableByteSpan(std::vector<std::uint8_t>& v) noexcept
        : _data(v.empty() ? nullptr : v.data()), _size(v.size())
    {
    }

    constexpr std::size_t size() const noexcept { return _size; }
    constexpr bool        empty() const noexcept { return _size == 0; }

    constexpr std::uint8_t*       data() noexcept { return _data; }
    constexpr const std::uint8_t*   data() const noexcept { return _data; }

    MutableByteSpan subspan(std::size_t offset) const
    {
        if (offset > _size)
        {
            return {};
        }
        return MutableByteSpan(_data + offset, _size - offset);
    }

private:
    std::uint8_t* _data;
    std::size_t   _size;
};

/** Default USB identifiers for MCP2221A (factory; may be reprogrammed in Flash). */
inline constexpr std::uint16_t DefaultVendorId  = 0x04D8U;
inline constexpr std::uint16_t DefaultProductId = 0x00DDU;

// ---------------------------------------------------------------------------
// HID command codes (Table 3-x in DS20005565D)
// ---------------------------------------------------------------------------

enum class Command : std::uint8_t
{
    StatusSetParameters   = 0x10, ///< Table 3-1 Status/Set Parameters
    I2cGetData            = 0x40, ///< Table 3-30 I2C Read Data – Get I2C Data
    SetGpioOutputs        = 0x50, ///< Table 3-32 Set GPIO Output Values
    GetGpioValues         = 0x51, ///< Table 3-34 Get GPIO Values
    SetSramSettings       = 0x60, ///< Table 3-36 Set SRAM settings
    GetSramSettings       = 0x61, ///< Table 3-38 Get SRAM Settings
    ReadFlashData         = 0xB0, ///< Table 3-3 Read Flash Data
    WriteFlashData        = 0xB1, ///< Table 3-11 Write Flash Data
    SendFlashPassword     = 0xB2, ///< Table 3-18 Send Flash Access Password
    I2cWriteData          = 0x90, ///< Table 3-20 I2C Write Data
    I2cReadData           = 0x91, ///< Table 3-26 I2C Read Data
    I2cWriteRepeatedStart = 0x92, ///< Table 3-22 I2C Write Data Repeated-START
    I2cReadRepeatedStart  = 0x93, ///< Table 3-28 I2C Read Data Repeated-START
    I2cWriteNoStop        = 0x94, ///< Table 3-24 I2C Write Data No STOP
    ResetChip             = 0x70, ///< Table 3-40 Reset Chip
};

// ---------------------------------------------------------------------------
// Status/Set Parameters sub-fields (Table 3-1)
// ---------------------------------------------------------------------------

/** Byte index 2: cancel current I2C/SMBus transfer (Table 3-1). */
enum class CancelI2cTransfer : std::uint8_t
{
    NoEffect = 0x00,
    /** Device will cancel the current transfer and attempt to free the bus. */
    Cancel = 0x10,
};

/** Byte index 3: set I2C/SMBus speed (Table 3-1). */
enum class SetI2cSpeedTag : std::uint8_t
{
    NoEffect = 0x00,
    /** Byte index 4 is interpreted as the I2C clock divider. */
    ApplyDivider = 0x20,
};

// ---------------------------------------------------------------------------
// I²C SCL frequency vs. divider (DS20005565D Table 3-1, Note 1)
// ---------------------------------------------------------------------------
/**
 * The MCP2221A derives SCL from a **12 MHz** internal clock. When byte index 3 of STATUS/SET
 * Parameters is `SetI2cSpeedTag::ApplyDivider`, byte index 4 is the **divider** D (one byte,
 * 0–255). Per DS20005565D Table 3-1 Note 1:
 *
 *     D = (12 MHz / f_SCL) − 2
 *
 * Equivalently:
 *
 *     f_SCL = 12 MHz / (D + 2)
 *
 * In code, `12000000U / (kbs * 1000U)` uses **integer division** (truncation toward zero), so the
 * realized f_SCL may differ slightly from the nominal target—check with `i2cNominalClockHz()`
 * after programming.
 *
 * **Range:** With D in 0…255, f_SCL spans roughly 12 MHz / 257 … 12 MHz / 2 (about **46.7 kHz** …
 * **6 MHz**). Targets that need D > 255 (e.g. ~10 kbit/s) **cannot** be encoded in one byte and are
 * not listed as named `I2cSpeedPreset` values.
 *
 * **I²C-bus mode classes** (NXP UM10204): bidirectional Sm / Fm / Fm+ / Hs-mode; unidirectional UFm.
 * The MCP2221A features list the I²C host up to **~400 kbit/s** in practice; higher targets follow
 * the same formula for API alignment.
 *
 * For a target SCL in **Hz** that is not a whole kbit/s, use `I2cClockDivider::fromNominalClockHz`
 * (same Table 3-1 formula with `12 MHz / f_Hz`).
 *
 * For an arbitrary nominal bit rate in **kbit/s**, use `i2cClockDividerForDataRateKbs`.
 *
 * @param kbs Nominal I²C bit rate in **kbit/s** (e.g. 100 for 100 kbit/s).
 * @return Divider byte D for byte index 4 when applying a new I²C speed.
 */
inline constexpr std::uint8_t i2cClockDividerForDataRateKbs(std::uint32_t kbs)
{
    return static_cast<std::uint8_t>((12000000U / (kbs * 1000U)) - 2U);
}

/**
 * Named I²C speeds; each enumerator’s **numeric value is the MCP2221A divider byte D** (Table 3-1
 * Note 1), i.e. `D = i2cClockDividerForDataRateKbs(<nominal kbit/s>)`. Use `nominalKbs(p)` for the
 * nominal rate in kbit/s. For any other rate, call `i2cClockDividerForDataRateKbs` (or
 * `I2cClockDivider::fromNominalKbs`) and wrap with `I2cClockDivider::fromRegisterValue`.
 */
enum class I2cSpeedPreset : std::uint8_t
{
    /** 50 kbit/s — D = 238. */
    LowSpeed_50kbs = i2cClockDividerForDataRateKbs(50U),

    /** Sm: 100 kbit/s — D = 118. */
    StandardMode_100kbs = i2cClockDividerForDataRateKbs(100U),

    /** 200 kbit/s — D = 58. */
    FastMode_200kbs = i2cClockDividerForDataRateKbs(200U),

    /** Fm: 400 kbit/s — D = 28. */
    FastMode_400kbs = i2cClockDividerForDataRateKbs(400U),

    /** Fm+: 1 Mbit/s — D = 10. */
    FastModePlus_1000kbs = i2cClockDividerForDataRateKbs(1000U),

    /** 1.7 Mbit/s — D = 5. */
    Intermediate_1700kbs = i2cClockDividerForDataRateKbs(1700U),

    /** Hs-mode: 3.4 Mbit/s — D = 1 (realized SCL may differ slightly). */
    HighSpeedMode_3400kbs = i2cClockDividerForDataRateKbs(3400U),

    /** UFm: 5 Mbit/s — D = 0. */
    UltraFastMode_5000kbs = i2cClockDividerForDataRateKbs(5000U),
};

/**
 * Nominal bit rate in **kbit/s** for a speed preset (documentation / display only; the wire value is
 * still `static_cast<std::uint8_t>(preset)` == D).
 */
inline constexpr std::uint16_t nominalKbs(I2cSpeedPreset p) noexcept
{
    switch (p)
    {
    case I2cSpeedPreset::LowSpeed_50kbs:
        return 50U;
    case I2cSpeedPreset::StandardMode_100kbs:
        return 100U;
    case I2cSpeedPreset::FastMode_200kbs:
        return 200U;
    case I2cSpeedPreset::FastMode_400kbs:
        return 400U;
    case I2cSpeedPreset::FastModePlus_1000kbs:
        return 1000U;
    case I2cSpeedPreset::Intermediate_1700kbs:
        return 1700U;
    case I2cSpeedPreset::HighSpeedMode_3400kbs:
        return 3400U;
    case I2cSpeedPreset::UltraFastMode_5000kbs:
        return 5000U;
    default:
        return 0U;
    }
}

/**
 * Divider byte sent in STATUS/SET Parameters (Table 3-1). Construct from `I2cSpeedPreset`, a raw D,
 * or `fromNominalKbs` / `fromNominalClockHz`.
 */
struct I2cClockDivider
{
    std::uint8_t _value{};

    /** Divider byte 0; ignored when `SetI2cSpeedTag::NoEffect` (e.g. STATUS read / snapshot). */
    constexpr I2cClockDivider() noexcept = default;

    /** Preset value is already the divider byte D. */
    constexpr explicit I2cClockDivider(I2cSpeedPreset preset) : _value(static_cast<std::uint8_t>(preset))
    {
    }

    /**
     * Builds D from a nominal bit rate in **kbit/s**: same as `i2cClockDividerForDataRateKbs`.
     */
    static constexpr I2cClockDivider fromNominalKbs(std::uint32_t kbs)
    {
        return I2cClockDivider{i2cClockDividerForDataRateKbs(kbs)};
    }

    /**
     * Builds D from a target SCL in **Hz**: `D = (12 MHz / i2c_clock_hz) − 2` (Table 3-1 Note 1).
     * Prefer `fromNominalKbs` when the rate is a whole kbit/s value.
     */
    static constexpr I2cClockDivider fromNominalClockHz(std::uint32_t i2cClockHz)
    {
        return I2cClockDivider{static_cast<std::uint8_t>((12000000U / i2cClockHz) - 2U)};
    }

    /** Raw divider byte as stored at report index 4 (Table 3-1). */
    static constexpr I2cClockDivider fromRegisterValue(std::uint8_t dividerByte)
    {
        return I2cClockDivider{dividerByte};
    }

    /** Equivalent to `I2cClockDivider{p}` (preset encodes D). */
    static constexpr I2cClockDivider fromI2cSpeedPreset(I2cSpeedPreset p)
    {
        return I2cClockDivider{p};
    }

private:
    constexpr explicit I2cClockDivider(std::uint8_t raw) : _value(raw) {}
};

/**
 * Nominal SCL frequency in Hz implied by divider D (inverse of Table 3-1 Note 1):
 *   f_SCL = 12 MHz / (D + 2).
 */
inline constexpr std::uint32_t i2cNominalClockHz(I2cClockDivider divider)
{
    return 12000000U / (static_cast<std::uint32_t>(divider._value) + 2U);
}

/**
 * Retry behaviour when the I2C engine returns busy (0x01) on the first HID command (Table 3-27).
 * Spacing is derived from the configured bus speed and read length so polling tracks bus time.
 */
struct I2cRetryPolicy
{
    /** Maximum attempts for the I2C Read Data / Repeated-START HID command. */
    unsigned _maxAttempts{32U};
    /**
     * Delay before retrying after I2cCommandStatus::EngineBusy (microseconds).
     * USB scheduling often dominates; this is a lower bound between polls.
     */
    unsigned _delayAfterBusyUs{500U};

    static constexpr I2cRetryPolicy singleAttempt() { return {1U, 0U}; }

    /**
     * Estimates one on-bus read duration (START + address + data + ACK bits) from the current
     * divider and byte count, then sets delay and a reasonable attempt cap (host USB round-trip
     * is not modeled exactly; values are conservative).
     */
    static I2cRetryPolicy forI2cRead(I2cClockDivider divider, std::uint16_t readByteCount);
};

// ---------------------------------------------------------------------------
// Flash read/write sub-codes (Table 3-3, Table 3-11)
// ---------------------------------------------------------------------------

enum class ReadFlashSubcode : std::uint8_t
{
    ChipSettings              = 0x00,
    GpSettings                = 0x01,
    UsbManufacturerString     = 0x02,
    UsbProductString          = 0x03,
    UsbSerialNumberString     = 0x04,
    ChipFactorySerialNumber   = 0x05,
};

enum class WriteFlashSubcode : std::uint8_t
{
    ChipSettings          = 0x00,
    GpSettings            = 0x01,
    UsbManufacturerString = 0x02,
    UsbProductString      = 0x03,
    UsbSerialNumberString = 0x04,
};

// ---------------------------------------------------------------------------
// I2C client address (Table 3-20 Note 2): 8-bit form, even = write, odd = read.
// ---------------------------------------------------------------------------

class I2cClientAddress
{
public:
    /** Build 8-bit address from 7-bit slave address for a write (R/W = 0). */
    static constexpr I2cClientAddress forWrite7bit(std::uint8_t address7bit)
    {
        return I2cClientAddress{static_cast<std::uint8_t>(static_cast<std::uint8_t>(address7bit << 1) & 0xFEU)};
    }

    /** Build 8-bit address from 7-bit slave address for a read (R/W = 1). */
    static constexpr I2cClientAddress forRead7bit(std::uint8_t address7bit)
    {
        return I2cClientAddress{static_cast<std::uint8_t>((static_cast<std::uint8_t>(address7bit << 1) & 0xFEU) | 1U)};
    }

    /** Use when the 8-bit address is already encoded per data sheet. */
    static constexpr I2cClientAddress fromRaw(std::uint8_t eightBitAddress)
    {
        return I2cClientAddress{eightBitAddress};
    }

    constexpr std::uint8_t raw() const { return _raw; }

private:
    constexpr explicit I2cClientAddress(std::uint8_t r) : _raw(r) {}

    std::uint8_t _raw{};
};

// ---------------------------------------------------------------------------
// GPIO (Tables 3-32, 3-34, 3-35)
// ---------------------------------------------------------------------------

enum class GpioAlterField : std::uint8_t
{
    NoChange = 0x00,
    /** Any non-zero value applies the paired value byte (data sheet Table 3-32). */
    Apply = 0x01,
};

enum class GpioOutputLevel : std::uint8_t
{
    LogicLow  = 0x00,
    LogicHigh = 0x01,
};

/** 0x00 = GPIO configured as output; non-zero = input (Table 3-32). */
enum class GpioPinDirection : std::uint8_t
{
    Output = 0x00,
    Input  = 0x01,
};

enum class GpioIndex : std::uint8_t
{
    Gp0 = 0,
    Gp1 = 1,
    Gp2 = 2,
    Gp3 = 3,
};

/** One GP pin block in the Set GPIO Output Values command (4 bytes per pin, Table 3-32). */
struct GpioPinCommandBlock
{
    GpioAlterField _alterOutput{};
    GpioOutputLevel _outputValue{};
    GpioAlterField _alterDirection{};
    GpioPinDirection _direction{};
};

/** Get GPIO Values response per pin: logic level and direction (Table 3-35). */
struct GpioPinState
{
    bool _configuredAsGpio{false};
    GpioOutputLevel _level{GpioOutputLevel::LogicLow};
    bool _directionIsInput{false};
};

// ---------------------------------------------------------------------------
// Protocol outcome / parsing (response byte 1 and others)
// ---------------------------------------------------------------------------

/** First response byte after command echo for most commands (e.g. Table 3-21). */
enum class I2cCommandStatus : std::uint8_t
{
    Success       = 0x00,
    EngineBusy    = 0x01,
};

/** Get I2C Data response byte 3: count of bytes in bytes 4.. or error marker (Table 3-31). */
enum class I2cGetDataCount : std::uint8_t
{
    ErrorNoValidData = 127U,
};

// ---------------------------------------------------------------------------
// Report layout: Status/Set Parameters response (Table 3-2)
// ---------------------------------------------------------------------------

namespace statusResponseIndex
{
inline constexpr std::size_t CommandEcho            = 0U;
inline constexpr std::size_t CompletionStatus       = 1U;
inline constexpr std::size_t CancelTransferStatus   = 2U;
inline constexpr std::size_t SpeedSetStatus         = 3U;
inline constexpr std::size_t I2cDividerEcho         = 4U;
inline constexpr std::size_t I2cRequestedLengthLo   = 9U;
inline constexpr std::size_t I2cRequestedLengthHi   = 10U;
inline constexpr std::size_t I2cTransferredLo       = 11U;
inline constexpr std::size_t I2cTransferredHi       = 12U;
inline constexpr std::size_t I2cBufferCounter       = 13U;
inline constexpr std::size_t CurrentI2cDivider      = 14U;
inline constexpr std::size_t CurrentI2cTimeout      = 15U;
inline constexpr std::size_t I2cAddressLo           = 16U;
inline constexpr std::size_t I2cAddressHi           = 17U;
inline constexpr std::size_t SclLineValue           = 22U;
inline constexpr std::size_t SdaLineValue           = 23U;
inline constexpr std::size_t InterruptEdgeState     = 24U;
inline constexpr std::size_t I2cReadPending         = 25U;
inline constexpr std::size_t HwRevisionMajor        = 46U;
inline constexpr std::size_t HwRevisionMinor        = 47U;
inline constexpr std::size_t FwRevisionMajor        = 48U;
inline constexpr std::size_t FwRevisionMinor        = 49U;
/** Three 16-bit little-endian ADC results: CH0, CH1, CH2 (Table 3-2). */
inline constexpr std::size_t AdcChannel0Lo          = 50U;
inline constexpr std::size_t AdcChannel0Hi          = 51U;
inline constexpr std::size_t AdcChannel1Lo          = 52U;
inline constexpr std::size_t AdcChannel1Hi          = 53U;
inline constexpr std::size_t AdcChannel2Lo          = 54U;
inline constexpr std::size_t AdcChannel2Hi          = 55U;
} // namespace statusResponseIndex

/** Parsed Status/Set Parameters response (subset of Table 3-2). */
struct StatusSnapshot
{
    Command _commandEcho{Command::StatusSetParameters};
    std::uint8_t _completionStatus{0U};
    std::uint16_t _i2cRequestedLength{0U};
    std::uint16_t _i2cTransferredLength{0U};
    std::uint8_t _i2cBufferCounter{0U};
    std::uint8_t _currentI2cDivider{0U};
    std::uint8_t _currentI2cTimeout{0U};
    std::uint16_t _i2cAddressUsed{0U};
    bool _sclHigh{false};
    bool _sdaHigh{false};
    std::uint8_t _interruptEdgeState{0U};
    std::uint8_t _i2cReadPending{0U};
    char _hwRevisionMajor{'?'};
    char _hwRevisionMinor{'?'};
    char _fwRevisionMajor{'?'};
    char _fwRevisionMinor{'?'};
    /** 10-bit ADC counts (device-dependent scaling; data sheet Section 1.x ADC). */
    std::uint16_t _adcCounts[3]{};
};

inline std::uint16_t readLe16(const RawReport& r, std::size_t offset)
{
    return static_cast<std::uint16_t>(static_cast<std::uint16_t>(r[offset]) |
                                      (static_cast<std::uint16_t>(r[offset + 1U]) << 8));
}

/** Parse Table 3-2 response into a structured snapshot (command echo must be Status/Set Parameters). */
inline StatusSnapshot statusSnapshotFromReport(const RawReport& r)
{
    StatusSnapshot s{};
    s._commandEcho = static_cast<Command>(r[statusResponseIndex::CommandEcho]);
    s._completionStatus       = r[statusResponseIndex::CompletionStatus];
    s._i2cRequestedLength    = readLe16(r, statusResponseIndex::I2cRequestedLengthLo);
    s._i2cTransferredLength  = readLe16(r, statusResponseIndex::I2cTransferredLo);
    s._i2cBufferCounter      = r[statusResponseIndex::I2cBufferCounter];
    s._currentI2cDivider     = r[statusResponseIndex::CurrentI2cDivider];
    s._currentI2cTimeout     = r[statusResponseIndex::CurrentI2cTimeout];
    s._i2cAddressUsed        = readLe16(r, statusResponseIndex::I2cAddressLo);
    s._sclHigh                = (r[statusResponseIndex::SclLineValue] != 0U);
    s._sdaHigh                = (r[statusResponseIndex::SdaLineValue] != 0U);
    s._interruptEdgeState    = r[statusResponseIndex::InterruptEdgeState];
    s._i2cReadPending        = r[statusResponseIndex::I2cReadPending];
    s._hwRevisionMajor       = static_cast<char>(r[statusResponseIndex::HwRevisionMajor]);
    s._hwRevisionMinor       = static_cast<char>(r[statusResponseIndex::HwRevisionMinor]);
    s._fwRevisionMajor       = static_cast<char>(r[statusResponseIndex::FwRevisionMajor]);
    s._fwRevisionMinor       = static_cast<char>(r[statusResponseIndex::FwRevisionMinor]);
    s._adcCounts[0]           = readLe16(r, statusResponseIndex::AdcChannel0Lo);
    s._adcCounts[1]           = readLe16(r, statusResponseIndex::AdcChannel1Lo);
    s._adcCounts[2]           = readLe16(r, statusResponseIndex::AdcChannel2Lo);
    return s;
}

/** Non-owning view of a 64-byte STATUS response (Table 3-2). */
struct StatusResponseView
{
    const RawReport& _r;

    explicit StatusResponseView(const RawReport& ref) : _r(ref) {}

    Command commandEcho() const { return static_cast<Command>(_r[0]); }

    StatusSnapshot snapshot() const { return statusSnapshotFromReport(_r); }
};

/**
 * Generic I2C engine reply: command echo (byte 0) and completion (byte 1) for Tables 3-21, 3-27, etc.
 */
struct I2cEngineResponseView
{
    const RawReport& _r;

    explicit I2cEngineResponseView(const RawReport& ref) : _r(ref) {}

    Command commandEcho() const { return static_cast<Command>(_r[0]); }

    I2cCommandStatus status() const { return static_cast<I2cCommandStatus>(_r[1]); }

    bool success() const { return _r[1] == static_cast<std::uint8_t>(I2cCommandStatus::Success); }

    bool engineBusy() const { return _r[1] == static_cast<std::uint8_t>(I2cCommandStatus::EngineBusy); }
};

// ---------------------------------------------------------------------------
// Get I2C Data (Table 3-31) — indices before I2cGetDataResponseView
// ---------------------------------------------------------------------------

namespace i2cGetDataIndex
{
inline constexpr std::size_t CommandEcho      = 0U;
inline constexpr std::size_t CompletionStatus = 1U;
inline constexpr std::size_t DataByteCount    = 3U;
inline constexpr std::size_t DataFirstByte    = 4U;
} // namespace i2cGetDataIndex

/** Get I2C Data reply (Table 3-31). */
enum class I2cGetDataStatus : std::uint8_t
{
    Success   = 0x00,
    ReadError = 0x41,
};

struct I2cGetDataResponseView
{
    const RawReport& _r;

    explicit I2cGetDataResponseView(const RawReport& ref) : _r(ref) {}

    Command commandEcho() const { return static_cast<Command>(_r[0]); }

    I2cGetDataStatus status() const { return static_cast<I2cGetDataStatus>(_r[1]); }

    bool success() const { return _r[1] == static_cast<std::uint8_t>(I2cGetDataStatus::Success); }

    /** Byte count for payload at indices 4.. (0–60); 127 means invalid (Table 3-31). */
    std::uint8_t payloadByteCount() const { return _r[i2cGetDataIndex::DataByteCount]; }

    /** Payload bytes at indices 4.. (length ≤ 60); empty if not `payloadValid()`. */
    ConstByteSpan payloadSpan() const
    {
        if (!payloadValid())
        {
            return {};
        }
        const std::uint8_t n = payloadByteCount();
        const std::size_t nBytes = std::min(static_cast<std::size_t>(n),
                                            ReportSize - i2cGetDataIndex::DataFirstByte);
        return ConstByteSpan(_r.data() + i2cGetDataIndex::DataFirstByte, nBytes);
    }

    bool payloadValid() const
    {
        const std::uint8_t n = payloadByteCount();
        return success() && (n != static_cast<std::uint8_t>(I2cGetDataCount::ErrorNoValidData));
    }
};

// ---------------------------------------------------------------------------
// Reset Chip fixed payload (Table 3-40)
// ---------------------------------------------------------------------------

inline constexpr std::uint8_t ResetKeyByte1 = 0xABU;
inline constexpr std::uint8_t ResetKeyByte2 = 0xCDU;
inline constexpr std::uint8_t ResetKeyByte3 = 0xEFU;

// ---------------------------------------------------------------------------
// GET SRAM Settings response indices (Table 3-39)
// ---------------------------------------------------------------------------

namespace getSramIndex
{
inline constexpr std::size_t CommandEcho     = 0U;
inline constexpr std::size_t Status          = 1U;
inline constexpr std::size_t ChipSettingsLen = 2U;
inline constexpr std::size_t GpSettingsLen   = 3U;
inline constexpr std::size_t ClockDivider    = 4U;
inline constexpr std::size_t ClockDuty       = 5U;
inline constexpr std::size_t DacRefValue     = 6U;   ///< Bits 7:6 = Vref option, bit 5 = Vref sel, bits 4:0 = power-up value
inline constexpr std::size_t Interrupt       = 7U;   ///< Bits 6:5 = edge detect
inline constexpr std::size_t AdcRef          = 8U;   ///< Bits 7:6 = Vref option, bit 5 = Vref sel
inline constexpr std::size_t Gp0             = 22U;
inline constexpr std::size_t Gp1             = 23U;
inline constexpr std::size_t Gp2             = 24U;
inline constexpr std::size_t Gp3             = 25U;
} // namespace getSramIndex

// ---------------------------------------------------------------------------
// SET SRAM Settings command indices (Table 3-36)
// ---------------------------------------------------------------------------

namespace setSramIndex
{
inline constexpr std::size_t Command       = 0U;
inline constexpr std::size_t ClockDivider  = 2U;   ///< Bit 7 = alter
inline constexpr std::size_t DacVref       = 3U;   ///< Bit 7 = alter, bits 2:0 = Vref
inline constexpr std::size_t DacValue      = 4U;   ///< Bit 7 = alter, bits 4:0 = value
inline constexpr std::size_t AdcVref       = 5U;   ///< Bit 7 = alter, bits 2:0 = Vref
inline constexpr std::size_t Interrupt     = 6U;   ///< Bit 7 = alter
inline constexpr std::size_t GpAlter       = 7U;   ///< Bit 7 = alter GP designation
inline constexpr std::size_t Gp0Settings   = 8U;
inline constexpr std::size_t Gp1Settings   = 9U;
inline constexpr std::size_t Gp2Settings   = 10U;
inline constexpr std::size_t Gp3Settings   = 11U;
} // namespace setSramIndex

inline I2cRetryPolicy I2cRetryPolicy::forI2cRead(I2cClockDivider divider, std::uint16_t readByteCount)
{
    const std::uint32_t hz = i2cNominalClockHz(divider);
    if (hz == 0U)
    {
        return singleAttempt();
    }
    /** START + 8-bit address + ACK + readByteCount * (8 data + 1 ACK) + STOP (order-of-magnitude). */
    const std::uint64_t bits = 9ULL + static_cast<std::uint64_t>(readByteCount) * 9ULL;
    const std::uint32_t oneReadUs =
        static_cast<std::uint32_t>((bits * 1000000ULL + static_cast<std::uint64_t>(hz) - 1ULL) /
                                   static_cast<std::uint64_t>(hz));
    constexpr std::uint32_t UsbHostMarginUs = 300U;

    I2cRetryPolicy p{};
    p._delayAfterBusyUs =
        std::max(100U, std::min(5000U, oneReadUs / 4U + UsbHostMarginUs / 4U));
    p._maxAttempts = 8U + (oneReadUs / 150U);
    if (p._maxAttempts > 64U)
    {
        p._maxAttempts = 64U;
    }
    if (p._maxAttempts < 4U)
    {
        p._maxAttempts = 4U;
    }
    return p;
}

} // namespace hid
} // namespace mcp2221a

#endif
