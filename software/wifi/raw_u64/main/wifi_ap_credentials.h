#ifndef WIFI_AP_CREDENTIALS_H
#define WIFI_AP_CREDENTIALS_H
static esp_err_t store_password(nvs_handle_t handle, char *key, const char *password, uint8_t auth_mode)
{
    key[0] = 'p'; // password pa, pb, pc, pd, ...
    esp_err_t err = nvs_set_str(handle, key, password);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to store password: %d", err);
        return err;
    }
    key[0] = 'm'; // authmode ma, mb, mc, md, ...
    err = nvs_set_u8(handle, key, auth_mode);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to store authmode: %d", err);
        return err;
    }
    err = nvs_commit(handle);
    return err;
}

esp_err_t wifi_store_ap(ConnectCommand_t *cmd)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("recent_aps", NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    int empty = -1, match = -1;
    char key[4] = {'s', 'a', '0', 0};
    for (int i = 0; i < 16; ++i) {
        char ssid[33]; size_t len = sizeof(ssid);
        key[1] = 'a' + i;
        err = nvs_get_str(handle, key, ssid, &len);
        if (err == ESP_ERR_NVS_NOT_FOUND) { if (empty < 0) empty = i; }
        else if (err != ESP_OK) { nvs_close(handle); return err; }
        else if (!strcmp(ssid, cmd->ssid)) { match = i; break; }
    }
    // Search all occupied slots before filling a hole left by Forget AP.
    int slot = match >= 0 ? match : empty >= 0 ? empty : 0;
    key[0] = 's'; key[1] = 'a' + slot;
    err = nvs_set_str(handle, key, cmd->ssid);
    if (err == ESP_OK) err = store_password(handle, key, cmd->pw, cmd->auth_mode);
    if (err == ESP_OK) cmd->list_index = slot;
    nvs_close(handle);
    return err;
}

esp_err_t wifi_set_last_ap(int index)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("recent_aps", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %d", err);
        return err;
    }
    err = nvs_set_i8(handle, "last", (int8_t)index);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}

#endif
