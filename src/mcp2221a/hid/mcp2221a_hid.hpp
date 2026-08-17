/**
 * MCP2221A USB HID host API using libusb-1.0 (interrupt transfers).
 * Protocol: DS20005565D Section 3.0 (64-byte reports).
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
 
#ifndef MCP2221A_HID_HPP
#define MCP2221A_HID_HPP

#include <cstddef>
#include <cstdint>
#include <libusb-1.0/libusb.h>
#include <memory>
#include <string>
#include <vector>

#include "mcp2221a_hid_types.hpp"

namespace mcp2221a
{
namespace hid
{

enum class Error
{
    None,
    NotConnected,
    InvalidArgument,
    UsbTransfer,
    ProtocolMismatch,
    IncompleteRead,
};

/** 8-byte Flash access password (Table 3-18, bytes 2–9). */
using FlashPassword = std::array<std::uint8_t, 8U>;

/** Alias for wire buffer; prefer RawReport / typed views in mcp2221a_hid_types.hpp for responses. */
using Report = RawReport;

/**
 * High-level wrapper: opens the default MCP2221A HID interface, sends OUT reports,
 * reads IN reports (except Reset Chip, which is OUT-only per data sheet).
 */
class Device
{
public:
    Device();
    ~Device();

    Device(const Device&)            = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) noexcept;
    Device& operator=(Device&&) noexcept;

    /** Count devices matching VID/PID on the current libusb context (no handle required). */
    int countDevices(std::uint16_t vid = DefaultVendorId, std::uint16_t pid = DefaultProductId) const;

    /** Open the Nth device matching VID/PID (defaults: DefaultVendorId / DefaultProductId). */
    bool openByIndex(std::uint16_t vid = DefaultVendorId, std::uint16_t pid = DefaultProductId,
                     int deviceIndex = 0);
    /**
     * Open the device matching VID/PID whose USB string descriptor (serial) equals `serialNo`.
     * Comparison is exact; use `openByIndex` if several devices share the same serial or lack one.
     */
    bool openBySN(std::uint16_t vid, std::uint16_t pid, const std::string& serialNo);
    void close();
    bool isOpen() const { return _handle != nullptr; }

    Error lastError() const { return _lastError; }
    void  clearLastError() { _lastError = Error::None; }

    /** Last libusb return code from interrupt transfer (e.g. negative LIBUSB_ERROR_*); 0 if none. */
    int lastLibusbError() const { return _lastLibusbErr; }
    /** Same as libusb_error_name(lastLibusbError()) when non-zero; empty string otherwise. */
    const char* lastLibusbErrorName() const;

    // --- Table 3-1 Status / Set Parameters ---
    /** Poll device status; ADC values are valid when GP pins are in ADC alternate mode. */
    bool statusSetParameters(CancelI2cTransfer cancel, SetI2cSpeedTag speedTag, I2cClockDivider divider);
    /** Same as overload without `rawResponseOut`, and copies the 64-byte IN report. */
    bool statusSetParameters(CancelI2cTransfer cancel, SetI2cSpeedTag speedTag, I2cClockDivider divider,
                             Report& rawResponseOut);

    bool getStatusSnapshot(StatusSnapshot& out);
    /** Same as overload without `rawResponseOut`, and returns the raw IN report. */
    bool getStatusSnapshot(StatusSnapshot& out, Report& rawResponseOut);

    // --- Table 3-3 / 3-11 Flash ---
    bool readFlash(ReadFlashSubcode subcode, std::vector<std::uint8_t>& payloadOut);
    bool readFlash(ReadFlashSubcode subcode, std::vector<std::uint8_t>& payloadOut, Report& rawResponseOut);
    /** IN report only (e.g. inspection); payload length is in `rawResponseOut[2]`. */
    bool readFlash(ReadFlashSubcode subcode, Report& rawResponseOut);

    bool writeFlash(WriteFlashSubcode subcode, const std::vector<std::uint8_t>& payloadFromByte2);

    /** Table 3-18 Send Flash Access Password. */
    bool sendFlashPassword(const FlashPassword& password);

    // --- Table 3-20 … 3-31 I2C ---
    /**
     * I2C Write Data: issues Start, address, data, Stop when total length reached (Table 3-20).
     * Chunks automatically when length exceeds 60 bytes per packet.
     */
    bool i2cWrite(I2cClientAddress address, ConstByteSpan data);

    bool i2cWriteRepeatedStart(I2cClientAddress address, ConstByteSpan data);
    bool i2cWriteNoStop(I2cClientAddress address, ConstByteSpan data);

    /**
     * I2C Read Data: issues read on the bus; payload is retrieved with Get I2C Data (Table 3-27, 3-31).
     * Single HID attempt; use the overload with I2cRetryPolicy if the engine may return busy (0x01).
     */
    bool i2cRead(I2cClientAddress address, std::uint16_t length);
    /** Retries while the I2C engine reports busy (Table 3-27), with delays from policy. */
    bool i2cRead(I2cClientAddress address, std::uint16_t length, const I2cRetryPolicy& policy);

    bool i2cReadRepeatedStart(I2cClientAddress address, std::uint16_t length);
    bool i2cReadRepeatedStart(I2cClientAddress address, std::uint16_t length, const I2cRetryPolicy& policy);

    /**
     * Issues I2C Read Data then polls Get I2C Data until `length` bytes are collected
     * (or an error / short count occurs).
     * The caller must already provide storage: `out.size() >= length` (nothing is allocated here).
     */
    bool i2cReadComplete(I2cClientAddress address, std::uint16_t length, MutableByteSpan out);
    bool i2cReadComplete(I2cClientAddress address, std::uint16_t length, MutableByteSpan out,
                         const I2cRetryPolicy& readPolicy);

    /**
     * Same as `MutableByteSpan` overload, but resizes `out` to `length` before reading
     * (dynamic allocation as needed).
     */
    bool i2cReadComplete(I2cClientAddress address, std::uint16_t length, std::vector<std::uint8_t>& out);
    bool i2cReadComplete(I2cClientAddress address, std::uint16_t length, std::vector<std::uint8_t>& out,
                         const I2cRetryPolicy& readPolicy);

    /** Table 3-30 / 3-31: retrieve bytes after an I2C read sequence. */
    bool i2cGetData(MutableByteSpan outBuffer, std::size_t& bytesCopiedOut);

    /**
     * Typical SMBus/register access: write (no STOP) then read with Repeated-START,
     * then drain read buffer via i2cGetData (data sheet Section 3.1.7 + 3.1.9 + 3.1.10).
     */
    bool i2cWriteThenRead(I2cClientAddress writeAddr, ConstByteSpan writeData, I2cClientAddress readAddr,
                          std::uint16_t readLength, MutableByteSpan readBuffer, std::size_t& readBytesOut);

    /**
     * Same as i2cWriteThenRead, but uses I2cRetryPolicy::forI2cRead for both the write (no STOP)
     * and read phases so EngineBusy (0x01) is retried at the configured bus speed.
     */
    bool i2cWriteThenRead(I2cClientAddress writeAddr, ConstByteSpan writeData, I2cClientAddress readAddr,
                          std::uint16_t readLength, MutableByteSpan readBuffer, std::size_t& readBytesOut,
                          I2cClockDivider configuredBusDivider);

    // --- Table 3-32 / 3-34 GPIO ---
    bool setGpioOutputs(const std::array<GpioPinCommandBlock, 4U>& pins);
    bool getGpioValues(std::array<GpioPinState, 4U>& out);

    // --- Table 3-36 / 3-38 SRAM (run-time DAC/ADC ref, clock, GP designation, etc.) ---
    /**
     * Bytes placed at report indices 2..11 (Table 3-36); indices 12..63 are zero-filled.
     * Use for on-the-fly DAC, references, clock divider, interrupt, and GP designation changes.
     */
    bool setSramSettings(const std::array<std::uint8_t, 10U>& bytesAtIndex2To11);

    bool getSramSettings(Report& rawOut);

    // --- Table 3-40 Reset ---
    /** Does not read a response (data sheet note on Table 3-40). */
    bool resetChip();

private:
    bool _openHandleAndClaim();
    /** Select configuration 1 before claim_interface (often required on Windows / WinUSB). */
    static void _setUsbConfigurationIfNeeded(libusb_device_handle* handle);
    bool _sendReport(const Report& out, Report* inOpt, bool expectInput);
    /** HID class SET_REPORT (Output) when the device has no interrupt OUT endpoint. */
    bool _sendOutputReportControl(const Report& out);
    bool _claimHidInterface();
    void _setError(Error e) { _lastError = e; }

    static bool _fillI2cSegment(Command cmd, I2cClientAddress address, std::uint16_t totalTransferLength,
                                ConstByteSpan data, Report* out);

    bool _i2cReadEngine(Command cmd, I2cClientAddress address, std::uint16_t length,
                        const I2cRetryPolicy& policy);
    /** Table 3-27: poll while byte 1 == EngineBusy (0x01). */
    bool _sendI2cReportWithBusyRetry(const Report& out, const I2cRetryPolicy& policy);
    bool _i2cWriteNoStopWithPolicy(I2cClientAddress address, ConstByteSpan data, const I2cRetryPolicy& policy);

    libusb_context* _context{nullptr};
    libusb_device_handle* _handle{nullptr};
    int _claimedInterface{-1};
    /** From USB descriptor; 0 = no interrupt OUT (use SET_REPORT). */
    unsigned char _epInterruptOut{0};
    /** Interrupt IN (required); 0 before claim. */
    unsigned char _epInterruptIn{0};
    Error _lastError{Error::None};
    int   _lastLibusbErr{0};
};

} // namespace hid
} // namespace mcp2221a

#endif
