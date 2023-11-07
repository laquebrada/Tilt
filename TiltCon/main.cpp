#include "pch.h"
#include "TiltMonitor.h"

CTiltMonitors* pTiltMonitors;

// Static callback for Bluetooth events for all Tilts (and other BT devices as well)
void OnAdvertisementReceived(BluetoothLEAdvertisementWatcher watcher, BluetoothLEAdvertisementReceivedEventArgs eventArgs)
{
    pTiltMonitors->OnAdvertisementReceived(watcher, eventArgs);
};

#define EMIT_MINUTES    15
#define EMIT_INTERVAL   (int64_t(EMIT_MINUTES) * int64_t(60) * int64_t(10000000))

int main()
{
    init_apartment();
    pTiltMonitors = new CTiltMonitors(OnAdvertisementReceived);
    cout << "Detecting Tilt(s)";
    while (!pTiltMonitors->Detect())
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));   // no tilt detected
    DateTime StartTime = clock::now();
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        std::string status;
        pTiltMonitors->UpdateDisplay(&status);
        TimeSpan tsElapsed = clock::now() - StartTime;
        int Remaining = int((EMIT_INTERVAL - tsElapsed.count()) / 10000000);
        char cBuf[100];
        sprintf_s(cBuf, ARRAYSIZE(cBuf), "Next Emit in %d Seconds.\n", Remaining);
        status.append(cBuf);
        cout << status;
        if (tsElapsed.count() > EMIT_INTERVAL)
        {
            TimeSpan Interval(EMIT_INTERVAL);
            StartTime += Interval;
            pTiltMonitors->Emit();
        }
    }
    delete pTiltMonitors;
}
