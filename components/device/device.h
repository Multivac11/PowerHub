#pragma once

class Device
{
   public:
    virtual ~Device() = default;

    virtual bool Probe() = 0;  // 探测设备是否存在

    virtual bool Init() = 0;  // 初始化（挂载到总线）

    virtual void Deinit() = 0;  // 反初始化
};