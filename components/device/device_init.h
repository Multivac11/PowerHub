#pragma once

class DeviceInit
{
   public:
    static DeviceInit& GetInstance()
    {
        static DeviceInit instance;
        return instance;
    }

    void Init();

   private:
    DeviceInit() = default;
    ~DeviceInit() = default;
};
