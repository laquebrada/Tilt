#pragma once
#include "pch.h"
#include <ctime>
#include <iomanip>
#include <cassert>

/*
* Windows based bluetooth monitor for Tilt.
*/

/*
OF - Offset in raw message. Raw message is not visible in windows.
D0 - Offset in Data 0 field.
D1 - Offset in Data 2 field.
Nx: Notes
1 - Watcher.Received(OnAdvertisementReceived)
2 - BluetoothLEAdvertisementType::NonConnectableUndirected
3 - eventArgs.BluetoothAddress()
4 - eventArgs.Advertisement().ManufacturerData(), not verified.
5 - Temperature is only valid if temperature flag is set. (Observation)
6 - Gravity is only valid if gravity flag is set (Observation)
7 - Observed value appears to be flags, not txpower. Bit 3 is Temperture valid, Bit 7 is Gravity valid.
8 - eventArgs.RawSignalStrengthInDBm()

OF Nx D0 D1
00  1       04: HCI Packet Type HCI Event
01          3E: LE Meta event
02          27: Parameter total length (39 octets)
03  2       02: LE Advertising report sub-event
04          01: Number of reports (1)
05          00: Event type connectable and scannable undirected advertising
06          00: Public address type
07  3       5A: address
08  3       09: address
09  3       9B: address
0A  3       16: address
0B  3       A3: address
0C  3       04: address
0D          1B: length of data field (27 octets)
0E          1A: length of first advertising data (AD) structure (26)
0F          FF: type of first AD structure - manufacturer specific data
10  4    00 4C: manufacturer ID - Apple iBeacon
11  4    01 00: manufacturer ID - Apple iBeacon
12       02 02: type (constant, defined by iBeacon spec)
13       03 15: length (constant, defined by iBeacon spec)
14       04 A4: device UUID
15       05 95: device UUID
16       06 BB: device UUID
17       07 10: device UUID, upper nibble is TiltID (Color)
18       08 C5: device UUID
19       09 B1: device UUID
1A       0A 4B: device UUID
1B       0B 44: device UUID
1C       0C B5: device UUID
1D       0D 12: device UUID
1E       0E 13: device UUID
1F       0F 70: device UUID
20       10 F0: device UUID
21       11 2D: device UUID
22       12 74: device UUID
23       13 DE: device UUID
24  5    14 00: major - temperature (in degrees fahrenheit)
25  5    15 44: major - temperature (in degress fahrenheit)
26  6    16 03: minor - specific gravity (x1000)
27  6    17 F8: minor - specific gravity (x1000)
28  7    18 C5: TX power in dBm
29  8       C7: RSSI in dBm
*/

// convert byte string to text in hex format.
class cvBinToHex
{
public:
    cvBinToHex(
        IN PVOID pBin,
        IN ULONG cBin,
        IN OUT std::wstring* pcsHex)
    {
        PBYTE pbBin = PBYTE(pBin);
        for (ULONG i = 0; i < cBin; i++)
        {
            wchar_t Buf[100];
            swprintf_s(Buf, ARRAYSIZE(Buf), L"%02X ", pbBin[i]);
            pcsHex->append(Buf);
        }
    };
};

// Convert DateTime to string

class DateTimeToString
{
public:
    DateTimeToString(
        IN DateTime* pDateTime, 
        IN OUT char* pcTime, 
        IN size_t cbTime)
    {
        std::time_t time = winrt::clock::to_time_t(*pDateTime);
        struct tm buf;
        errno_t result = localtime_s(&buf, &time);
        assert(result == 0);
        asctime_s(pcTime, cbTime, &buf);
        pcTime[strlen(pcTime)-1] = 0; // remove NL
    };
};

// Averaging Filter

#define FILTER_LENGTH   100
class CFilterInt
{
protected:
    ULONG m_Count;
    ULONG m_Index;
    bool m_FirstWrite;
    ULONG m_Scale;
    int m_Values[FILTER_LENGTH];
    DateTime m_Timestamps[FILTER_LENGTH];
public:
    CFilterInt(ULONG Scale)
        :
        m_Count(0),
        m_Index(0),
        m_FirstWrite(true),
        m_Scale(Scale)
    {
        Write(winrt::clock::now(), 0);
        m_FirstWrite = true;
        m_Count = 0;
    };
public:
    void Reset()
    {
        if (m_Count > 0)
        {
            DateTime timestamp = clock::now();
            int PrevIndex = FILTER_LENGTH - 1;
            if (m_Index != 0)
                PrevIndex = m_Index - 1;
            int Value = m_Values[PrevIndex];
            timestamp = m_Timestamps[PrevIndex];
            m_FirstWrite = true;
            Write(timestamp, Value);
        }
    }
    void Write(DateTime Timestamp, int Value)
    {
        if (m_FirstWrite)
        {
            m_FirstWrite = false;
            m_Index = 0;
            for (ULONG i = 0; i < FILTER_LENGTH; i++)
            {
                Write(Timestamp, Value);
            }
            m_Count = 0;
        }
        else
        {
            m_Values[m_Index] = Value;
            m_Timestamps[m_Index] = Timestamp;
            m_Index += 1;
            m_Index %= FILTER_LENGTH;
        };
        m_Count += 1;
    };
    void Read(TimeSpan* pElapsed, int* pValue, char* pcValue)
    {
        double v = Get();
        *pValue = int(v);
        sprintf_s(pcValue, 100, "%3.5f", float(v));
        DateTime StartTime = m_Timestamps[m_Index];
        DateTime EndTime = m_Timestamps[0];
        if (m_Index != 0)
            EndTime = m_Timestamps[m_Index - 1];
        *pElapsed = EndTime - StartTime;
    };
    double Get()
    {
        int64_t sum = 0;
        ULONG extent = FILTER_LENGTH;
        if (m_Count < FILTER_LENGTH)
            extent = m_Count;
        if (extent == 0)
            return 0.0;
        for (ULONG i = 0; i < extent; i++)
        {
            sum += m_Values[i];
        }
        double v = (double(sum) / double(extent)) / double(m_Scale);
        return v;
    };
    void Get(string* ps)
    {
        ULONG LastIndex = FILTER_LENGTH - 1;
        if (m_Index != 0)
            LastIndex = m_Index - 1;
        char cBuf[1000];
        double dLast = double(m_Values[LastIndex]) / double(m_Scale);
        sprintf_s(cBuf, sizeof(cBuf), "Last: %9.5f, Filtered: %9.5f", float(dLast), float(Get()));
        ps->append(cBuf);
    };
    void GetStatistics(string* ps)
    {
        ULONG LastIndex = FILTER_LENGTH - 1;
        if (m_Index != 0)
            LastIndex = m_Index - 1;
        DateTime dtNow = winrt::clock::now();
        TimeSpan elapsed = dtNow - m_Timestamps[LastIndex];
        TimeSpan elapsedTotal = dtNow - m_Timestamps[m_Index];
        double Rate = 0.0;
        if (m_Count > FILTER_LENGTH)
            Rate = (double(FILTER_LENGTH) * double(3600) * double(10000000)) / double(elapsedTotal.count());
        else
            if (m_Count > 0)
                Rate = (double(m_Count) * double(3600) * double(10000000)) / double(elapsedTotal.count());
        char cBuf[1000];
        sprintf_s(cBuf, sizeof(cBuf), "%d Samples, last updated %I64d Seconds ago, %d Samples/Hour",
            m_Count, elapsed.count() / 10000000, int(Rate));
        ps->append(cBuf);
    }
};

// Monitor a single Tilt

class CTiltMonitor
{
protected:
    int         m_TiltID;
    CFilterInt  m_Temperature;
    CFilterInt  m_Gravity;
    CFilterInt  m_txPower;
    CFilterInt  m_Signal;           // rssi, i.e., Received Signal Strength
    DateTime    m_StartTime;
    string      m_Path;
private:
    static constexpr char cr[2] = { 0x0D, 0x00 };
    static constexpr const char* m_Color[8] = { "Red", "Green", "Black", "Purple", "Orange", "Blue", "Yellow", "Pink" };
public:
    void GetTemperature(std::string *ps)
    {
        ps->append("Temperature: ");
        m_Temperature.Get(ps);
    };
    void GetGravity(std::string* ps)
    {
        ps->append("Gravity    : ");
        m_Gravity.Get(ps);
    };
    void GetTxPower(std::string* ps)
    {
        ps->append("TxPower    : ");
        m_txPower.Get(ps);
    };
    void GetSignal(std::string* ps)
    {
        ps->append("Signal     : ");
        m_Signal.Get(ps);
    };
    void UpdateDisplay(string* ps)
    {
        if (m_TiltID == -1)
            return;
        char cBuf[100];
        sprintf_s(cBuf, sizeof(cBuf), "%s: ", m_Color[m_TiltID-1]);
        ps->append(cBuf);
        m_Temperature.GetStatistics(ps);
        ps->append("\n  ");
        GetTemperature(ps);
        ps->append("\n  ");
        GetGravity(ps);
        ps->append("\n  ");
        GetTxPower(ps);
        ps->append("\n  ");
        GetSignal(ps);
        ps->append("\n");
    };
public:
    CTiltMonitor()
        :
        m_TiltID(-1),
        m_Temperature(1),
        m_Gravity(1000),
        m_Signal(1),
        m_txPower(1),
        m_StartTime(winrt::clock::now())
    {
    };
    void OnAdvertisementReceived(
        int TiltID,
        DateTime timestamp,
        int16_t rssi,
        int txPower,
        int temperature,
        int gravity)
    {
        if (m_TiltID == -1)
            Begin(TiltID);
        m_Temperature.Write(timestamp, temperature);
        m_Gravity.Write(timestamp, gravity);
        m_txPower.Write(timestamp, txPower);
        m_Signal.Write(timestamp, int(rssi));
    };
    bool Detect()
    {
        if (m_TiltID == -1)
            return false;
        return true;
    };
    void Begin(int TiltID)
    {
        m_TiltID = TiltID;
        char cTiltLog[1000];
        sprintf_s(cTiltLog, sizeof(cTiltLog), "TiltLog_%s.txt", m_Color[m_TiltID-1]);
        m_Path.append(cTiltLog);
        FILE* pFile = NULL;
        fopen_s(&pFile, m_Path.data(), "r");
        if (pFile)
            fclose(pFile);
        else
        {   // file does not exist, start with the header.
            fopen_s(&pFile, m_Path.data(), "w");
            if (pFile)
            {
                char cHeader[1000];
                sprintf_s(cHeader, ARRAYSIZE(cHeader), "Temperature,Gravity,TxPower,Signal,Timestamp,DateTime\n");
                fwrite(cHeader, 1, strlen(cHeader), pFile);
                fclose(pFile);
            }
        }
    }
    void Emit()
    {
        if (m_TiltID == -1)
            return;
        char cTimestamp[100];
        DateTime dtNow = winrt::clock::now();
        DateTimeToString dts(&dtNow, cTimestamp, ARRAYSIZE(cTimestamp));
        char cEntry[1000];
        sprintf_s(cEntry, sizeof(cEntry), "%f,%f,%f,%f,%I64d,\"%s\"\n", float(m_Temperature.Get()), float(m_Gravity.Get()), float(m_txPower.Get()), float(m_Signal.Get()), dtNow.time_since_epoch().count(), cTimestamp);
        FILE* pFile = NULL;
        fopen_s(&pFile, m_Path.data(), "a+");
        if (pFile)
        {
            fwrite(cEntry, 1, strlen(cEntry), pFile);
            fclose(pFile);
        }
        // Re-start integration after each emit.
        m_Temperature.Reset();
        m_Gravity.Reset();
        m_txPower.Reset();
        m_Signal.Reset();
    }
};

// Aggregate monitoring class for all Tilts

class CTiltMonitors
{
public:
    CTiltMonitor m_TiltMonitors[8]; // one monitor per tilt
    BluetoothLEAdvertisementWatcher m_Watcher;
    BluetoothSignalStrengthFilter   m_ssFilter;
public:
    // handler parameter is the static callback for all BT events. The hander invokes our OnAdvertisementReceived method.
    CTiltMonitors(winrt::Windows::Foundation::TypedEventHandler<winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementWatcher, winrt::Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementReceivedEventArgs> const& handler)
    {
        m_ssFilter.InRangeThresholdInDBm(-90);
        m_ssFilter.OutOfRangeThresholdInDBm(-99);
        TimeSpan intervalOutOfRange(10000000);
        intervalOutOfRange = m_Watcher.MaxOutOfRangeTimeout();
        m_ssFilter.OutOfRangeTimeout(intervalOutOfRange);
        TimeSpan intervalSampling(10000000);
        //intervalSampling = Watcher.MinSamplingInterval();
        m_ssFilter.SamplingInterval(intervalSampling);

        m_Watcher.SignalStrengthFilter(m_ssFilter);
        m_Watcher.AllowExtendedAdvertisements(true);
        m_Watcher.Received(handler);

        m_Watcher.Start();
        m_Watcher.ScanningMode(BluetoothLEScanningMode::None);  // crashes if executed before Start.
    };
    ~CTiltMonitors()
    {
        m_Watcher.Stop();
    };
    bool Detect()
    {
        bool fDetect = false;
        for (ULONG i = 0; i < ARRAYSIZE(m_TiltMonitors); i++)
        {
            fDetect |= m_TiltMonitors[i].Detect();
        }
        return fDetect;
    }
    // Invoked on any BT event for any BT device.
    void OnAdvertisementReceived(BluetoothLEAdvertisementWatcher watcher, BluetoothLEAdvertisementReceivedEventArgs eventArgs)
    {
        // The type of advertisement
        BluetoothLEAdvertisementType advertisementType = eventArgs.AdvertisementType();
        if (advertisementType != BluetoothLEAdvertisementType::NonConnectableUndirected)
        {
            return; // not a beacon
        }
        // The received signal strength indicator (RSSI)
        int16_t rssi = eventArgs.RawSignalStrengthInDBm();
        if (rssi == -127)
        {
            return; // signal lost
        }
        // Tilt uses MfgID 0x4c (Apple). Some other BT devices do as well.
        IVectorView<BluetoothLEManufacturerData> IReadOnlyListMfgData = eventArgs.Advertisement().GetManufacturerDataByCompanyId(0x004C);
        if (IReadOnlyListMfgData.Size() == 0)
        {
            return; // not Apple Mfg ID
        }
        // MfgID is apple (Tilt)
        IVector DataSections = eventArgs.Advertisement().DataSections();
        int payloadIndex = 0;
        int tiltID = 99;
        int temperature = 9999;
        int gravity = 9999;
        int txPower = 0;
        for (auto i = DataSections.begin(); i < DataSections.end(); i++)
        {
            BluetoothLEAdvertisementDataSection DataSection = *i;
            IBuffer Data = DataSection.Data();
            BYTE payload[256];
            if (Data.Length() > sizeof(payload))
                return; // not correct data length
            memcpy(payload, Data.data(), Data.Length());
            if (payloadIndex == 1)
            {
                if (Data.Length() != 25)
                    return;
                if (payload[0] != 0x4C)
                    return;
                if (payload[1] != 0x00)
                    return;
                if (payload[2] != 0x02)
                    return;
                if (payload[3] != 0x15)
                    return;
                if (payload[4] != 0xA4)
                    return;
                if (payload[5] != 0x95)
                    return;
                if (payload[6] != 0xBB)
                    return;
                tiltID = payload[7] >> 4;
                if (payload[8] != 0xC5)
                    return;
                if (payload[9] != 0xB1)
                    return;
                if (payload[10] != 0x4B)
                    return;
                if (payload[11] != 0x44)
                    return;
                if (payload[12] != 0xB5)
                    return;
                if (payload[13] != 0x12)
                    return;
                if (payload[14] != 0x13)
                    return;
                if (payload[15] != 0x70)
                    return;
                if (payload[16] != 0xF0)
                    return;
                if (payload[17] != 0x2D)
                    return;
                if (payload[18] != 0x74)
                    return;
                if (payload[19] != 0xDE)
                    return;
                temperature = payload[20];
                temperature *= 256;
                temperature += payload[21];
                gravity = payload[22];
                gravity *= 256;
                gravity += payload[23];
                txPower = int(payload[24]);
            }
            payloadIndex += 1;
        }
        if (payloadIndex != 2)
        {
            return; // missing second payload field
        }

        if (tiltID >= ARRAYSIZE(m_TiltMonitors))
            return; // Invalid TiltID
        m_TiltMonitors[tiltID].OnAdvertisementReceived(
            tiltID,
            eventArgs.Timestamp(),
            rssi,
            txPower,
            temperature,
            gravity);
    };
    void UpdateDisplay(std::string* pStatus)
    {
        pStatus->append("\033[H\033[2J");
        for (ULONG Tilt = 0; Tilt < ARRAYSIZE(m_TiltMonitors); Tilt++)
        {
            m_TiltMonitors[Tilt].UpdateDisplay(pStatus);
        }
    };
    void Emit()
    {
        for (ULONG Tilt = 0; Tilt < ARRAYSIZE(m_TiltMonitors); Tilt++)
        {
            m_TiltMonitors[Tilt].Emit();
        }
    };
};