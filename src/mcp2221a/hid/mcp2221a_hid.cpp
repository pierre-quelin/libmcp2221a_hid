/**
 * MCP2221A HID transport and command helpers (DS20005565D Section 3.0).
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

#include "mcp2221a_hid.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <thread>

namespace mcp2221a
{
namespace hid
{

namespace
{

constexpr int UsbTimeoutMs = 1000;

/** HID Output report type for SET_REPORT wValue (high byte). */
constexpr std::uint8_t HidReportTypeOutput = 2U;

/**
 * Fills interrupt OUT/IN addresses for the given interface (0 = no OUT; many MCP2221 variants have IN only).
 * @return true if at least one interrupt IN exists (required for device replies).
 */
bool discoverHidInterruptEndpoints(libusb_device* dev, int interfaceNumber, unsigned char& epOut, unsigned char& epIn)
{
    epOut = 0;
    epIn  = 0;
    libusb_config_descriptor* cfg = nullptr;
    if (libusb_get_active_config_descriptor(dev, &cfg) != 0)
    {
        return false;
    }
    for (int i = 0; i < cfg->bNumInterfaces; ++i)
    {
        const libusb_interface& iface = cfg->interface[i];
        for (int a = 0; a < iface.num_altsetting; ++a)
        {
            const libusb_interface_descriptor& alt = iface.altsetting[a];
            if (static_cast<int>(alt.bInterfaceNumber) != interfaceNumber)
            {
                continue;
            }
            for (int e = 0; e < alt.bNumEndpoints; ++e)
            {
                const libusb_endpoint_descriptor& ep = alt.endpoint[e];
                const unsigned int                ttype =
                    ep.bmAttributes & LIBUSB_TRANSFER_TYPE_MASK;
                if (ttype != LIBUSB_TRANSFER_TYPE_INTERRUPT)
                {
                    continue;
                }
                const unsigned char addr = ep.bEndpointAddress;
                if ((addr & LIBUSB_ENDPOINT_IN) != 0)
                {
                    if (epIn == 0)
                    {
                        epIn = addr;
                    }
                }
                else if (epOut == 0)
                {
                    epOut = addr;
                }
            }
        }
    }
    libusb_free_config_descriptor(cfg);
    return epIn != 0;
}

/**
 * Some backends (notably WinUSB/libusb HID on Windows) may:
 * - complete one interrupt URB per packet (wMaxPacketSize 8/16/32/64);
 * - write one extra Report-ID byte past the requested length (RTC #2 / stack smash).
 * MCP2221A protocol payload remains 64 bytes — re-submit until `length` is filled,
 * but `data` must have `capacity >= length + HidOverrunSlop`.
 */
constexpr int HidOverrunSlop = 16;

bool interruptTransferAll(libusb_device_handle* handle, unsigned char ep, unsigned char* data, int length,
                           int capacity, int* outLastErr)
{
    if (data == nullptr || length <= 0 || capacity < length + HidOverrunSlop)
    {
        *outLastErr = LIBUSB_ERROR_INVALID_PARAM;
        return false;
    }
    int filled = 0;
    int rounds = 0;
    *outLastErr = 0;
    while (filled < length)
    {
        if (++rounds > length * 4)
        {
            *outLastErr = LIBUSB_ERROR_OVERFLOW;
            return false;
        }
        const int remaining = length - filled;
        const int space     = capacity - filled;
        int transferred     = 0;
        // Request only `remaining` protocol bytes; `space` absorbs Windows HID overruns.
        const int st = libusb_interrupt_transfer(handle, ep, data + filled, remaining, &transferred,
                                                 UsbTimeoutMs);
        if (st != 0)
        {
            *outLastErr = st;
            return false;
        }
        if (transferred <= 0)
        {
            *outLastErr = LIBUSB_ERROR_IO;
            return false;
        }
        // Accept a 1-byte overrun (Report ID) into slop; never advance `filled` past `length`.
        if (transferred > remaining)
        {
            if (transferred > space)
            {
                *outLastErr = LIBUSB_ERROR_OVERFLOW;
                return false;
            }
            // Keep only the protocol bytes we asked for (drop trailing overrun in-place).
            transferred = remaining;
        }
        filled += transferred;
    }
    return true;
}

void writeLe16(Report& r, std::size_t offset, std::uint16_t v)
{
    r[offset]     = static_cast<std::uint8_t>(v & 0xFFU);
    r[offset + 1U] = static_cast<std::uint8_t>((v >> 8) & 0xFFU);
}

bool parseGpioPin(std::uint8_t valueByte, std::uint8_t dirByte, GpioPinState& pin)
{
    if (valueByte == 0xEEU || dirByte == 0xEFU)
    {
        pin._configuredAsGpio = false;
        return true;
    }
    pin._configuredAsGpio   = true;
    pin._level                = (valueByte != 0U) ? GpioOutputLevel::LogicHigh : GpioOutputLevel::LogicLow;
    pin._directionIsInput   = (dirByte != 0U);
    return true;
}

int findHidInterfaceNumber(libusb_device* dev)
{
    libusb_config_descriptor* cfg = nullptr;
    if (libusb_get_active_config_descriptor(dev, &cfg) != 0)
    {
        return -1;
    }
    int found = -1;
    for (int i = 0; i < cfg->bNumInterfaces; ++i)
    {
        const libusb_interface& iface = cfg->interface[i];
        for (int a = 0; a < iface.num_altsetting; ++a)
        {
            if (iface.altsetting[a].bInterfaceClass == LIBUSB_CLASS_HID)
            {
                found = iface.altsetting[a].bInterfaceNumber;
                break;
            }
        }
        if (found >= 0)
        {
            break;
        }
    }
    libusb_free_config_descriptor(cfg);
    return found;
}

} // namespace

void Device::_setUsbConfigurationIfNeeded(libusb_device_handle* handle)
{
    if (handle == nullptr)
    {
        return;
    }
    int cfg = 0;
    if (libusb_get_configuration(handle, &cfg) == 0 && cfg == 1)
    {
        return;
    }
    /** Ignore return value: already configured / busy / OS quirks — claim_interface will fail if unusable. */
    (void)libusb_set_configuration(handle, 1);
}

Device::Device()
{
    libusb_init(&_context);
}

const char* Device::lastLibusbErrorName() const
{
    if (_lastLibusbErr == 0)
    {
        return "";
    }
    return libusb_error_name(_lastLibusbErr);
}

Device::~Device()
{
    close();
    if (_context != nullptr)
    {
        libusb_exit(_context);
        _context = nullptr;
    }
}

Device::Device(Device&& o) noexcept
    : _context(o._context), _handle(o._handle), _claimedInterface(o._claimedInterface),
      _epInterruptOut(o._epInterruptOut), _epInterruptIn(o._epInterruptIn), _lastError(o._lastError),
      _lastLibusbErr(o._lastLibusbErr)
{
    o._context           = nullptr;
    o._handle            = nullptr;
    o._claimedInterface = -1;
    o._epInterruptOut    = 0;
    o._epInterruptIn     = 0;
    o._lastLibusbErr     = 0;
}

Device& Device::operator=(Device&& o) noexcept
{
    if (this != &o)
    {
        close();
        if (_context != nullptr)
        {
            libusb_exit(_context);
        }
        _context             = o._context;
        _handle              = o._handle;
        _claimedInterface   = o._claimedInterface;
        _lastError          = o._lastError;
        _lastLibusbErr      = o._lastLibusbErr;
        _epInterruptOut     = o._epInterruptOut;
        _epInterruptIn      = o._epInterruptIn;
        o._context           = nullptr;
        o._handle            = nullptr;
        o._claimedInterface = -1;
        o._epInterruptOut    = 0;
        o._epInterruptIn     = 0;
        o._lastLibusbErr     = 0;
    }
    return *this;
}

bool Device::_claimHidInterface()
{
    libusb_device* dev = libusb_get_device(_handle);
    int              ifNum = findHidInterfaceNumber(dev);
    if (ifNum < 0)
    {
        _setError(Error::UsbTransfer);
        return false;
    }
    if (libusb_kernel_driver_active(_handle, ifNum) == 1)
    {
        (void)libusb_detach_kernel_driver(_handle, ifNum);
    }
    int st = libusb_claim_interface(_handle, ifNum);
    if (st != 0)
    {
        _setError(Error::UsbTransfer);
        return false;
    }
    unsigned char epOut = 0;
    unsigned char epIn  = 0;
    if (!discoverHidInterruptEndpoints(dev, ifNum, epOut, epIn))
    {
        (void)libusb_release_interface(_handle, ifNum);
        _setError(Error::UsbTransfer);
        return false;
    }
    _claimedInterface = ifNum;
    _epInterruptOut   = epOut;
    _epInterruptIn    = epIn;
    // Clear sticky HALT after (re)claim — avoids AV/timeouts on first transfers (Windows/WinUSB).
    if (_epInterruptIn != 0)
    {
        (void)libusb_clear_halt(_handle, _epInterruptIn);
    }
    if (_epInterruptOut != 0)
    {
        (void)libusb_clear_halt(_handle, _epInterruptOut);
    }
    return true;
}

bool Device::_openHandleAndClaim()
{
#if LIBUSB_API_VERSION >= 0x01000106
    (void)libusb_set_auto_detach_kernel_driver(_handle, 1);
#endif
    _setUsbConfigurationIfNeeded(_handle);
    if (!_claimHidInterface())
    {
        libusb_close(_handle);
        _handle = nullptr;
        return false;
    }
    return true;
}

bool Device::openByIndex(std::uint16_t vid, std::uint16_t pid, int deviceIndex)
{
    close();
    _lastError = Error::None;

    libusb_device** list = nullptr;
    const ssize_t cnt    = libusb_get_device_list(_context, &list);
    if (cnt < 0)
    {
        _setError(Error::UsbTransfer);
        return false;
    }

    int matchIndex = 0;
    bool opened     = false;
    for (ssize_t i = 0; i < cnt; ++i)
    {
        libusb_device_descriptor d {};
        if (libusb_get_device_descriptor(list[i], &d) != 0)
        {
            continue;
        }
        if (d.idVendor == vid && d.idProduct == pid)
        {
            if (matchIndex == deviceIndex)
            {
                if (libusb_open(list[i], &_handle) == 0)
                {
                    opened = true;
                }
                break;
            }
            ++matchIndex;
        }
    }
    libusb_free_device_list(list, 1);

    if (!opened || _handle == nullptr)
    {
        _setError(Error::NotConnected);
        return false;
    }

    return _openHandleAndClaim();
}

bool Device::openBySN(std::uint16_t vid, std::uint16_t pid, const std::string& serialNo)
{
    close();
    _lastError = Error::None;

    libusb_device** list = nullptr;
    const ssize_t cnt    = libusb_get_device_list(_context, &list);
    if (cnt < 0)
    {
        _setError(Error::UsbTransfer);
        return false;
    }

    bool opened = false;
    for (ssize_t i = 0; i < cnt; ++i)
    {
        libusb_device_descriptor d {};
        if (libusb_get_device_descriptor(list[i], &d) != 0)
        {
            continue;
        }
        if (d.idVendor != vid || d.idProduct != pid)
        {
            continue;
        }

        libusb_device_handle* testHandle = nullptr;
        if (libusb_open(list[i], &testHandle) != 0 || testHandle == nullptr)
        {
            continue;
        }

        if (d.iSerialNumber == 0)
        {
            if (serialNo.empty())
            {
                _handle = testHandle;
                opened  = true;
                break;
            }
            libusb_close(testHandle);
            continue;
        }

        unsigned char serialBuf[256] = {};
        const int     n               = libusb_get_string_descriptor_ascii(
            testHandle, d.iSerialNumber, serialBuf, static_cast<int>(sizeof(serialBuf)));
        if (n < 0)
        {
            libusb_close(testHandle);
            continue;
        }
        const std::string devSerial(reinterpret_cast<const char*>(serialBuf), static_cast<std::size_t>(n));
        if (devSerial != serialNo)
        {
            libusb_close(testHandle);
            continue;
        }
        _handle = testHandle;
        opened  = true;
        break;
    }
    libusb_free_device_list(list, 1);

    if (!opened || _handle == nullptr)
    {
        _setError(Error::NotConnected);
        return false;
    }

    return _openHandleAndClaim();
}

void Device::close()
{
    if (_handle != nullptr)
    {
        if (_claimedInterface >= 0)
        {
            (void)libusb_release_interface(_handle, _claimedInterface);
            _claimedInterface = -1;
        }
        libusb_close(_handle);
        _handle = nullptr;
    }
    _lastError     = Error::None;
    _lastLibusbErr = 0;
    _epInterruptOut = 0;
    _epInterruptIn  = 0;
}

bool Device::_sendOutputReportControl(const Report& out)
{
    if (_handle == nullptr || _claimedInterface < 0)
    {
        return false;
    }
    const std::uint16_t wValue = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(HidReportTypeOutput) << 8) | 0U);
    const int st = libusb_control_transfer(
        _handle,
        static_cast<std::uint8_t>(LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE | LIBUSB_ENDPOINT_OUT),
        0x09, // SET_REPORT
        wValue,
        static_cast<std::uint16_t>(_claimedInterface),
        const_cast<unsigned char*>(out.data()),
        static_cast<std::uint16_t>(ReportSize),
        static_cast<unsigned int>(UsbTimeoutMs));
    _lastLibusbErr = st;
    if (st < 0)
    {
        return false;
    }
    if (st != static_cast<int>(ReportSize))
    {
        /** Short/zero write: libusb returns a byte count, not a negative code — keep a real libusb code for logs. */
        _lastLibusbErr = (st == 0) ? LIBUSB_ERROR_IO : LIBUSB_ERROR_OVERFLOW;
        _setError(Error::UsbTransfer);
        return false;
    }
    return true;
}

bool Device::_sendReport(const Report& out, Report* inOpt, bool expectInput)
{
    if (_handle == nullptr)
    {
        _setError(Error::NotConnected);
        _lastLibusbErr = 0;
        return false;
    }
    if (_epInterruptIn == 0)
    {
        _lastLibusbErr = LIBUSB_ERROR_NOT_FOUND;
        _setError(Error::UsbTransfer);
        return false;
    }

    // Never pass the 64-byte Report stack object to libusb: Windows HID may write Report-ID+payload
    // past the requested length (RTC #2 — "Stack around the variable 'in' was corrupted").
    constexpr int WireCap = static_cast<int>(ReportSize) + HidOverrunSlop;
    std::array<std::uint8_t, static_cast<std::size_t>(WireCap)> wireOut {};
    std::copy(out.begin(), out.end(), wireOut.begin());

    _lastLibusbErr = 0;
    bool outOk     = false;
    {
        if (_epInterruptOut != 0)
        {
            outOk = interruptTransferAll(_handle, _epInterruptOut, wireOut.data(),
                                         static_cast<int>(ReportSize), WireCap, &_lastLibusbErr);
        }
        else
        {
            outOk = _sendOutputReportControl(out);
        }
    }
    if (!outOk)
    {
        _setError(Error::UsbTransfer);
        return false;
    }

    if (!expectInput)
    {
        _lastLibusbErr = 0;
        return true;
    }

    if (inOpt == nullptr)
    {
        _setError(Error::InvalidArgument);
        _lastLibusbErr = 0;
        return false;
    }

    std::array<std::uint8_t, static_cast<std::size_t>(WireCap)> wireIn {};
    if (!interruptTransferAll(_handle, _epInterruptIn, wireIn.data(), static_cast<int>(ReportSize), WireCap,
                              &_lastLibusbErr))
    {
        _setError(Error::UsbTransfer);
        return false;
    }
    // Windows HID may physically write Report-ID (0x00) + 64 payload while reporting 64 transferred.
    // Command echoes are in 0x10..0xFF (Table 3-x); a leading 0x00 is the Report ID, not payload.
    const bool leadingReportId = (wireIn[0] == 0U && wireIn[1] >= 0x10U);
    if (leadingReportId)
    {
        std::copy_n(wireIn.begin() + 1, ReportSize, inOpt->begin());
    }
    else
    {
        std::copy_n(wireIn.begin(), ReportSize, inOpt->begin());
    }
    _lastLibusbErr = 0;
    return true;
}

bool Device::statusSetParameters(CancelI2cTransfer cancel, SetI2cSpeedTag speedTag, I2cClockDivider divider)
{
    Report out {};
    out[0] = static_cast<std::uint8_t>(Command::StatusSetParameters);
    out[1] = 0U;
    out[2] = static_cast<std::uint8_t>(cancel);
    out[3] = static_cast<std::uint8_t>(speedTag);
    out[4] = divider._value;

    Report in {};
    return _sendReport(out, &in, true);
}

bool Device::statusSetParameters(CancelI2cTransfer cancel, SetI2cSpeedTag speedTag,
                                   I2cClockDivider divider, Report& rawResponseOut)
{
    Report out {};
    out[0] = static_cast<std::uint8_t>(Command::StatusSetParameters);
    out[1] = 0U;
    out[2] = static_cast<std::uint8_t>(cancel);
    out[3] = static_cast<std::uint8_t>(speedTag);
    out[4] = divider._value;

    Report in {};
    if (!_sendReport(out, &in, true))
    {
        return false;
    }
    rawResponseOut = in;
    return true;
}

bool Device::getStatusSnapshot(StatusSnapshot& out)
{
    Report r {};
    if (!statusSetParameters(CancelI2cTransfer::NoEffect, SetI2cSpeedTag::NoEffect, I2cClockDivider{}, r))
    {
        return false;
    }
    out = statusSnapshotFromReport(r);
    return true;
}

bool Device::getStatusSnapshot(StatusSnapshot& out, Report& rawResponseOut)
{
    if (!statusSetParameters(CancelI2cTransfer::NoEffect, SetI2cSpeedTag::NoEffect, I2cClockDivider{},
                             rawResponseOut))
    {
        return false;
    }
    out = statusSnapshotFromReport(rawResponseOut);
    return true;
}

bool Device::readFlash(ReadFlashSubcode subcode, std::vector<std::uint8_t>& payloadOut)
{
    Report unusedRaw {};
    return readFlash(subcode, payloadOut, unusedRaw);
}

bool Device::readFlash(ReadFlashSubcode subcode, std::vector<std::uint8_t>& payloadOut, Report& rawResponseOut)
{
    Report out {};
    out[0] = static_cast<std::uint8_t>(Command::ReadFlashData);
    out[1] = static_cast<std::uint8_t>(subcode);
    Report in {};
    if (!_sendReport(out, &in, true))
    {
        return false;
    }
    rawResponseOut = in;
    if (in[1] != 0U)
    {
        _setError(Error::ProtocolMismatch);
        return false;
    }
    const std::uint8_t len = in[2];
    /** Data for several sub-commands starts at byte index 4 (e.g. Table 3-5 Read Chip Settings). */
    constexpr std::size_t DataStart = 4U;
    const std::size_t     maxCopy =
        std::min(static_cast<std::size_t>(len), ReportSize - DataStart);
    payloadOut.clear();
    payloadOut.insert(payloadOut.end(), in.begin() + static_cast<std::ptrdiff_t>(DataStart),
                         in.begin() + static_cast<std::ptrdiff_t>(DataStart + maxCopy));
    return true;
}

bool Device::readFlash(ReadFlashSubcode subcode, Report& rawResponseOut)
{
    Report out {};
    out[0] = static_cast<std::uint8_t>(Command::ReadFlashData);
    out[1] = static_cast<std::uint8_t>(subcode);
    Report in {};
    if (!_sendReport(out, &in, true))
    {
        return false;
    }
    rawResponseOut = in;
    if (in[1] != 0U)
    {
        _setError(Error::ProtocolMismatch);
        return false;
    }
    return true;
}

bool Device::writeFlash(WriteFlashSubcode subcode, const std::vector<std::uint8_t>& payloadFromByte2)
{
    if (payloadFromByte2.size() > (ReportSize - 3U))
    {
        _setError(Error::InvalidArgument);
        return false;
    }
    Report out {};
    out[0] = static_cast<std::uint8_t>(Command::WriteFlashData);
    out[1] = static_cast<std::uint8_t>(subcode);
    std::copy(payloadFromByte2.begin(), payloadFromByte2.end(), out.begin() + 2);
    Report in {};
    if (!_sendReport(out, &in, true))
    {
        return false;
    }
    if (in[1] != 0U)
    {
        _setError(Error::ProtocolMismatch);
        return false;
    }
    return true;
}

bool Device::sendFlashPassword(const FlashPassword& password)
{
    Report out {};
    out[0] = static_cast<std::uint8_t>(Command::SendFlashPassword);
    out[1] = 0U;
    std::copy(password.begin(), password.end(), out.begin() + 2);
    Report in {};
    if (!_sendReport(out, &in, true))
    {
        return false;
    }
    if (in[1] != 0U)
    {
        _setError(Error::ProtocolMismatch);
        return false;
    }
    return true;
}

bool Device::_fillI2cSegment(Command cmd, I2cClientAddress address, std::uint16_t totalTransferLength,
                               ConstByteSpan data, Report* out)
{
    (*out)[0] = static_cast<std::uint8_t>(cmd);
    writeLe16(*out, 1U, totalTransferLength);
    (*out)[3] = address.raw();
    if (data.size() > (ReportSize - 4U))
    {
        return false;
    }
    if (!data.empty())
    {
        std::memcpy(out->data() + 4U, data.data(), data.size());
    }
    return true;
}

bool Device::i2cWrite(I2cClientAddress address, ConstByteSpan data)
{
    const std::size_t length = data.size();
    if (length > 0xFFFFU)
    {
        _setError(Error::InvalidArgument);
        return false;
    }
    const auto total = static_cast<std::uint16_t>(length);
    std::size_t offset = 0U;
    while (offset < length)
    {
        const std::size_t chunk = std::min<std::size_t>(60U, length - offset);
        Report out {};
        if (!_fillI2cSegment(Command::I2cWriteData, address, total, data.subspan(offset, chunk), &out))
        {
            _setError(Error::InvalidArgument);
            return false;
        }
        if (!_sendI2cReportWithBusyRetry(out, I2cRetryPolicy{}))
        {
            return false;
        }
        offset += chunk;
    }
    return true;
}

bool Device::i2cWriteRepeatedStart(I2cClientAddress address, ConstByteSpan data)
{
    const std::size_t length = data.size();
    if (length > 0xFFFFU)
    {
        _setError(Error::InvalidArgument);
        return false;
    }
    const auto total = static_cast<std::uint16_t>(length);
    std::size_t offset = 0U;
    while (offset < length)
    {
        const std::size_t chunk = std::min<std::size_t>(60U, length - offset);
        Report out {};
        if (!_fillI2cSegment(Command::I2cWriteRepeatedStart, address, total, data.subspan(offset, chunk),
                             &out))
        {
            _setError(Error::InvalidArgument);
            return false;
        }
        if (!_sendI2cReportWithBusyRetry(out, I2cRetryPolicy{}))
        {
            return false;
        }
        offset += chunk;
    }
    return true;
}

bool Device::i2cWriteNoStop(I2cClientAddress address, ConstByteSpan data)
{
    return _i2cWriteNoStopWithPolicy(address, data, I2cRetryPolicy{});
}

bool Device::_i2cWriteNoStopWithPolicy(I2cClientAddress address, ConstByteSpan data, const I2cRetryPolicy& policy)
{
    const std::size_t length = data.size();
    if (length > 0xFFFFU)
    {
        _setError(Error::InvalidArgument);
        return false;
    }
    const auto total = static_cast<std::uint16_t>(length);
    std::size_t offset = 0U;
    while (offset < length)
    {
        const std::size_t chunk = std::min<std::size_t>(60U, length - offset);
        Report out {};
        if (!_fillI2cSegment(Command::I2cWriteNoStop, address, total, data.subspan(offset, chunk), &out))
        {
            _setError(Error::InvalidArgument);
            return false;
        }
        if (!_sendI2cReportWithBusyRetry(out, policy))
        {
            return false;
        }
        offset += chunk;
    }
    return true;
}

bool Device::_sendI2cReportWithBusyRetry(const Report& out, const I2cRetryPolicy& policy)
{
    for (unsigned attempt = 0U; attempt < policy._maxAttempts; ++attempt)
    {
        Report in {};
        if (!_sendReport(out, &in, true))
        {
            return false;
        }
        const I2cEngineResponseView view(in);
        if (view.success())
        {
            return true;
        }
        if (view.engineBusy() && attempt + 1U < policy._maxAttempts && policy._delayAfterBusyUs > 0U)
        {
            std::this_thread::sleep_for(std::chrono::microseconds(policy._delayAfterBusyUs));
            continue;
        }
        _setError(Error::ProtocolMismatch);
        return false;
    }
    _setError(Error::ProtocolMismatch);
    return false;
}

bool Device::_i2cReadEngine(Command cmd, I2cClientAddress address, std::uint16_t length,
                              const I2cRetryPolicy& policy)
{
    Report out {};
    if (!_fillI2cSegment(cmd, address, length, ConstByteSpan{}, &out))
    {
        _setError(Error::InvalidArgument);
        return false;
    }
    return _sendI2cReportWithBusyRetry(out, policy);
}

bool Device::i2cRead(I2cClientAddress address, std::uint16_t length)
{
    return i2cRead(address, length, I2cRetryPolicy::singleAttempt());
}

bool Device::i2cRead(I2cClientAddress address, std::uint16_t length, const I2cRetryPolicy& policy)
{
    return _i2cReadEngine(Command::I2cReadData, address, length, policy);
}

bool Device::i2cReadRepeatedStart(I2cClientAddress address, std::uint16_t length)
{
    return i2cReadRepeatedStart(address, length, I2cRetryPolicy::singleAttempt());
}

bool Device::i2cReadRepeatedStart(I2cClientAddress address, std::uint16_t length,
                                     const I2cRetryPolicy& policy)
{
    return _i2cReadEngine(Command::I2cReadRepeatedStart, address, length, policy);
}

bool Device::i2cGetData(MutableByteSpan outBuffer, std::size_t& bytesCopiedOut)
{
    Report out {};
    out[0] = static_cast<std::uint8_t>(Command::I2cGetData);
    Report in {};
    if (!_sendReport(out, &in, true))
    {
        return false;
    }
    if (in[1] != 0U)
    {
        if (in[1] == 0x41U)
        {
            _setError(Error::ProtocolMismatch);
        }
        else
        {
            _setError(Error::ProtocolMismatch);
        }
        return false;
    }
    const std::uint8_t n = in[i2cGetDataIndex::DataByteCount];
    if (n == static_cast<std::uint8_t>(I2cGetDataCount::ErrorNoValidData))
    {
        _setError(Error::ProtocolMismatch);
        return false;
    }
    const std::size_t copyN = std::min<std::size_t>(n, outBuffer.size());
    if (copyN > 0U)
    {
        std::memcpy(outBuffer.data(), in.data() + i2cGetDataIndex::DataFirstByte, copyN);
    }
    bytesCopiedOut = copyN;
    return true;
}

bool Device::i2cReadComplete(I2cClientAddress address, std::uint16_t length, MutableByteSpan out)
{
    return i2cReadComplete(address, length, out, I2cRetryPolicy::singleAttempt());
}

bool Device::i2cReadComplete(I2cClientAddress address, std::uint16_t length, MutableByteSpan out,
                               const I2cRetryPolicy& readPolicy)
{
    if (out.size() < static_cast<std::size_t>(length))
    {
        _setError(Error::InvalidArgument);
        return false;
    }
    if (!i2cRead(address, length, readPolicy))
    {
        return false;
    }
    std::size_t total = 0U;
    while (total < static_cast<std::size_t>(length))
    {
        std::size_t chunk = 0U;
        if (!i2cGetData(out.subspan(total), chunk))
        {
            return false;
        }
        if (chunk == 0U)
        {
            _setError(Error::IncompleteRead);
            return false;
        }
        total += chunk;
        if (total > static_cast<std::size_t>(length))
        {
            _setError(Error::ProtocolMismatch);
            return false;
        }
    }
    return true;
}

bool Device::i2cWriteThenRead(I2cClientAddress writeAddr, ConstByteSpan writeData,
                                 I2cClientAddress readAddr, std::uint16_t readLength,
                                 MutableByteSpan readBuffer, std::size_t& readBytesOut)
{
    if (readLength > 0U && readBuffer.size() < static_cast<std::size_t>(readLength))
    {
        _setError(Error::InvalidArgument);
        return false;
    }
    readBytesOut = 0U;
    if (!i2cWriteNoStop(writeAddr, writeData))
    {
        return false;
    }
    if (!i2cReadRepeatedStart(readAddr, readLength))
    {
        return false;
    }
    std::size_t total = 0U;
    while (total < static_cast<std::size_t>(readLength))
    {
        std::size_t chunk = 0U;
        if (!i2cGetData(readBuffer.subspan(total), chunk))
        {
            return false;
        }
        if (chunk == 0U)
        {
            _setError(Error::IncompleteRead);
            return false;
        }
        total += chunk;
    }
    readBytesOut = total;
    return true;
}

bool Device::i2cWriteThenRead(I2cClientAddress writeAddr, ConstByteSpan writeData,
                                 I2cClientAddress readAddr, std::uint16_t readLength,
                                 MutableByteSpan readBuffer, std::size_t& readBytesOut,
                                 I2cClockDivider configuredBusDivider)
{
    if (readLength > 0U && readBuffer.size() < static_cast<std::size_t>(readLength))
    {
        _setError(Error::InvalidArgument);
        return false;
    }
    readBytesOut = 0U;
    const I2cRetryPolicy writePolicy =
        I2cRetryPolicy::forI2cRead(configuredBusDivider, static_cast<std::uint16_t>(writeData.size()));
    const I2cRetryPolicy readPolicy = I2cRetryPolicy::forI2cRead(configuredBusDivider, readLength);
    if (!_i2cWriteNoStopWithPolicy(writeAddr, writeData, writePolicy))
    {
        return false;
    }
    if (!i2cReadRepeatedStart(readAddr, readLength, readPolicy))
    {
        return false;
    }
    std::size_t total = 0U;
    while (total < static_cast<std::size_t>(readLength))
    {
        std::size_t chunk = 0U;
        if (!i2cGetData(readBuffer.subspan(total), chunk))
        {
            return false;
        }
        if (chunk == 0U)
        {
            _setError(Error::IncompleteRead);
            return false;
        }
        total += chunk;
    }
    readBytesOut = total;
    return true;
}

bool Device::i2cReadComplete(I2cClientAddress address, std::uint16_t length, std::vector<std::uint8_t>& out)
{
    return i2cReadComplete(address, length, out, I2cRetryPolicy::singleAttempt());
}

bool Device::i2cReadComplete(I2cClientAddress address, std::uint16_t length, std::vector<std::uint8_t>& out,
                               const I2cRetryPolicy& readPolicy)
{
    out.resize(static_cast<std::size_t>(length));
    return i2cReadComplete(address, length, MutableByteSpan(out), readPolicy);
}

bool Device::setGpioOutputs(const std::array<GpioPinCommandBlock, 4U>& pins)
{
    Report out {};
    out[0] = static_cast<std::uint8_t>(Command::SetGpioOutputs);
    out[1] = 0U;
    for (int p = 0; p < 4; ++p)
    {
        const std::size_t b = 2U + static_cast<std::size_t>(p) * 4U;
        out[b]     = static_cast<std::uint8_t>(pins[static_cast<std::size_t>(p)]._alterOutput);
        out[b + 1U] = static_cast<std::uint8_t>(pins[static_cast<std::size_t>(p)]._outputValue);
        out[b + 2U] = static_cast<std::uint8_t>(pins[static_cast<std::size_t>(p)]._alterDirection);
        out[b + 3U] = static_cast<std::uint8_t>(pins[static_cast<std::size_t>(p)]._direction);
    }
    Report in {};
    if (!_sendReport(out, &in, true))
    {
        return false;
    }
    if (in[1] != 0U)
    {
        _setError(Error::ProtocolMismatch);
        return false;
    }
    return true;
}

bool Device::getGpioValues(std::array<GpioPinState, 4U>& out)
{
    Report cmd {};
    cmd[0] = static_cast<std::uint8_t>(Command::GetGpioValues);
    Report in {};
    if (!_sendReport(cmd, &in, true))
    {
        return false;
    }
    if (in[1] != 0U)
    {
        _setError(Error::ProtocolMismatch);
        return false;
    }
    for (int p = 0; p < 4; ++p)
    {
        const std::size_t vb = 2U + static_cast<std::size_t>(p) * 2U;
        parseGpioPin(in[vb], in[vb + 1U], out[static_cast<std::size_t>(p)]);
    }
    return true;
}

bool Device::setSramSettings(const std::array<std::uint8_t, 10U>& bytesAtIndex2To11)
{
    Report out {};
    out[0] = static_cast<std::uint8_t>(Command::SetSramSettings);
    out[1] = 0U;
    std::copy(bytesAtIndex2To11.begin(), bytesAtIndex2To11.end(), out.begin() + 2);
    Report in {};
    if (!_sendReport(out, &in, true))
    {
        return false;
    }
    if (in[0] != static_cast<std::uint8_t>(Command::SetSramSettings) || in[1] != 0U)
    {
        _setError(Error::ProtocolMismatch);
        return false;
    }
    return true;
}

bool Device::getSramSettings(Report& rawOut)
{
    Report out {};
    out[0] = static_cast<std::uint8_t>(Command::GetSramSettings);
    Report in {};
    if (!_sendReport(out, &in, true))
    {
        return false;
    }
    // Table 3-39: byte0 = command echo (0x61), byte1 = completion (0x00 = success)
    if (in[0] != static_cast<std::uint8_t>(Command::GetSramSettings) || in[1] != 0U)
    {
        _setError(Error::ProtocolMismatch);
        return false;
    }
    rawOut = in;
    return true;
}

bool Device::resetChip()
{
    Report out {};
    out[0] = static_cast<std::uint8_t>(Command::ResetChip);
    out[1] = ResetKeyByte1;
    out[2] = ResetKeyByte2;
    out[3] = ResetKeyByte3;
    if (!_sendReport(out, nullptr, false))
    {
        return false;
    }
    return true;
}

int Device::countDevices(std::uint16_t vid, std::uint16_t pid) const
{
    libusb_device** list = nullptr;
    const ssize_t cnt    = libusb_get_device_list(_context, &list);
    if (cnt < 0)
    {
        return 0;
    }
    int n = 0;
    for (ssize_t i = 0; i < cnt; ++i)
    {
        libusb_device_descriptor d {};
        if (libusb_get_device_descriptor(list[i], &d) == 0 && d.idVendor == vid && d.idProduct == pid)
        {
            ++n;
        }
    }
    libusb_free_device_list(list, 1);
    return n;
}

} // namespace hid
} // namespace mcp2221a
