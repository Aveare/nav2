# NUC 导航代码双端同步说明

## 本次同步

- 日期：2026-11-26
- 云端仓库：`https://github.com/Aveare/nav2.git`
- 本地分支：`main`
- NUC 本轮核对快照：`/home/rm/nav2`（后续不直接连接 GitHub）
- NUC 运行工作区：`/home/rm/cod_ws`

同步前先获取并检查了 `origin/main`。本地以仅快进方式从旧提交前进到 `f898bed`，阅读新增提交后再开始处理本轮差异；同步流程规则随后提交为 `5f4bd13`。

NUC 上原有的 `cod_navigation` 和 `cod_behavior_stack` 是分别来自旧 Gitee 上游的独立仓库，并含有未提交的现场改动，因此没有对它们执行强制拉取或整目录覆盖。本轮曾在 `/home/rm/nav2` 建立核对快照并逐个文件比较运行工作区；后续按照用户确认的新流程，不再由 NUC 直接获取或推送 GitHub。

## 固定同步方向

NUC 现场源码是硬件调试改动的采集端，本地仓库是唯一 GitHub 操作端。同步链路固定为：

```text
NUC /home/rm/cod_ws
        │ SSH 下载并核对
        ▼
本地 D:\BaiduNetdiskDownload\Nav\nav2
        │ fetch / pull / review / Markdown / commit / push
        ▼
GitHub Aveare/nav2
```

NUC 不执行面向 GitHub 的 `git fetch`、`git pull` 或 `git push`。需要将本地修复部署回 NUC 时，使用 SSH 对明确文件做备份、传输和哈希核对，不把部署解释为 NUC 与 GitHub 的直接同步。

## 差异处理

从 NUC 运行工作区保留并合入云端的行为树改动如下：

- 使用 `ament_index_cpp` 获取 ROS 2 软件包的 share 目录。
- 为行为树 XML 和航点 CSV 增加统一资源路径解析。
- 兼容绝对路径、当前工作目录相对路径和安装后的包内资源路径。
- 找不到资源时输出候选路径并让行为树安全失败，避免因写死开发机绝对路径而直接异常退出。
- 补齐 `CMakeLists.txt` 和 `package.xml` 中的 `ament_index_cpp` 依赖。
- 将 BehaviorTree ROS 2 头文件统一改为软件包标准包含路径，避免同一头文件从源码相对路径和安装路径重复引入而产生类型重定义。

云端较新、需要反向部署到 NUC 运行工作区的内容如下：

- `cod_serial.cpp` 的 `chassis_velocity` 参数与角速度字段选择逻辑。
- `cod_navigation/HARDWARE_NAV_TEST.md` 硬件导航测试说明。

`cod_nuc_bringup` 的 31 个云端跟踪文件在同步前已经与运行工作区完全一致。电控源码保存在 NUC 基准仓库中，不在 NUC 上编译、烧录或覆盖其他电控工作区。

## 安全边界

源码同步不处理 `build`、`install`、`log`、运行日志、地图、备份或凭据，也不删除 NUC 工作区的额外文件。向运行工作区写入前必须保存被替换文件的带时间戳备份；同步完成后仅核对文件哈希和 Git 提交，不自动编译、重启建图、启动底盘或烧录固件。

验证在 NUC 的 `/tmp` 独立构建目录中进行，不覆盖运行工作区的 `build` 或 `install`。首次验证由混用两种 BehaviorTree 头文件路径触发类型重定义，统一为标准包含路径后重新执行同一独立构建，`cod_behavior` 编译成功。

向运行工作区同步前已将被替换文件备份到 `/home/rm/cod_deploy/backups/nav2_sync_20261126_042824`。同步后逐文件 SHA-256 与 NUC 基准仓库一致。检查时 `cod-handheld-mapping.service` 在同步前后均为 `inactive`、主进程 PID 均为 0，本轮没有启动或重启建图。

## 后续固定流程

1. 修改导航代码前执行 `git fetch origin`，检查 `origin/main` 的新提交和差异。
2. 使用 `git pull --ff-only origin main` 拉取最新代码，并阅读与待改模块相关的变更。
3. 修改代码时同时新增或更新对应的 Markdown 说明，并完成适合当前风险的验证。
4. 提交并推送到 `origin/main`，确认本地 `HEAD` 与远端 `origin/main` 一致。
5. NUC 产生新改动时，通过 SSH 将明确文件下载到本地仓库，再从第 1 步开始处理；NUC 不直接连接 GitHub。
6. 只有在明确需要部署时才通过 SSH 把对应文件传回 NUC，写入前逐项比较并备份；编译、重启、实车启动和烧录均作为独立操作处理。
