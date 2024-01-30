### secure communication

#### encryption
- symmetric-key encryption over the channel
- key exchange (Diffie-Hellman or EDHC)
- ephemeral key for each channel

#### authentication
1. custom authtication scheme independent of Mavlink and pilot firmware
2. using Mavlink signing feature
3. using mavlink `auth` command

### encryption & authentication 1

### 问题
- 无人机硬件标准化（设备、物理接口）：需要对无人机整体的硬件内容进行标准化处理，做好飞控、板卡、传感器与数传模块的选型，使用工业化的标准物理接口（而非使用杜邦线）。
- 物理开发环境欠缺：现在对于flyvisor的开发缺少对应的硬件支撑，也就是目前没有能够构建第二台flycube的环境。
- 缺少对无人机系统具体的、细节上的认知：对于无人机系统中的部分组成要素缺少详细的认知，对于无人机实际的使用与功能需求认知不足，缺少对该内容的边界认知。（设计与影响到当前flyvisor的设计规划，可能产生的无用设计与开发投入）。
