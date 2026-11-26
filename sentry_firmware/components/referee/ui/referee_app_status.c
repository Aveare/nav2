/**
 * @file    referee_app_status.c
 * @brief   裁判 UI 应用状态提供者的注册与读取
 * @layer   COMPONENT
 * @author  小猫大王
* @note    单写者 = 初始化期注册;读取发生在裁判 UI 任务,提供者需自行保证数据一致。
 */
#include "referee_app_status.h"

#include <stddef.h>

static bool (*status_provider)(referee_app_status_t *out);

void referee_app_status_register(bool (*provider)(referee_app_status_t *out))
{
    status_provider = provider;
}

bool referee_app_status_get(referee_app_status_t *out)
{
    if (status_provider == NULL)
        return false;
    return status_provider(out);
}
