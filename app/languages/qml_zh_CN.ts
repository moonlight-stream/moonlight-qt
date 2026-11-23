<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="zh_CN">
<context>
    <name>AppView</name>
    <message><source>Connect to Desktop</source><translation>连接桌面</translation></message>
    <message><source>Reconnect to Desktop</source><translation>重新连接桌面</translation></message>
    <message><source>End Desktop Session</source><translation>结束桌面会话</translation></message>
    <message>
        <location filename="../gui/AppView.qml" line="143"/>
        <location filename="../gui/AppView.qml" line="299"/>
        <source>Resume Game</source>
        <translation>继续游戏</translation>
    </message>
    <message>
        <location filename="../gui/AppView.qml" line="169"/>
        <location filename="../gui/AppView.qml" line="303"/>
        <source>Quit Game</source>
        <translation>退出游戏</translation>
    </message>
    <message>
        <location filename="../gui/AppView.qml" line="299"/>
        <source>Launch Game</source>
        <translation>启动游戏</translation>
    </message>
    <message>
        <location filename="../gui/AppView.qml" line="310"/>
        <source>Direct Launch</source>
        <translation>直接启动</translation>
    </message>
    <message>
        <location filename="../gui/AppView.qml" line="314"/>
        <source>Launch this app immediately when the host is selected, bypassing the app selection grid.</source>
        <translation>选择目标计算机后立即启动此应用程序，绕过应用程序选择网格。</translation>
    </message>
    <message>
        <location filename="../gui/AppView.qml" line="322"/>
        <source>Hide Game</source>
        <translation>隐藏游戏</translation>
    </message>
    <message>
        <location filename="../gui/AppView.qml" line="326"/>
        <source>Hide this game from the app grid. To access hidden games, right-click on the host and choose %1.</source>
        <translation>从应用程序网格中隐藏此游戏。要访问隐藏的游戏，请右键单击目标计算机并选择 %1。</translation>
    </message>
    <message>
        <location filename="../gui/AppView.qml" line="326"/>
        <source>View All Apps</source>
        <translation>查看所有应用程序</translation>
    </message>
    <message>
        <location filename="../gui/AppView.qml" line="341"/>
        <source>This computer doesn&apos;t seem to have any applications or some applications are hidden</source>
        <translation>这台计算机上似乎没有任何应用程序，或者有些应用程序已被隐藏</translation>
    </message>
    <message>
        <location filename="../gui/AppView.qml" line="354"/>
        <source>Are you sure you want to quit %1? Any unsaved progress will be lost.</source>
        <translation>你确定要退出 %1 吗？所有未保存的游戏进度都将丢失。</translation>
    </message>
</context>
<context>
    <name>CliPair</name>
    <message>
        <location filename="../gui/CliPair.qml" line="8"/>
        <source>Establishing connection to PC...</source>
        <translation>正在建立与目标计算机的连接...</translation>
    </message>
    <message>
        <location filename="../gui/CliPair.qml" line="12"/>
        <source>Pairing... Please enter &apos;%1&apos; on %2.</source>
        <translation>配对中……请在 %2 输入 “%1”。</translation>
    </message>
    <message>
        <location filename="../gui/CliPair.qml" line="81"/>
        <source>Pairing completed successfully</source>
        <translation>配对成功</translation>
    </message>
</context>
<context>
    <name>CliQuitStreamSegue</name>
    <message>
        <location filename="../gui/CliQuitStreamSegue.qml" line="9"/>
        <source>Establishing connection to PC...</source>
        <translation>正在建立与目标计算机的连接...</translation>
    </message>
    <message>
        <location filename="../gui/CliQuitStreamSegue.qml" line="13"/>
        <source>Quitting app...</source>
        <translation>应用程序退出中...</translation>
    </message>
</context>
<context>
    <name>CliStartStreamSegue</name>
    <message><source>Waiting for confirmation...</source><translation>正在等待确认切换应用…</translation></message>
    <message>
        <location filename="../gui/CliStartStreamSegue.qml" line="8"/>
        <source>Establishing connection to PC...</source>
        <translation>正在建立与目标计算机的连接...</translation>
    </message>
    <message>
        <location filename="../gui/CliStartStreamSegue.qml" line="12"/>
        <source>Loading app list...</source>
        <translation>应用程序列表加载中...</translation>
    </message>
    <message>
        <location filename="../gui/CliStartStreamSegue.qml" line="78"/>
        <source>Are you sure you want to quit %1? Any unsaved progress will be lost.</source>
        <translation>你确定要退出 %1 吗？任何未保存的进度都将丢失。</translation>
    </message>
</context>
<context>
    <name>ComputerModel</name>
    <message><source>Connect to a paired, online host to transfer files.</source><translation>请选择在线且已配对的主机来传输文件。</translation></message>
    <message><source>Wake packet sent to %1.</source><translation>已向 %1 发送唤醒包。</translation></message>
    <message><source>Could not send a wake packet to %1.</source><translation>未能向 %1 发送唤醒包。</translation></message>
    <message><source>Restart requested for %1.</source><translation>%1 已接受重启请求。</translation></message>
    <message><source>Shutdown requested for %1.</source><translation>%1 已接受关机请求。</translation></message>
    <message><source>Power request to %1: %2</source><translation>向 %1 发送电源请求失败：%2</translation></message>
    <message><source>Power controls are unavailable for this host.</source><translation>此主机当前不支持远程电源操作。</translation></message>
    <message><source>File transfer needs an active remote control window.</source><translation>文件传输需要已连接的远程控制窗口。</translation></message>
    <message><source>No desktop is available for file transfer.</source><translation>没有可用于文件传输的远程桌面。</translation></message>
    <message><source>Could not open the file transfer window.</source><translation>无法打开文件传输窗口。</translation></message>
    <message><source>File transfer requires Windows.</source><translation>文件传输需要 Windows。</translation></message>
    <message>
        <location filename="../gui/computermodel.cpp" line="50"/>
        <source>Online</source>
        <translation>在线</translation>
    </message>
    <message>
        <location filename="../gui/computermodel.cpp" line="53"/>
        <source>Offline</source>
        <translation>离线</translation>
    </message>
    <message>
        <location filename="../gui/computermodel.cpp" line="56"/>
        <location filename="../gui/computermodel.cpp" line="68"/>
        <location filename="../gui/computermodel.cpp" line="80"/>
        <location filename="../gui/computermodel.cpp" line="82"/>
        <location filename="../gui/computermodel.cpp" line="83"/>
        <source>Unknown</source>
        <translation>未知</translation>
    </message>
    <message>
        <location filename="../gui/computermodel.cpp" line="62"/>
        <source>Paired</source>
        <translation>已配对</translation>
    </message>
    <message>
        <location filename="../gui/computermodel.cpp" line="65"/>
        <source>Unpaired</source>
        <translation>未配对</translation>
    </message>
    <message>
        <location filename="../gui/computermodel.cpp" line="72"/>
        <source>Name: %1</source>
        <translation>名称: %1</translation>
    </message>
    <message>
        <location filename="../gui/computermodel.cpp" line="73"/>
        <source>Status: %1</source>
        <translation>状态: %1</translation>
    </message>
    <message>
        <location filename="../gui/computermodel.cpp" line="74"/>
        <source>Active Address: %1</source>
        <translation>活动地址: %1</translation>
    </message>
    <message>
        <location filename="../gui/computermodel.cpp" line="75"/>
        <source>UUID: %1</source>
        <translation>UUID: %1</translation>
    </message>
    <message>
        <location filename="../gui/computermodel.cpp" line="76"/>
        <source>Local Address: %1</source>
        <translation>本地地址: %1</translation>
    </message>
    <message>
        <location filename="../gui/computermodel.cpp" line="77"/>
        <source>Remote Address: %1</source>
        <translation>远程地址: %1</translation>
    </message>
    <message>
        <location filename="../gui/computermodel.cpp" line="78"/>
        <source>IPv6 Address: %1</source>
        <translation>IPv6 地址: %1</translation>
    </message>
    <message>
        <location filename="../gui/computermodel.cpp" line="79"/>
        <source>Manual Address: %1</source>
        <translation>手动地址: %1</translation>
    </message>
    <message>
        <location filename="../gui/computermodel.cpp" line="80"/>
        <source>MAC Address: %1</source>
        <translation>MAC 地址: %1</translation>
    </message>
    <message>
        <location filename="../gui/computermodel.cpp" line="81"/>
        <source>Pair State: %1</source>
        <translation>配对状态: %1</translation>
    </message>
    <message>
        <location filename="../gui/computermodel.cpp" line="82"/>
        <source>Running Game ID: %1</source>
        <translation>正在运行的游戏 ID: %1</translation>
    </message>
    <message>
        <location filename="../gui/computermodel.cpp" line="83"/>
        <source>HTTPS Port: %1</source>
        <translation>HTTPS 端口: %1</translation>
    </message>
</context>
<context>
    <name>GamepadMapper</name>
    <message>
        <location filename="../gui/GamepadMapper.qml" line="4"/>
        <source>Gamepad Mapping</source>
        <translation>手柄按键映射</translation>
    </message>
</context>
<context>
    <name>NvHTTP</name>
    <message><source>A paired HTTPS connection is required.</source><translation>需要已配对的 HTTPS 连接。</translation></message>
    <message><source>The host did not accept the power request.</source><translation>主机未接受电源操作请求。</translation></message>
    <message>
        <location filename="../backend/nvhttp.cpp" line="381"/>
        <source>Missing audio capture device. Reinstalling GeForce Experience should resolve this error.</source>
        <translation>缺少音频捕获设备。重新安装 GeForce Experience 可以解决此错误。</translation>
    </message>
</context>
<context>
    <name>PcView</name>
    <message><source>View only</source><translation>观看模式</translation></message>
    <message><source>More tools</source><translation>更多工具</translation></message>
    <message><source>Restart</source><translation>重启</translation></message>
    <message><source>Shut down</source><translation>关机</translation></message>
    <message><source>Show viewing window</source><translation>显示观看窗口</translation></message>
    <message><source>Viewing</source><translation>正在观看</translation></message>
    <message><source>The desktop is unavailable for viewing on %1.</source><translation>暂时无法观看 %1 的桌面。</translation></message>
    <message><source>Restart %1? Anyone using this device could lose unsaved work.</source><translation>确定要重启 %1 吗？正在使用这台设备的人可能会丢失未保存的工作。</translation></message>
    <message><source>Shut down %1? Anyone using this device could lose unsaved work.</source><translation>确定要关闭 %1 吗？正在使用这台设备的人可能会丢失未保存的工作。</translation></message>
    <message><source>Quick launch</source><translation>快速启动</translation></message>
    <message><source>Show hidden apps</source><translation>显示隐藏的应用</translation></message>
    <message><source>Options for %1</source><translation>%1 的应用选项</translation></message>
    <message><source>Unpaired</source><translation>未配对</translation></message>
    <message><source>Devices</source><translation>设备</translation></message>
    <message><source>My devices</source><translation>我的设备</translation></message>
    <message><source>Unpaired devices</source><translation>未配对设备</translation></message>
    <message><source>Searching</source><translation>检测中</translation></message>
    <message><source>Device options</source><translation>设备选项</translation></message>
    <message><source>Add device</source><translation>添加设备</translation></message>
    <message><source>Pair device</source><translation>配对设备</translation></message>
    <message><source>Enter desktop</source><translation>进入桌面</translation></message>
    <message><source>Connecting…</source><translation>正在连接…</translation></message>
    <message><source>Remote control active</source><translation>远控中</translation></message>
    <message><source>Show remote control window</source><translation>显示远控窗口</translation></message>
    <message>
        <source>No desktop or direct-launch app is available on this device.</source>
        <translation>此设备没有可连接的桌面或设为直接启动的应用。</translation>
    </message>
    <message>
        <location filename="../gui/PcView.qml" line="21"/>
        <source>Computers</source>
        <translation>计算机列表</translation>
    </message>
    <message>
        <location filename="../gui/PcView.qml" line="63"/>
        <source>Unable to connect to the specified PC.</source>
        <translation>无法连接到目标计算机。</translation>
    </message>
    <message>
        <location filename="../gui/PcView.qml" line="66"/>
        <source>This PC&apos;s Internet connection is blocking Moonlight. Streaming over the Internet may not work while connected to this network.</source>
        <translation>此计算机的互联网连接被阻塞。当连接到此网络时，互联网上的流式传输可能无法工作。</translation>
    </message>
    <message>
        <location filename="../gui/PcView.qml" line="69"/>
        <source>Click the Help button for possible solutions.</source>
        <translation>单击帮助按钮以获取可能的解决方案。</translation>
    </message>
    <message>
        <location filename="../gui/PcView.qml" line="99"/>
        <source>Searching for compatible hosts on your local network...</source>
        <translation>在你的本地网络中搜索兼容的主机...</translation>
    </message>
    <message>
        <location filename="../gui/PcView.qml" line="100"/>
        <source>Automatic PC discovery is disabled. Add your PC manually.</source>
        <translation>已禁用计算机自动发现。需要手动添加计算机。</translation>
    </message>
    <message>
        <location filename="../gui/PcView.qml" line="170"/>
        <source>PC Status: %1</source>
        <translation>计算机状态: %1</translation>
    </message>
    <message>
        <location filename="../gui/PcView.qml" line="170"/>
        <source>Online</source>
        <translation>在线</translation>
    </message>
    <message>
        <location filename="../gui/PcView.qml" line="170"/>
        <source>Offline</source>
        <translation>离线</translation>
    </message>
    <message>
        <location filename="../gui/PcView.qml" line="175"/>
        <source>View All Apps</source>
        <translation>查看所有应用程序</translation>
    </message>
    <message>
        <location filename="../gui/PcView.qml" line="184"/>
        <source>Wake PC</source>
        <translation>唤醒目标计算机</translation>
    </message>
    <message>
        <location filename="../gui/PcView.qml" line="189"/>
        <source>Test Network</source>
        <translation>网络测试</translation>
    </message>
    <message>
        <location filename="../gui/PcView.qml" line="197"/>
        <source>Rename PC</source>
        <translation>重命名计算机</translation>
    </message>
    <message>
        <location filename="../gui/PcView.qml" line="205"/>
        <source>Remove device</source>
        <translation>移除设备</translation>
    </message>
    <message>
        <location filename="../gui/PcView.qml" line="213"/>
        <source>View Details</source>
        <translation>查看详情</translation>
    </message>
    <message>
        <location filename="../gui/PcView.qml" line="225"/>
        <source>The version of GeForce Experience on %1 is not supported by this build of Moonlight. You must update Moonlight to stream from %1.</source>
        <translation>此版本 Moonlight 不支持 %1 版本的 GeForce Experience，请更新 Moonlight 以支持从 %1 串流。</translation>
    </message>
    <message>
        <location filename="../gui/PcView.qml" line="297"/>
        <source>Please enter %1 on your host PC. This dialog will close when pairing is completed.</source>
        <translation>请在你的主机上输入 %1 。配对完成后，此对话框将关闭。</translation>
    </message>
    <message>
        <location filename="../gui/PcView.qml" line="298"/>
        <source>If your host PC is running Sunshine, navigate to the Sunshine web UI to enter the PIN.</source>
        <translation>如果你的主机正在运行 Sunshine，那请前往Sunshine的网页界面输入PIN码。</translation>
    </message>
    <message>
        <location filename="../gui/PcView.qml" line="310"/>
        <source>Remove “%1” from your device list?</source>
        <translation>要从设备列表中移除“%1”吗？</translation>
    </message>
    <message>
        <source>Cancel</source>
        <translation>取消</translation>
    </message>
    <message>
        <location filename="../gui/PcView.qml" line="324"/>
        <source>This may take a few seconds…</source>
        <translation>这可能需要几秒钟……</translation>
    </message>
    <message>
        <location filename="../gui/PcView.qml" line="324"/>
        <source>Moonlight is testing your network connection to determine if any required ports are blocked.</source>
        <translation>Moonlight 正在测试你的网络连接来确认是否有需要的端口被屏蔽。</translation>
    </message>
    <message>
        <location filename="../gui/PcView.qml" line="331"/>
        <source>The network test could not be performed because none of Moonlight&apos;s connection testing servers were reachable from this PC. Check your Internet connection or try again later.</source>
        <translation>无法执行网络测试，因为此计算机无法访问 Moonlight 的连接测试服务器。请检查你的互联网连接或稍后再试。</translation>
    </message>
    <message>
        <location filename="../gui/PcView.qml" line="335"/>
        <source>This network does not appear to be blocking Moonlight. If you still have trouble connecting, check your PC&apos;s firewall settings.</source>
        <translation>此网络似乎没有阻止 Moonlight。如果仍然无法连接，请检查计算机的防火墙设置。</translation>
    </message>
    <message>
        <location filename="../gui/PcView.qml" line="335"/>
        <source>If you are trying to stream over the Internet, install the Moonlight Internet Hosting Tool on your gaming PC and run the included Internet Streaming Tester to check your gaming PC&apos;s Internet connection.</source>
        <translation>如果你想通过互联网进行流式传输，请在你的目标计算机上安装 Moonlight Internet Hosting 工具，并运行包含的互联网流式传输测试程序来检查你目标计算机的互联网连接。</translation>
    </message>
    <message>
        <location filename="../gui/PcView.qml" line="339"/>
        <source>Your PC&apos;s current network connection seems to be blocking Moonlight. Streaming over the Internet may not work while connected to this network.</source>
        <translation>此计算机当前的网络连接似乎被阻塞了。当连接到此网络时，互联网上的流式传输可能无法工作。</translation>
    </message>
    <message>
        <location filename="../gui/PcView.qml" line="339"/>
        <source>The following network ports were blocked:</source>
        <translation>下列网络端口被阻止:</translation>
    </message>
    <message>
        <location filename="../gui/PcView.qml" line="351"/>
        <source>Enter the new name for this PC:</source>
        <translation>输入这台计算机的新名称:</translation>
    </message>
</context>
<context>
    <name>PendingPairingTask</name>
    <message>
        <location filename="../backend/computermanager.cpp" line="604"/>
        <source>The PIN from the PC didn&apos;t match. Please try again.</source>
        <translation>来自电脑的密码不匹配。请再试一次。</translation>
    </message>
    <message>
        <location filename="../backend/computermanager.cpp" line="608"/>
        <source>You cannot pair while a previous session is still running on the host PC. Quit any running games or reboot the host PC, then try pairing again.</source>
        <translation>当主机 PC 上仍在运行之前的会话时，你无法配对。 退出任何正在运行的游戏或重新启动主机 PC，然后再次尝试配对。</translation>
    </message>
    <message>
        <location filename="../backend/computermanager.cpp" line="611"/>
        <source>Pairing failed. Please try again.</source>
        <translation>配对失败，请重试。</translation>
    </message>
    <message>
        <location filename="../backend/computermanager.cpp" line="615"/>
        <source>Another pairing attempt is already in progress.</source>
        <translation>另一个配对请求正在进程中。</translation>
    </message>
    <message>
        <location filename="../backend/computermanager.cpp" line="625"/>
        <source>GeForce Experience returned error: %1</source>
        <translation>GeForce Experience 返回错误: %1</translation>
    </message>
</context>
<context>
    <name>ComputerManager</name>
    <message><source>The host went offline.</source><translation>主机已离线。</translation></message>
    <message><source>The host was removed.</source><translation>主机已移除。</translation></message>
</context>
<context>
    <name>PendingQuitTask</name>
    <message>
        <location filename="../backend/computermanager.cpp" line="675"/>
        <source>The running game wasn&apos;t started by this PC. You must quit the game on the host PC manually or use the device that originally started the game.</source>
        <translation>正在运行的游戏不是由这台计算机启动的。你必须在目标计算机上手动退出游戏或使用最初启动游戏的设备。</translation>
    </message>
</context>
<context>
    <name>QObject</name>
    <message><source>Viewing mode requires the desktop to be available.</source><translation>观看模式需要主机桌面可用。</translation></message>
    <message>
        <location filename="../cli/pair.cpp" line="80"/>
        <source>%1 is already paired</source>
        <translation>%1 已被配对</translation>
    </message>
    <message>
        <location filename="../cli/pair.cpp" line="109"/>
        <location filename="../cli/quitstream.cpp" line="76"/>
        <location filename="../cli/startstream.cpp" line="133"/>
        <source>Failed to connect to %1</source>
        <translation>无法连接到 %1</translation>
    </message>
    <message>
        <location filename="../cli/quitstream.cpp" line="88"/>
        <location filename="../cli/startstream.cpp" line="89"/>
        <source>Computer %1 has not been paired. Please open Moonlight to pair before streaming.</source>
        <translation>计算机 %1 尚未配对。</translation>
    </message>
    <message>
        <location filename="../cli/quitstream.cpp" line="102"/>
        <location filename="../cli/startstream.cpp" line="126"/>
        <source>Quitting app failed, reason: %1</source>
        <translation>退出应用程序失败，原因: %1</translation>
    </message>
    <message>
        <location filename="../cli/startstream.cpp" line="137"/>
        <source>Failed to find application %1</source>
        <translation>找不到应用程序 %1</translation>
    </message>
    <message>
        <location filename="../cli/listapps.cpp" line="108"/>
        <source>Computer %1 has not been paired. Please open Moonlight to pair before retrieving games list.</source>
        <translation>电脑 %1 未配对，请在请求游戏列表前使用 Moonlight 配对。</translation>
    </message>
</context>
<context>
    <name>QPlatformTheme</name>
    <message>
        <location filename="../main.cpp" line="867"/>
        <source>&amp;Yes</source>
        <translation>是(&amp;Y)</translation>
    </message>
    <message>
        <location filename="../main.cpp" line="868"/>
        <source>&amp;No</source>
        <translation>否(&amp;N)</translation>
    </message>
    <message>
        <location filename="../main.cpp" line="869"/>
        <source>OK</source>
        <translation>确定</translation>
    </message>
    <message>
        <location filename="../main.cpp" line="870"/>
        <source>Help</source>
        <translation>帮助</translation>
    </message>
    <message>
        <location filename="../main.cpp" line="871"/>
        <source>Cancel</source>
        <translation>取消</translation>
    </message>
</context>
<context>
    <name>QuitSegue</name>
    <message>
        <location filename="../gui/QuitSegue.qml" line="13"/>
        <source>Quitting %1...</source>
        <translation>正在退出 %1...</translation>
    </message>
</context>
<context>
    <name>ClipboardTransfer</name>
    <message><source>Receiving files</source><translation>正在接收文件</translation></message>
    <message><source>Files saved</source><translation>文件已保存</translation></message>
    <message><source>Transfer cancelled</source><translation>传输已取消</translation></message>
    <message><source>Preparing files (%1)</source><translation>正在准备文件（共 %1 个）</translation></message>
    <message><source>Resuming %1 of %2</source><translation>正在续传第 %1 个文件，共 %2 个</translation></message>
    <message><source>Speed —</source><translation>速度 —</translation></message>
    <message><source>Speed %1/s</source><translation>速度 %1/s</translation></message>
    <message><source>Uploading %1 of %2</source><translation>正在上传第 %1 个文件，共 %2 个</translation></message>
    <message><source>Waiting for host</source><translation>等待主机确认</translation></message>
    <message><source>Downloading %1 of %2</source><translation>正在下载第 %1 个文件，共 %2 个</translation></message>
    <message><source>Preparing local clipboard</source><translation>正在准备本机剪贴板</translation></message>
    <message><source>Ready to paste on this PC</source><translation>可以在本机粘贴了</translation></message>
    <message><source>Not enough local disk space</source><translation>本机磁盘空间不足</translation></message>
    <message><source>File download failed</source><translation>文件下载失败</translation></message>
    <message><source>Queued</source><translation>等待中</translation></message>
    <message><source>Cancel (Esc)</source><translation>取消 (Esc)</translation></message>
    <message><source>Cancel</source><translation>取消</translation></message>
    <message><source>Dismiss</source><translation>移除</translation></message>
    <message><source>Paste here</source><translation>粘贴到这里</translation></message>
    <message><source>Choose where to paste</source><translation>请选择粘贴位置</translation></message>
    <message><source>Waiting for paste</source><translation>等待粘贴</translation></message>
    <message><source>Transfer failed</source><translation>传输失败</translation></message>
    <message><source>Connection or source file unavailable</source><translation>连接中断或源文件无法读取</translation></message>
    <message><source>Transfer queue is full</source><translation>传输队列已满</translation></message>
    <message><source>Cannot open the selected files</source><translation>无法打开所选文件</translation></message>
    <message><source>Remote clipboard changed</source><translation>远端剪贴板已更改</translation></message>
    <message><source>More tasks (%1-%2 of %3)</source><translation>查看更多任务（%1–%2 / %3）</translation></message>
</context>
<context>
    <name>Session</name>
    <message><source>%1 — View only</source><translation>%1 — 观看模式</translation></message>
    <message><source>Uploading files %1%</source><translation>正在上传文件 %1%</translation></message>
    <message><source>Unable to open the file transfer window.</source><translation>无法打开文件传输窗口。</translation></message>
    <message><source>Checking the network route...</source><translation>正在检查到主机的网络路径…</translation></message>
    <message><source>Setting up networking...</source><translation>正在初始化网络…</translation></message>
    <message><source>Resolving the host address...</source><translation>正在解析主机地址…</translation></message>
    <message><source>Preparing the audio stream...</source><translation>正在准备音频流…</translation></message>
    <message><source>Negotiating stream settings (RTSP)...</source><translation>正在协商串流参数（RTSP）…</translation></message>
    <message><source>Preparing the control channel...</source><translation>正在准备控制通道…</translation></message>
    <message><source>Preparing the video stream...</source><translation>正在准备视频流…</translation></message>
    <message><source>Preparing the input channel...</source><translation>正在准备输入通道…</translation></message>
    <message><source>Connecting the control channel...</source><translation>正在连接控制通道…</translation></message>
    <message><source>Connecting the video stream...</source><translation>正在连接视频通道…</translation></message>
    <message><source>Connecting the audio stream...</source><translation>正在连接音频通道…</translation></message>
    <message><source>Connecting the input channel...</source><translation>正在连接输入通道…</translation></message>
    <message><source>Checking network ports...</source><translation>正在检查网络端口…</translation></message>
    <message><source>Stats</source><translation>统计</translation></message>
    <message><source>Fullscreen</source><translation>全屏</translation></message>
    <message>
        <source>Frame %1 ms</source>
        <translation>帧延迟 %1 ms</translation>
    </message>
    <message>
        <source>NIC ↑%1 ↓%2 kB/s</source>
        <translation>网卡 ↑%1 ↓%2 kB/s</translation>
    </message>
    <message>
        <source>Exit fullscreen</source>
        <translation>退出全屏</translation>
    </message>
    <message>
        <source>Disconnect</source>
        <translation>退出远控</translation>
    </message>
    <message>
        <source>Poor connection</source>
        <translation>连接不佳</translation>
    </message>
    <message>
        <source>Connection stable</source>
        <translation>连接稳定</translation>
    </message>
    <message>
        <source> · RTT %1 ms</source>
        <translation> · 网络延迟 %1 ms</translation>
    </message>
    <message>
        <location filename="../streaming/session.cpp" line="104"/>
        <source>No video received from host.</source>
        <translation>没有从目标计算机接收到视频画面。</translation>
    </message>
    <message>
        <location filename="../streaming/session.cpp" line="105"/>
        <source>Check your firewall and port forwarding rules for port(s): %1</source>
        <translation>检查防火墙和端口转发规则: %1</translation>
    </message>
    <message>
        <location filename="../streaming/session.cpp" line="110"/>
        <source>Your network connection isn&apos;t performing well. Reduce your video bitrate setting or try a faster connection.</source>
        <translation>网络连接性能不佳。请降低视频比特率或尝试更快的网络连接。</translation>
    </message>
    <message>
        <location filename="../streaming/session.cpp" line="116"/>
        <source>Something went wrong on your host PC when starting the stream.</source>
        <translation>目标计算机启动流式传输时出现问题。</translation>
    </message>
    <message>
        <location filename="../streaming/session.cpp" line="117"/>
        <source>Make sure you don&apos;t have any DRM-protected content open on your host PC. You can also try restarting your host PC.</source>
        <translation>确保目标计算机上没有打开任何受 DRM 保护的内容。你也可以尝试重新启动目标计算机。</translation>
    </message>
    <message>
        <location filename="../streaming/session.cpp" line="122"/>
        <source>The host PC reported a fatal video encoding error.</source>
        <translation>主机 PC 报告了致命的视频编码错误。</translation>
    </message>
    <message>
        <location filename="../streaming/session.cpp" line="123"/>
        <source>Try disabling HDR mode, changing the streaming resolution, or changing your host PC&apos;s display resolution.</source>
        <translation>尝试禁用 HDR 模式、更改流分辨率或更改主机 PC 的显示分辨率。</translation>
    </message>
    <message>
        <location filename="../streaming/session.cpp" line="131"/>
        <source>Connection terminated</source>
        <translation>会话已终止</translation>
    </message>
    <message>
        <location filename="../streaming/session.cpp" line="132"/>
        <source>Error code: %1</source>
        <translation>错误代码: %1</translation>
    </message>
    <message>
        <location filename="../streaming/session.cpp" line="958"/>
        <source>The version of GeForce Experience on %1 is not supported by this build of Moonlight. You must update Moonlight to stream from %1.</source>
        <translation>在 %1 安装的 GeForce Experience 版本不受当前版本的 Moonlight 支持。你必须升级 Moonlight 才能从 %1 串流。</translation>
    </message>
    <message>
        <location filename="../streaming/session.cpp" line="963"/>
        <source>Your selection to enable remote desktop mouse mode may cause problems in games.</source>
        <translation>启用远程桌面鼠标模式可能会导致游戏出现问题。</translation>
    </message>
    <message>
        <location filename="../streaming/session.cpp" line="967"/>
        <source>Your settings selection to force software decoding may cause poor streaming performance.</source>
        <translation>你选择的用于强制软件解码的设置可能会导致流式传输性能不佳。</translation>
    </message>
    <message>
        <location filename="../streaming/session.cpp" line="1003"/>
        <source>Your host PC doesn&apos;t support encoding HEVC.</source>
        <translation>你的 PC 主机不支持使用HEVC编码。</translation>
    </message>
    <message>
        <location filename="../streaming/session.cpp" line="1021"/>
        <source>Using software decoding due to your selection to force HEVC without GPU support. This may cause poor streaming performance.</source>
        <translation>根据你的选择使用软件解码，以在没有显卡支持的情况下强制 HEVC。这可能会导致流式传输性能不佳。</translation>
    </message>
    <message>
        <location filename="../streaming/session.cpp" line="973"/>
        <source>Your host software or GPU doesn&apos;t support encoding AV1.</source>
        <translation>你的主机软件或显卡不支持进行 AV1 编码。</translation>
    </message>
    <message>
        <location filename="../streaming/session.cpp" line="991"/>
        <source>Using software decoding due to your selection to force AV1 without GPU support. This may cause poor streaming performance.</source>
        <translation>由于你选择在没有显卡支持的情况下强制使用 AV1，因此目前使用软件解码。这可能会导致视频流解码处理性能不佳。</translation>
    </message>
    <message>
        <location filename="../streaming/session.cpp" line="1068"/>
        <source>Your host PC doesn&apos;t support HDR streaming.</source>
        <translation>你的 PC 主机不支持 HDR 串流。</translation>
    </message>
    <message>
        <location filename="../streaming/session.cpp" line="1060"/>
        <source>HDR is not supported using the H.264 codec.</source>
        <translation>使用 H.264 编解码器的情况下不支持 HDR。</translation>
    </message>
    <message>
        <location filename="../streaming/session.cpp" line="1083"/>
        <source>This PC&apos;s GPU doesn&apos;t support AV1 Main10 decoding for HDR streaming.</source>
        <translation>这台电脑的显卡不支持包含 HDR 的 AV1 Main10 解码。</translation>
    </message>
    <message>
        <location filename="../streaming/session.cpp" line="1064"/>
        <source>This PC&apos;s GPU doesn&apos;t support 10-bit HEVC or AV1 decoding for HDR streaming.</source>
        <translation>这台电脑的显卡不支持包含 HDR 的 HEVC 或 AV1 Main10 解码。</translation>
    </message>
    <message>
        <location filename="../streaming/session.cpp" line="1116"/>
        <source>Your host PC and client PC don&apos;t support the same HDR video codecs.</source>
        <translation>主机电脑和客户端电脑不支持相同的 HDR 视频编解码器。</translation>
    </message>
    <message>
        <location filename="../streaming/session.cpp" line="1209"/>
        <source>Video resolutions over 4K are not supported by the H.264 codec.</source>
        <translation>H.264 编解码器不支持超过 4K 的视频分辨率。</translation>
    </message>
    <message>
        <location filename="../streaming/session.cpp" line="1606"/>
        <source>Host returned error: %1</source>
        <translation>主机返回错误：%1</translation>
    </message>
    <message>
        <location filename="../streaming/session.cpp" line="1040"/>
        <source>Using software decoding due to your selection to force H.264 without GPU support. This may cause poor streaming performance.</source>
        <translation>使用软件解码，因为你选择强制使用没有 GPU 支持的 H.264。这可能会导致较差的流性能。</translation>
    </message>
    <message>
        <location filename="../streaming/session.cpp" line="1050"/>
        <source>Your host PC and client PC don&apos;t support the same video codecs. This may cause poor streaming performance.</source>
        <translation>你的主机和客户端 PC 不支持相同的视频编解码器。这可能会导致较差的流性能。</translation>
    </message>
    <message>
        <location filename="../streaming/session.cpp" line="1053"/>
        <source>Your client GPU doesn&apos;t support H.264 decoding. This may cause poor streaming performance.</source>
        <translation>你的客户端 GPU 不支持 H.264 解码。这可能会导致较差的流性能。</translation>
    </message>
    <message>
        <location filename="../streaming/session.cpp" line="1089"/>
        <location filename="../streaming/session.cpp" line="1107"/>
        <source>Using software decoding due to your selection to force HDR without GPU support. This may cause poor streaming performance.</source>
        <translation>由于您选择强制使用未经 GPU 支持的 HDR，因此将采用软件解码。这可能会导致串流性能不佳。</translation>
    </message>
    <message>
        <location filename="../streaming/session.cpp" line="1101"/>
        <source>This PC&apos;s GPU doesn&apos;t support HEVC Main10 decoding for HDR streaming.</source>
        <translation>此计算机的显卡不支持 HEVC Main10 解码 HDR 流。</translation>
    </message>
    <message>
        <location filename="../streaming/session.cpp" line="1123"/>
        <source>Your host PC doesn&apos;t support YUV 4:4:4 streaming.</source>
        <translation>您的主机 PC 不支持 YUV 4:4:4 串流。</translation>
    </message>
    <message>
        <location filename="../streaming/session.cpp" line="1131"/>
        <source>Your host PC doesn&apos;t support YUV 4:4:4 streaming for selected video codec.</source>
        <translation>您的主机 PC 不支持所选视频编解码器的 YUV 4:4:4 串流。</translation>
    </message>
    <message>
        <location filename="../streaming/session.cpp" line="1146"/>
        <source>Using software decoding due to your selection to force YUV 4:4:4 without GPU support. This may cause poor streaming performance.</source>
        <translation>由于您选择强制使用未经 GPU 支持的 YUV 4:4:4，因此将采用软件解码。这可能会导致串流性能不佳。</translation>
    </message>
    <message>
        <location filename="../streaming/session.cpp" line="1152"/>
        <source>This PC&apos;s GPU doesn&apos;t support YUV 4:4:4 decoding for selected video codec.</source>
        <translation>这台 PC 的 GPU 不支持所选视频编解码器的 YUV 4:4:4 解码。</translation>
    </message>
    <message>
        <location filename="../streaming/session.cpp" line="1161"/>
        <source>GeForce Experience 3.0 or higher is required for 4K streaming.</source>
        <translation>4K 流式传输需要 GeForce Experience 3.0 或更高版本。</translation>
    </message>
    <message>
        <location filename="../streaming/session.cpp" line="1176"/>
        <source>Your selected surround sound setting is not supported by the current audio device.</source>
        <translation>当前音频设备不支持你选择的环绕声设置。</translation>
    </message>
    <message>
        <location filename="../streaming/session.cpp" line="1182"/>
        <source>Failed to open audio device. Audio will be unavailable during this session.</source>
        <translation>无法打开音频设备。音频在此会话期间将不可用。</translation>
    </message>
    <message>
        <location filename="../streaming/session.cpp" line="1187"/>
        <source>An attached gamepad has no mapping and won&apos;t be usable. Visit the Moonlight help to resolve this.</source>
        <translation>附加的手柄没有映射，因此无法使用。请访问 Moonlight 帮助解决此问题。</translation>
    </message>
    <message>
        <location filename="../streaming/session.cpp" line="1205"/>
        <source>Your host PC&apos;s GPU doesn&apos;t support streaming video resolutions over 4K.</source>
        <translation>目标计算机的显卡不支持 4K 以上的流式传输视频分辨率。</translation>
    </message>
    <message>
        <location filename="../streaming/session.cpp" line="1223"/>
        <source>Your selection to force hardware decoding cannot be satisfied due to missing hardware decoding support on this PC&apos;s GPU.</source>
        <translation>由于此计算机的显卡上缺少硬件解码支持，无法满足强制硬件解码的选择。</translation>
    </message>
    <message>
        <location filename="../streaming/session.cpp" line="1226"/>
        <source>Your codec selection and force hardware decoding setting are not compatible. This PC&apos;s GPU lacks support for decoding your chosen codec.</source>
        <translation>你选择的编解码器和强制硬件解码设置不兼容。此计算机的显卡不支持解码你选择的编解码器。</translation>
    </message>
    <message>
        <location filename="../streaming/session.cpp" line="2204"/>
        <source>Unable to initialize video decoder. Please check your streaming settings and try again.</source>
        <translation>无法初始化视频解码器。请检查你的流式传输设置，然后重试。</translation>
    </message>
</context>
<context>
    <name>SettingsView</name>
    <message><source>Sync copied text with the remote Windows PC</source><translation>与远端 Windows 电脑同步复制的文本</translation></message>
    <message><source>Sync copied images with the remote Windows PC</source><translation>与远端 Windows 电脑同步复制的图片</translation></message>
    <message><source>Transfer copied files through the clipboard</source><translation>通过剪贴板传输复制的文件</translation></message>
    <message><source>Compatible Windows Sunshine hosts lock after the last stream ends, even after a network loss. This request takes effect when you connect. On older hosts, Moonlight sends a lock shortcut when you disconnect.</source><translation>兼容的 Windows Sunshine 主机会在最后一个串流结束后锁屏，网络意外断开也会生效。此请求在连接时生效。对于旧版主机，Moonlight 会在主动断开时发送锁屏快捷键。</translation></message>
    <message><source>Off</source><translation>关闭</translation></message>
    <message><source>Fullscreen only</source><translation>仅全屏时</translation></message>
    <message><source>Always</source><translation>始终</translation></message>
    <message><source>Security</source><translation>安全</translation></message>
    <message><source>Session security</source><translation>会话安全</translation></message>
    <message><source>Lock the remote PC when I disconnect</source><translation>主动断开远控时锁定被控电脑</translation></message>
    <message><source>Moonlight sends Win+L when you disconnect manually. The remote PC must support this shortcut. If the connection is lost, Moonlight cannot send it.</source><translation>主动断开时，Moonlight 会向被控电脑发送 Win+L。被控电脑需支持此快捷键。连接意外中断时无法发送。</translation></message>
    <message><source>General</source><translation>常规</translation></message>
    <message><source>Streaming</source><translation>串流</translation></message>
    <message><source>Input</source><translation>输入</translation></message>
    <message><source>Advanced</source><translation>高级</translation></message>
    <message><source>Close main window to system tray</source><translation>关闭主窗口时收入系统托盘</translation></message>
    <message>
        <location filename="../gui/SettingsView.qml" line="13"/>
        <source>Settings</source>
        <translation>设置</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="106"/>
        <source>Basic Settings</source>
        <translation>基本设置</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="116"/>
        <source>Resolution and FPS</source>
        <translation>分辨率和 FPS</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="124"/>
        <source>Setting values too high for your PC or network connection may cause lag, stuttering, or errors.</source>
        <translation>如果你的计算机或网络连接设置的值过高，可能会导致延迟，卡顿或错误。</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="250"/>
        <source>720p</source>
        <translation>720p</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="256"/>
        <source>1080p</source>
        <translation>1080p</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="262"/>
        <source>1440p</source>
        <translation>1440p</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="268"/>
        <source>4K</source>
        <translation>4K</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="375"/>
        <source>Custom resolutions are not officially supported by GeForce Experience, so it will not set your host display resolution. You will need to set it manually while in game.</source>
        <translation>自定义分辨率不受 GeForce Experience 的正式支持，因此不会设置目标计算机的分辨率。你需要在游戏中手动设置。</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="376"/>
        <source>Resolutions that are not supported by your client or host PC may cause streaming errors.</source>
        <translation>使用当前计算机或目标计算机不支持的分辨率可能会导致流式传输错误。</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="382"/>
        <source>Enter a custom resolution:</source>
        <translation>输入自定义分辨率:</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="512"/>
        <location filename="../gui/SettingsView.qml" line="622"/>
        <source>Custom (%1 FPS)</source>
        <translation>自定义(%1 FPS)</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="527"/>
        <source>Enter a custom frame rate:</source>
        <translation>输入自定义帧率:</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="604"/>
        <source>%1 FPS</source>
        <translation>%1 FPS</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="672"/>
        <source>Video bitrate:</source>
        <translation>视频比特率:</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="680"/>
        <source>Lower the bitrate on slower connections. Raise the bitrate to increase image quality.</source>
        <translation>在较慢的连接上降低比特率。提高比特率以提高图像质量。</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="702"/>
        <source>Video bitrate: %1 Mbps</source>
        <translation>视频比特率: %1 Mbps</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="732"/>
        <source>Display mode</source>
        <translation>显示模式</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="762"/>
        <source>(Recommended)</source>
        <translation>(推荐)</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="743"/>
        <location filename="../gui/SettingsView.qml" line="1218"/>
        <source>Fullscreen</source>
        <translation>全屏</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="182"/>
        <source>Native</source>
        <translation>原生</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="183"/>
        <source>Native (Excluding Notch)</source>
        <translation>原生(不包括刻痕)</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="219"/>
        <location filename="../gui/SettingsView.qml" line="228"/>
        <location filename="../gui/SettingsView.qml" line="625"/>
        <source>Custom</source>
        <translation>自定义</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="643"/>
        <source>30 FPS</source>
        <translation>30 帧</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="648"/>
        <source>60 FPS</source>
        <translation>60 帧</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="718"/>
        <source>Use Default (%1 Mbps)</source>
        <translation>使用默认 (%1 Mbps)</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="748"/>
        <source>Borderless windowed</source>
        <translation>无边框窗口</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="753"/>
        <location filename="../gui/SettingsView.qml" line="1210"/>
        <source>Windowed</source>
        <translation>窗口化</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="812"/>
        <source>Fullscreen generally provides the best performance, but borderless windowed may work better with features like macOS Spaces, Alt+Tab, screenshot tools, on-screen overlays, etc.</source>
        <translation>全屏通常提供最好的性能，但是无边框窗口可以更好地与 macOS Spaces、Alt+Tab、屏幕截图工具、屏幕覆盖等功能配合使用。</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="819"/>
        <source>V-Sync</source>
        <translation>垂直同步</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="829"/>
        <source>Disabling V-Sync allows sub-frame rendering latency, but it can display visible tearing</source>
        <translation>禁用垂直同步可允许子帧渲染延迟，但会显示可见的撕裂</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="836"/>
        <source>Frame pacing</source>
        <translation>帧速调节</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="846"/>
        <source>Frame pacing reduces micro-stutter by delaying frames that come in too early</source>
        <translation>通过延迟过早进入的帧来减少卡顿</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="856"/>
        <source>Audio Settings</source>
        <translation>音频设置</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="866"/>
        <source>Audio configuration</source>
        <translation>音频配置</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="891"/>
        <source>Stereo</source>
        <translation>立体声</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="895"/>
        <source>5.1 surround sound</source>
        <translation>5.1 环绕声</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="899"/>
        <source>7.1 surround sound</source>
        <translation>7.1 环绕声</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="913"/>
        <source>Mute host PC speakers while streaming</source>
        <translation>流式传输启动时将目标计算机的扬声器静音</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="923"/>
        <source>You must restart any game currently in progress for this setting to take effect</source>
        <translation>要使此设置生效，必须重新启动当前正在进行的所有游戏</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="929"/>
        <source>Mute audio stream when Moonlight is not the active window</source>
        <translation>当 Moonlight 失去焦点时静音</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="940"/>
        <source>Mutes Moonlight&apos;s audio when you Alt+Tab out of the stream or click on a different window.</source>
        <translation>当你用 Alt+Tab 键离开流式传输或者点击其他窗口时，Moonlight 将静音。</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="989"/>
        <source>UI Settings</source>
        <translation>界面设置</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="999"/>
        <source>Language</source>
        <translation>语言</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1025"/>
        <source>Automatic</source>
        <translation>自动</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1160"/>
        <source>You must restart Moonlight for this change to take effect</source>
        <translation>你必须重新启动 Moonlight 才能使更改生效</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1177"/>
        <source>GUI display mode</source>
        <translation>窗口显示模式</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1214"/>
        <source>Maximized</source>
        <translation>最大化</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1231"/>
        <source>Show connection quality warnings</source>
        <translation>显示连接质量警告</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1242"/>
        <source>Show configuration warnings</source>
        <translation>显示配置警告</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1254"/>
        <source>Discord Rich Presence integration</source>
        <translation>Discord Rich Presence 集成</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1264"/>
        <source>Updates your Discord status to display the name of the game you&apos;re streaming.</source>
        <translation>更新你的 Discord 状态，以显示正在运行的游戏名称。</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1270"/>
        <source>Keep the display awake while streaming</source>
        <translation>在串流时保持屏幕开启</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1280"/>
        <source>Prevents the screensaver from starting or the display from going to sleep while streaming.</source>
        <translation>防止串流时进入屏保或屏幕休眠。</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1298"/>
        <source>Input Settings</source>
        <translation>输入设置</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1309"/>
        <source>Optimize mouse for remote desktop instead of games</source>
        <translation>为远程桌面而不是游戏优化鼠标</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1319"/>
        <source>This enables seamless mouse control without capturing the client&apos;s mouse cursor. It is ideal for remote desktop usage but will not work in most games.</source>
        <translation>这可以实现无缝鼠标控制，而无需捕获客户端的鼠标光标。这是理想的远程桌面使用方案，但不会在大多数游戏中工作。</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1320"/>
        <source>You can toggle this while streaming using Ctrl+Alt+Shift+M.</source>
        <translation>你可以使用 Ctrl+Alt+Shift+M 切换鼠标控制模式。</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1321"/>
        <source>NOTE: Due to a bug in GeForce Experience, this option may not work properly if your host PC has multiple monitors.</source>
        <translation>注意：由于 GeForce Experience 的 Bug，如果你的目标计算机有多个显示器，则此选项可能无法正常工作。</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1331"/>
        <source>Capture system keyboard shortcuts</source>
        <translation>捕获系统快捷键</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1339"/>
        <source>This enables the capture of system-wide keyboard shortcuts like Alt+Tab that would normally be handled by the client OS while streaming.</source>
        <translation>这样可以捕获系统范围的键盘快捷键，例如 Alt + Tab，这些快捷键通常在流式传输时由客户端系统处理。</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1621"/>
        <source>AV1</source>
        <translation>AV1</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1636"/>
        <source>Enable HDR</source>
        <translation>启用HDR</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1651"/>
        <source>The stream will be HDR-capable, but some games may require an HDR monitor on your host PC to enable HDR mode.</source>
        <translation>HDR功能将在串流中可用，但一些游戏可能需要PC端接入HDR显示器以启用HDR模式。</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1653"/>
        <source>HDR streaming is not supported on this PC.</source>
        <translation>此PC不支持HDR。</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1659"/>
        <source>Enable YUV 4:4:4</source>
        <translation>启用 YUV 4:4:4</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1740"/>
        <source>Show performance stats while streaming</source>
        <translation>在流中显示性能统计数据</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1750"/>
        <source>Display real-time stream performance information while streaming.</source>
        <translation>在流中显示实时流性能信息。</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1751"/>
        <source>You can toggle it at any time while streaming using Ctrl+Alt+Shift+S or Select+L1+R1+X.</source>
        <translation>你可以在任何时候使用 Ctrl + Alt + Shift + S 或选择 + L1 + R1 + X 来切换它。</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1752"/>
        <source>The performance overlay is not supported on Steam Link or Raspberry Pi.</source>
        <translation>Steam链接或树莓派不支持性能覆盖。</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1340"/>
        <source>NOTE: Certain keyboard shortcuts like Ctrl+Alt+Del on Windows cannot be intercepted by any application, including Moonlight.</source>
        <translation>注意：Windows 上的某些键盘快捷键（如 Ctrl+Alt+Del）不能被任何应用程序截获，包括 Moonlight。</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1369"/>
        <source>in fullscreen</source>
        <translation>全屏</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1373"/>
        <source>always</source>
        <translation>总是</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1403"/>
        <source>Use touchscreen as a virtual trackpad</source>
        <translation>将触摸屏用作虚拟触控板</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1413"/>
        <source>When checked, the touchscreen acts like a trackpad. When unchecked, the touchscreen will directly control the mouse pointer.</source>
        <translation>选中时，触摸屏就像一个触控板。取消选中时，触摸屏将直接控制鼠标指针。</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1420"/>
        <source>Swap left and right mouse buttons</source>
        <translation>交换鼠标左键和右键</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1432"/>
        <source>Reverse mouse scrolling direction</source>
        <translation>反转鼠标滚动方向</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1446"/>
        <source>Gamepad Settings</source>
        <translation>手柄设置</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1456"/>
        <source>Swap A/B and X/Y gamepad buttons</source>
        <translation>交换手柄的 A/B 和 X/Y 按钮</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1466"/>
        <source>This switches gamepads into a Nintendo-style button layout</source>
        <translation>这会将手柄切换为任天堂风格的按钮布局</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1472"/>
        <source>Force gamepad #1 always connected</source>
        <translation>强制手柄 #1 始终连接</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1482"/>
        <source>Forces a single gamepad to always stay connected to the host, even if no gamepads are actually connected to this PC.</source>
        <translation>强制单个手柄始终与目标计算机保持连接，即使没有任何手柄实际连接到此计算机。</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1483"/>
        <source>Only enable this option when streaming a game that doesn&apos;t support gamepads being connected after startup.</source>
        <translation>仅在游戏启动后不支持手柄连接时才启用此选项。</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1490"/>
        <source>Enable mouse control with gamepads by holding the &apos;Start&apos; button</source>
        <translation>按住手柄的“Start”按钮启用鼠标控制</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1501"/>
        <source>Process gamepad input when Moonlight is in the background</source>
        <translation>当 Moonlight 在后台时处理手柄输入</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1512"/>
        <source>Allows Moonlight to capture gamepad inputs even if it&apos;s not the current window in focus</source>
        <translation>即使不是焦点窗口，也允许 Moonlight 捕获手柄的输入</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="949"/>
        <source>Host Settings</source>
        <translation>目标计算机设置</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="959"/>
        <source>Optimize game settings for streaming</source>
        <translation>优化游戏设置以进行流式传输</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="970"/>
        <source>Quit app on host PC after ending stream</source>
        <translation>流式传输结束后退出目标计算机上的应用程序</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="980"/>
        <source>This will close the app or game you are streaming when you end your stream. You will lose any unsaved progress!</source>
        <translation>当你结束流式传输时，这将关闭正在运行的应用程序或游戏。这会导致你失去所有未保存的游戏进度！</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1521"/>
        <source>Advanced Settings</source>
        <translation>高级设置</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1531"/>
        <source>Video decoder</source>
        <translation>视频解码器</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1556"/>
        <location filename="../gui/SettingsView.qml" line="1609"/>
        <source>Automatic (Recommended)</source>
        <translation>自动 (推荐)</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1560"/>
        <source>Force software decoding</source>
        <translation>强制软件解码</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1564"/>
        <source>Force hardware decoding</source>
        <translation>强制硬件解码</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1579"/>
        <source>Video codec</source>
        <translation>视频编解码器</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1613"/>
        <source>H.264</source>
        <translation>H.264</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1617"/>
        <source>HEVC (H.265)</source>
        <translation>HEVC (H.265)</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1681"/>
        <source>Good for streaming desktop and text-heavy games, but not recommended for fast-paced games.</source>
        <translation>适合串流桌游和文字类游戏，但不推荐用于快节奏游戏。</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1683"/>
        <source>YUV 4:4:4 is not supported on this PC.</source>
        <translation>此电脑不支持 YUV 4:4:4。</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1689"/>
        <source>Unlock bitrate limit (Experimental)</source>
        <translation>解锁比特率限制（实验性）</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1702"/>
        <source>This unlocks extremely high video bitrates for use with Sunshine hosts. It should only be used when streaming over an Ethernet LAN connection.</source>
        <translation>这可解锁极高的视频码率供 Sunshine 主机使用。仅限于局域网串流时使用。</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1708"/>
        <source>Automatically find PCs on the local network (Recommended)</source>
        <translation>自动在本地网络上查找计算机 (推荐)</translation>
    </message>
    <message>
        <location filename="../gui/SettingsView.qml" line="1729"/>
        <source>Automatically detect blocked connections (Recommended)</source>
        <translation>自动检测被阻止的连接 (推荐)</translation>
    </message>
</context>
<context>
    <name>StreamStatusCard</name>
    <message><source>Step %1 of %2</source><translation>第 %1 步，共 %2 步</translation></message>
</context>
<context>
    <name>StreamSegue</name>
    <message><source>Checking video and audio capabilities...</source><translation>正在检测本机的音视频能力…</translation></message>
    <message><source>Reviewing stream settings...</source><translation>正在显示串流设置提示…</translation></message>
    <message>
        <location filename="../gui/StreamSegue.qml" line="12"/>
        <source>Resuming %1...</source>
        <translation>正在恢复 %1...</translation>
    </message>
    <message>
        <location filename="../gui/StreamSegue.qml" line="13"/>
        <location filename="../gui/StreamSegue.qml" line="20"/>
        <source>Starting %1...</source>
        <translation>正在启动 %1...</translation>
    </message>
    <message>
        <location filename="../gui/StreamSegue.qml" line="26"/>
        <source>Starting %1 failed: Error %2</source>
        <translation>启动 %1 失败: 错误 %2</translation>
    </message>
    <message>
        <location filename="../gui/StreamSegue.qml" line="29"/>
        <source>Check your firewall and port forwarding rules for port(s): %1</source>
        <translation>检查防火墙和端口转发规则: %1</translation>
    </message>
    <message>
        <location filename="../gui/StreamSegue.qml" line="65"/>
        <source>This PC&apos;s Internet connection is blocking Moonlight. Streaming over the Internet may not work while connected to this network.</source>
        <translation>此计算机的互联网连接被阻塞。当连接到此网络时，互联网上的流式传输可能无法工作。</translation>
    </message>
    <message>
        <location filename="../gui/StreamSegue.qml" line="168"/>
        <source>Tip:</source>
        <translation>提示:</translation>
    </message>
    <message>
        <location filename="../gui/StreamSegue.qml" line="168"/>
        <source>Press %1 to disconnect your session</source>
        <translation>按 %1 来断开会话</translation>
    </message>
    <message>
        <location filename="../gui/StreamSegue.qml" line="169"/>
        <source>Start+Select+L1+R1</source>
        <translation>Start+Select+L1+R1</translation>
    </message>
    <message>
        <location filename="../gui/StreamSegue.qml" line="169"/>
        <source>Ctrl+Alt+Shift+Q</source>
        <translation>Ctrl+Alt+Shift+Q</translation>
    </message>
</context>
<context>
    <name>main</name>
    <message><source>Minimize</source><translation>最小化</translation></message>
    <message><source>Restore</source><translation>还原</translation></message>
    <message><source>Maximize</source><translation>最大化</translation></message>
    <message><source>Close</source><translation>关闭</translation></message>
    <message><source>Unable to open the streaming window.</source><translation>无法打开串流窗口。</translation></message>
    <message><source>Show main window</source><translation>显示主窗口</translation></message>
    <message><source>Quit</source><translation>退出程序</translation></message>
    <message>
        <location filename="../gui/main.qml" line="444"/>
        <source>Settings</source>
        <translation>设置</translation>
    </message>
    <message>
        <location filename="../gui/main.qml" line="291"/>
        <source>Version %1</source>
        <translation>版本 %1</translation>
    </message>
    <message>
        <location filename="../gui/main.qml" line="307"/>
        <source>Join our community on Discord</source>
        <translation>Discord 社区</translation>
    </message>
    <message>
        <location filename="../gui/main.qml" line="326"/>
        <source>Add PC manually</source>
        <translation>手动添加计算机</translation>
    </message>
    <message>
        <location filename="../gui/main.qml" line="366"/>
        <source>Update available for Moonlight: Version %1</source>
        <translation>Moonlight 可用更新：版本 %1</translation>
    </message>
    <message>
        <location filename="../gui/main.qml" line="390"/>
        <source>Help</source>
        <translation>帮助</translation>
    </message>
    <message>
        <location filename="../gui/main.qml" line="413"/>
        <source>Gamepad Mapper</source>
        <translation>手柄按键映射</translation>
    </message>
    <message>
        <location filename="../gui/main.qml" line="451"/>
        <source>No functioning hardware accelerated video decoder was detected by Moonlight. Your streaming performance may be severely degraded in this configuration.</source>
        <translation>Moonlight 未检测到有效的硬件加速视频解码器。在此配置中，你的流性能可能会严重下降。</translation>
    </message>
    <message>
        <location filename="../gui/main.qml" line="453"/>
        <source>Click the Help button for more information on solving this problem.</source>
        <translation>单击帮助按钮以获取有关解决此问题的更多信息。</translation>
    </message>
    <message>
        <location filename="../gui/main.qml" line="459"/>
        <source>Hardware acceleration doesn&apos;t work on XWayland. Continuing on XWayland may result in poor streaming performance. Try running with QT_QPA_PLATFORM=wayland or switch to X11.</source>
        <translation>硬件加速在 XWayland 上不起作用。在 XWayland 上继续操作可能会导致流式传输性能下降。</translation>
    </message>
    <message>
        <location filename="../gui/main.qml" line="461"/>
        <source>Click the Help button for more information.</source>
        <translation>单击帮助按钮以获取更多信息。</translation>
    </message>
    <message>
        <location filename="../gui/main.qml" line="468"/>
        <source>This version of Moonlight isn&apos;t optimized for your PC. Please download the &apos;%1&apos; version of Moonlight for the best streaming performance.</source>
        <translation>此版本的 Moonlight 尚未针对你的计算机优化。请下载“%1”版本的 Moonlight，以获得最佳的流式传输性能。</translation>
    </message>
    <message>
        <location filename="../gui/main.qml" line="477"/>
        <source>Moonlight detected gamepads without a mapping:</source>
        <translation>Moonlight 检测到没有映射的手柄:</translation>
    </message>
    <message>
        <location filename="../gui/main.qml" line="479"/>
        <source>Click the Help button for information on how to map your gamepads.</source>
        <translation>单击帮助按钮以获取有关如何映射手柄的信息。</translation>
    </message>
    <message>
        <location filename="../gui/main.qml" line="487"/>
        <source>Are you sure you want to quit?</source>
        <translation>确定要退出吗？</translation>
    </message>
    <message>
        <location filename="../gui/main.qml" line="517"/>
        <source>Enter the IP address of your host PC:</source>
        <translation>输入你主机的 IP 地址：</translation>
    </message>
</context>
<context>
    <name>StreamControlCenter</name>
    <message><source>File transfer</source><translation>文件传输</translation></message>
    <message><source>Request screen lock on disconnect</source><translation>主动断开时锁定被控电脑</translation></message>
    <message><source>Use installed host lock shortcut</source><translation>使用被控机锁屏快捷方式</translation></message>
    <message><source>Request screen lock now</source><translation>立即请求锁屏</translation></message>
    <message><source>Lock shortcut queued</source><translation>锁屏按键已加入发送队列</translation></message>
    <message><source>Couldn't queue lock shortcut</source><translation>锁屏按键未能加入发送队列</translation></message>
    <message><source>If Win+L doesn't lock this PC, install the Ctrl+Alt+F10 lock shortcut on the remote PC and enable it above. Press Ctrl+Alt+Shift+End during a stream to request a lock.</source><translation>如果 Win+L 无法锁定被控机，请在被控机上安装 Ctrl+Alt+F10 锁屏快捷方式，再开启上方对应选项。串流中可按 Ctrl+Alt+Shift+End 请求锁屏。</translation></message>
    <message><source>HUD settings</source><translation>统计浮层设置</translation></message>
    <message><source>Security</source><translation>安全</translation></message>
    <message><source>Send Win+L on disconnect</source><translation>主动断开时锁定被控电脑</translation></message>
    <message><source>Duration</source><translation>连接时长</translation></message>
    <message><source>Frame rate</source><translation>帧率</translation></message>
    <message><source>Network RTT</source><translation>网络延迟</translation></message>
    <message><source>Frame latency</source><translation>帧延迟</translation></message>
    <message><source>Adapter upload</source><translation>网卡上传</translation></message>
    <message><source>Adapter download</source><translation>网卡下载</translation></message>
    <message><source>Network frame loss</source><translation>网络丢帧率</translation></message>
    <message><source>Resolution</source><translation>分辨率</translation></message>
    <message><source>Statistics HUD</source><translation>显示统计浮窗</translation></message>
    <message><source>Displayed metrics</source><translation>显示项目</translation></message>
    <message><source>Position</source><translation>浮窗位置</translation></message>
    <message><source>Opacity</source><translation>不透明度</translation></message>
    <message><source>Fullscreen</source><translation>进入全屏</translation></message>
    <message><source>Top left</source><translation>左上角</translation></message>
    <message><source>Top right</source><translation>右上角</translation></message>
    <message><source>Bottom left</source><translation>左下角</translation></message>
    <message><source>Bottom right</source><translation>右下角</translation></message>
    <message><source>Control center</source><translation>控制中心</translation></message>
</context>
<context>
    <name>FileTransferWindow</name>
    <message><source>Drag files or folders to the other PC. Drop on a folder to copy into it.</source><translation>可将文件或文件夹拖到另一台电脑。拖到文件夹上即可复制到该目录。</translation></message>
    <message><source>Drop the selection in the local pane, in File Explorer, or on the desktop.</source><translation>请将所选项目拖到本机面板、资源管理器或桌面。</translation></message>
    <message><source>File actions</source><translation>文件操作</translation></message>
    <message><source>Open</source><translation>打开</translation></message>
    <message><source>Open temporary copy</source><translation>在本机打开临时副本</translation></message>
    <message><source>Could not open %1.</source><translation>无法打开 %1。</translation></message>
    <message><source>Could not create a temporary folder.</source><translation>无法创建临时文件夹。</translation></message>
    <message><source>Downloading %1 to open…</source><translation>正在下载 %1，完成后将在本机打开…</translation></message>
    <message><source>Copying files…</source><translation>正在复制文件…</translation></message>
    <message><source>File already exists</source><translation>文件已存在</translation></message>
    <message><source>%1 already exists. Replace it?</source><translation>%1 已存在，是否替换？</translation></message>
    <message><source>%1 already exists.</source><translation>%1 已存在。</translation></message>
    <message><source>Skip</source><translation>跳过</translation></message>
    <message><source>Replace</source><translation>替换</translation></message>
    <message><source>Keep both</source><translation>保留两者</translation></message>
    <message><source>Delete…</source><translation>删除…</translation></message>
    <message><source>Delete files</source><translation>删除文件</translation></message>
    <message><source>Permanently delete the selected files on the remote PC?</source><translation>要永久删除远端电脑上的所选文件吗？</translation></message>
    <message><source>Move the selected files to the Recycle Bin on this PC?</source><translation>要将本机的所选文件移入回收站吗？</translation></message>
    <message><source>%1 files in %2</source><translation>%2 中的 %1 个文件</translation></message>
    <message><source>Deleting files…</source><translation>正在删除文件…</translation></message>
    <message><source>Resume selected</source><translation>继续所选任务</translation></message>
    <message><source>Interrupted</source><translation>已中断</translation></message>
    <message><source>Speed</source><translation>速度</translation></message>
    <message><source>%1/s</source><translation>%1/秒</translation></message>
    <message><source>Cancelling…</source><translation>正在取消…</translation></message>
    <message><source>Cancelled</source><translation>已取消</translation></message>
    <message><source>Transfer is busy in another window</source><translation>另一窗口正在传输此任务</translation></message>
    <message><source>Transfer ended in another window</source><translation>此任务已在另一窗口结束</translation></message>
    <message><source>New folder</source><translation>新建文件夹</translation></message>
    <message><source>Folder name</source><translation>文件夹名称</translation></message>
    <message><source>Locations and bookmarks</source><translation>位置和书签</translation></message>
    <message><source>Select all</source><translation>全选</translation></message>
    <message><source>Type</source><translation>类型</translation></message>
    <message><source>Drive</source><translation>磁盘</translation></message>
    <message><source>Folder</source><translation>文件夹</translation></message>
    <message><source>File</source><translation>文件</translation></message>
    <message><source>Documents</source><translation>我的文档</translation></message>
    <message><source>Desktop</source><translation>桌面</translation></message>
    <message><source>Pictures</source><translation>图片</translation></message>
    <message><source>Videos</source><translation>视频</translation></message>
    <message><source>Music</source><translation>音乐</translation></message>
    <message><source>Downloads</source><translation>下载</translation></message>
    <message><source>Locations unavailable</source><translation>暂时无法读取快捷位置</translation></message>
    <message><source>This host did not grant file access. Check the Sunshine version and file transfer permission.</source><translation>主机未授予文件访问权限，请检查 Sunshine 版本和文件传输权限。</translation></message>
    <message><source>File transfer</source><translation>文件传输</translation></message>
    <message><source>Local · %1</source><translation>本机 · %1</translation></message>
    <message><source>Remote · %1</source><translation>远端 · %1</translation></message>
    <message><source>Transfers</source><translation>传输列表</translation></message>
    <message><source>Cancel selected</source><translation>取消所选任务</translation></message>
    <message><source>Clear finished</source><translation>清除已结束任务</translation></message>
    <message><source>Name</source><translation>名称</translation></message>
    <message><source>Status</source><translation>状态</translation></message>
    <message><source>Transferred</source><translation>已传输</translation></message>
    <message><source>Source</source><translation>发送路径</translation></message>
    <message><source>Destination</source><translation>接收路径</translation></message>
    <message><source>In Sunshine, enable “Browse and transfer files” under Configuration → Input, then reconnect.</source><translation>请在 Sunshine 的「配置 → 输入」中开启「Browse and transfer files」（浏览和传输文件），然后重新连接串流。</translation></message>
    <message><source>← Download</source><translation>← 下载</translation></message>
    <message><source>Upload →</source><translation>上传 →</translation></message>
    <message><source>Back</source><translation>后退</translation></message>
    <message><source>Parent folder</source><translation>上一级</translation></message>
    <message><source>This PC</source><translation>此电脑</translation></message>
    <message><source>Folder path</source><translation>文件夹路径</translation></message>
    <message><source>Refresh</source><translation>刷新</translation></message>
    <message><source>Bookmarks</source><translation>书签</translation></message>
    <message><source>Modified</source><translation>修改日期</translation></message>
    <message><source>Size</source><translation>大小</translation></message>
    <message><source>Copy</source><translation>复制</translation></message>
    <message><source>Paste here</source><translation>粘贴到此处</translation></message>
    <message><source>Enter an absolute folder path.</source><translation>请输入完整的文件夹路径。</translation></message>
    <message><source>Loading…</source><translation>正在读取…</translation></message>
    <message><source>Empty folder</source><translation>文件夹为空</translation></message>
    <message><source>%1 items</source><translation>%1 个项目</translation></message>
    <message><source>Bookmark current folder</source><translation>收藏当前文件夹</translation></message>
    <message><source>Remove current bookmark</source><translation>取消收藏当前文件夹</translation></message>
    <message><source>Queued</source><translation>等待传输</translation></message>
</context>
<context>
    <name>FileTransferEngine</name>
    <message><source>Drop files on a folder or empty space in File Explorer or on the desktop.</source><translation>请将文件拖到资源管理器中的文件夹或空白处，也可以拖到桌面。</translation></message>
    <message><source>The file drop timed out.</source><translation>文件拖放超时，请重试。</translation></message>
    <message><source>The dragged selection contains an unavailable file or folder.</source><translation>拖动的文件或文件夹中有项目无法访问。</translation></message>
    <message><source>The file changed or is in use: %1</source><translation>文件已更改或正被使用：%1</translation></message>
    <message><source> - Copy</source><translation> - 副本</translation></message>
    <message><source> - Copy (%1)</source><translation> - 副本 (%1)</translation></message>
    <message><source>The source and destination are the same file.</source><translation>源文件和目标文件是同一个文件。</translation></message>
    <message><source>A folder occupies this name.</source><translation>目标位置已有同名文件夹。</translation></message>
    <message><source>Replacing remote files requires an updated Sunshine host with file deletion enabled.</source><translation>替换远端文件需要主机使用支持此功能的 Sunshine 版本，并开启“允许删除文件”。</translation></message>
    <message><source>Skipped</source><translation>已跳过</translation></message>
    <message><source>Completed (%1 skipped)</source><translation>已完成（已跳过 %1 项）</translation></message>
    <message><source>Update Sunshine to delete remote files.</source><translation>请更新 Sunshine 以使用远端文件删除功能。</translation></message>
    <message><source>Enable “Allow file deletion” in Sunshine under Configuration → Input.</source><translation>请在 Sunshine 的「配置 → 输入」中开启「Allow file deletion」（允许删除文件）。</translation></message>
    <message><source>This transfer is already running or its checkpoint is unavailable</source><translation>任务已在传输，或无法访问其断点记录</translation></message>
    <message><source>Cannot read transfer checkpoint</source><translation>无法读取传输断点</translation></message>
    <message><source>Cannot save transfer checkpoint</source><translation>无法保存传输断点</translation></message>
    <message><source>Invalid transfer checkpoint from host</source><translation>主机返回的传输断点无效</translation></message>
    <message><source>The partial download changed: %1</source><translation>已下载的部分文件已发生变化：%1</translation></message>
    <message><source>Cannot create partial download: %1</source><translation>无法创建下载临时文件：%1</translation></message>
    <message><source>Cannot write partial download: %1</source><translation>无法写入下载临时文件：%1</translation></message>
    <message><source>File verification failed: %1</source><translation>文件校验失败：%1</translation></message>
    <message><source>Could not publish download; check the destination: %1</source><translation>无法保存已下载的文件，请检查目标路径：%1</translation></message>
    <message><source>Enable “Browse and transfer files” in Sunshine and check that this client is paired. Then refresh.</source><translation>请检查客户端配对状态和主机的文件传输权限，然后刷新。</translation></message>
    <message><source>Update Sunshine to a version that supports standalone file transfer, then refresh.</source><translation>请将 Sunshine 更新至支持独立文件传输的版本，然后刷新。</translation></message>
    <message><source>Could not start file transfer. Check the connection and that Sunshine supports standalone file transfer. %1</source><translation>无法连接文件传输服务。请检查网络连接和 Sunshine 版本。%1</translation></message>
    <message><source>Cancelled</source><translation>已取消</translation></message>
    <message><source>Cannot open folder: %1</source><translation>无法打开文件夹：%1</translation></message>
    <message><source>Invalid folder listing from host</source><translation>远端返回的目录列表无效</translation></message>
    <message><source>Invalid file or folder: %1</source><translation>文件或目录无效：%1</translation></message>
    <message><source>Source changed: %1</source><translation>源文件已更改：%1</translation></message>
    <message><source>Destination already exists: %1</source><translation>目标已存在：%1</translation></message>
    <message><source>Cannot create folder: %1</source><translation>无法创建文件夹：%1</translation></message>
    <message><source>File transfer is unavailable for this session</source><translation>当前会话无法传输文件</translation></message>
    <message><source>File changed or destination exists: %1</source><translation>文件已更改或目标已存在：%1</translation></message>
    <message><source>Access denied by host</source><translation>主机拒绝访问</translation></message>
    <message><source>File or folder not found: %1</source><translation>找不到文件或文件夹：%1</translation></message>
    <message><source>Unexpected host response (HTTP %1)</source><translation>主机响应异常（HTTP %1）</translation></message>
    <message><source>Incomplete data from host: %1</source><translation>主机返回的数据不完整：%1</translation></message>
    <message><source>Transferring</source><translation>传输中</translation></message>
    <message><source>Completed</source><translation>已完成</translation></message>
</context>
</TS>
