#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct step_mapping {
  const char *program_name;
  const char *script_name;
};

enum { SCRIPT_PATH_CAPACITY = 4096 };

static const struct step_mapping STEP_MAPPINGS[] = {
    {"00_查看执行顺序", "00_查看执行顺序.sh"},
    {"01_建图前安全检查", "01_建图前安全检查.sh"},
    {"02_启动实车扫描建图", "02_启动实车扫描建图.sh"},
    {"03_保存为test地图", "03_保存为test地图.sh"},
    {"04_停止扫描建图", "04_停止扫描建图.sh"},
    {"05_启动test地图定位与RViz", "05_启动test地图定位与RViz.sh"},
    {"06_选择test中心点（可选）", "06_RViz点击并保存test中心点.sh"},
    {"06A_选择红方出生点", "06A_RViz点击并保存红方出生点.sh"},
    {"06B_选择蓝方出生点", "06B_RViz点击并保存蓝方出生点.sh"},
    {"07_显示test中心点", "07_显示test中心点.sh"},
    {"08_停止定位和中心点显示", "08_停止定位和中心点显示.sh"},
    {"09_核对地图和出生点", "09_核对test地图和中心点文件.sh"},
    {"10_启动自动建图并30秒保存", "10_启动自动建图并30秒保存.sh"},
    {"11_停止自动建图", "11_停止自动建图.sh"},
    {"12_开启开机自动建图", "12_开启开机自动建图.sh"},
    {"13_关闭开机自动建图", "13_关闭开机自动建图.sh"},
};

static const char *base_name(const char *path) {
  const char *slash = strrchr(path, '/');
  return slash == NULL ? path : slash + 1;
}

int main(int argc, char **argv) {
  const char *program_name = argc > 0 ? base_name(argv[0]) : "";
  const char *script_name = NULL;

  for (size_t index = 0;
       index < sizeof(STEP_MAPPINGS) / sizeof(STEP_MAPPINGS[0]); ++index) {
    if (strcmp(program_name, STEP_MAPPINGS[index].program_name) == 0) {
      script_name = STEP_MAPPINGS[index].script_name;
      break;
    }
  }

  if (script_name == NULL) {
    fprintf(stderr, "未知的建图程序入口：%s\n", program_name);
    return 2;
  }

  char script_path[SCRIPT_PATH_CAPACITY];
  int written = snprintf(script_path, sizeof(script_path),
                         "/home/rm/Desktop/建图与中心点/.scripts/%s",
                         script_name);
  if (written < 0 || (size_t)written >= sizeof(script_path)) {
    fputs("建图脚本路径过长\n", stderr);
    return 3;
  }

  if (getenv("FIELD_STEP_LAUNCHER_PRINT_SCRIPT") != NULL) {
    puts(script_path);
    return 0;
  }

  execl("/usr/bin/bash", "bash", script_path, (char *)NULL);
  fprintf(stderr, "无法启动 %s：%s\n", script_path, strerror(errno));
  return 4;
}
