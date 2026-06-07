#include <masterpilot/customdata-core.h>
#include "main.h"
#include <stdlib.h>

mp_config_t gen_mp_config(const AppConfig *config)
{
    mp_config_t ret = {
        .buffer = malloc(config->mtu * config->buffer_count),
        .buffer_count = config->buffer_count,
        .mtu = config->mtu
    };
    return ret;
}

void mp_config_free(mp_config_t *cfg)
{
    free(cfg->buffer);
    cfg->buffer = NULL;
    cfg->buffer_count = 0;
    cfg->mtu = 0;
}