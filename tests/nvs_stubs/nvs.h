#pragma once
#include <stddef.h>
#include <stdint.h>

using nvs_handle_t=uint32_t;
using esp_err_t=int;
enum nvs_open_mode_t { NVS_READONLY, NVS_READWRITE };
enum { ESP_OK, ESP_ERR_NVS_NOT_FOUND, ESP_ERR_NVS_INVALID_LENGTH, ESP_ERR_NVS_TYPE_MISMATCH,
       ESP_ERR_NVS_NOT_ENOUGH_SPACE, ESP_ERR_NVS_INVALID_STATE };

esp_err_t nvs_open(const char *,nvs_open_mode_t,nvs_handle_t *);
void nvs_close(nvs_handle_t);
esp_err_t nvs_get_blob(nvs_handle_t,const char *,void *,size_t *);
esp_err_t nvs_get_u32(nvs_handle_t,const char *,uint32_t *);
esp_err_t nvs_set_blob(nvs_handle_t,const char *,const void *,size_t);
esp_err_t nvs_set_u32(nvs_handle_t,const char *,uint32_t);
esp_err_t nvs_commit(nvs_handle_t);
